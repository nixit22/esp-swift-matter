/*
 * Copyright (c) 2026 Nicolas Christe
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "matter_time_synchronization.h"
#include <esp_matter.h>
#include <app/clusters/time-synchronization-server/time-synchronization-cluster.h>
#include <app/clusters/time-synchronization-server/DefaultTimeSyncDelegate.h>
#include <app/data-model/Nullable.h>
#include <lib/support/TimeUtils.h>
#include <platform/PlatformManager.h>

#include <esp_netif_sntp.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <lwip/netdb.h>
#include <lwip/inet.h>
#include <lwip/dns.h>

#include <atomic>
#include <cstring>
#include <algorithm>

namespace {

constexpr char kTag[] = "MatterTimeSync";
constexpr size_t kMaxHostSize = 128; // matches TimeSynchronizationServer::kMaxDefaultNTPSize (private)
constexpr TickType_t kSntpSyncTimeout = pdMS_TO_TICKS(15000);

using chip::app::Clusters::TimeSynchronization::DefaultTimeSyncDelegate;
using chip::app::Clusters::TimeSynchronization::OnFallbackNTPCompletion;

/* Real NTP fallback: DefaultTimeSyncDelegate's own UpdateTimeUsingNTPFallback is an
 * unimplemented stub (returns CHIP_ERROR_NOT_IMPLEMENTED — confirmed unchanged in
 * connectedhomeip upstream as of this writing). This subclass is the only override needed;
 * IsNTPAddressValid/IsNTPAddressDomain/UpdateTimeFromPlatformSource are inherited as-is. */
class SwiftTimeSyncDelegate : public DefaultTimeSyncDelegate {
public:
    CHIP_ERROR UpdateTimeUsingNTPFallback(const chip::CharSpan &fallbackNTP,
                                           chip::Callback::Callback<OnFallbackNTPCompletion> *callback) override
    {
        bool expected = false;
        if (!mQueryInFlight.compare_exchange_strong(expected, true)) {
            // retryTimeSync()'s 30s loop can re-enter this while a prior query is still
            // running (e.g. waiting on the SNTP timeout) — decline; the loop tries again later.
            return CHIP_ERROR_BUSY;
        }

        size_t len = std::min(fallbackNTP.size(), sizeof(mHost) - 1);
        memcpy(mHost, fallbackNTP.data(), len);
        mHost[len] = '\0';
        mCallback = callback;

        if (xTaskCreate(&SwiftTimeSyncDelegate::SntpQueryTask, "ntp_fallback", 4096, this,
                         tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
            mQueryInFlight.store(false);
            return CHIP_ERROR_NO_MEMORY;
        }
        return CHIP_NO_ERROR;
    }

private:
    // Blocking SNTP query — runs on its own task, off the CHIP event-loop thread (this method
    // is invoked either on that thread directly, or under ScopedChipStackLock via
    // esp_matter_time_synchronization_retry_sync(); either way, blocking here would stall the
    // whole Matter stack for up to kSntpSyncTimeout).
    static void SntpQueryTask(void *arg)
    {
        auto *self = static_cast<SwiftTimeSyncDelegate *>(arg);

        // Diagnostic only — disambiguates "DNS never resolves" from "resolved fine, no reply
        // to the actual NTP packet" before esp_netif_sntp_sync_wait collapses both into
        // ESP_ERR_TIMEOUT. Also dumps lwIP's configured DNS servers directly: on Thread,
        // nothing populates these automatically unless RDNSS/NAT64 discovery wired them up —
        // if all three are unset, that alone explains EAI_FAIL, independent of whether the
        // Border Router itself has WAN.
        for (u8_t i = 0; i < DNS_MAX_SERVERS; i++) {
            const ip_addr_t *server = dns_getserver(i);
            char ipStr[INET6_ADDRSTRLEN] = {};
            if (server != nullptr && !ip_addr_isany(server)) {
                ipaddr_ntoa_r(server, ipStr, sizeof(ipStr));
                ESP_LOGI(kTag, "lwIP DNS server[%u] = %s", i, ipStr);
            } else {
                ESP_LOGI(kTag, "lwIP DNS server[%u] = (unset)", i);
            }
        }

        struct addrinfo hints = {};
        hints.ai_family = AF_UNSPEC;
        struct addrinfo *res = nullptr;
        int gaiErr = lwip_getaddrinfo(self->mHost, nullptr, &hints, &res);
        if (gaiErr == 0 && res != nullptr) {
            char ipStr[INET6_ADDRSTRLEN] = {};
            void *addr = (res->ai_family == AF_INET6)
                ? (void *)&reinterpret_cast<struct sockaddr_in6 *>(res->ai_addr)->sin6_addr
                : (void *)&reinterpret_cast<struct sockaddr_in *>(res->ai_addr)->sin_addr;
            inet_ntop(res->ai_family, addr, ipStr, sizeof(ipStr));
            ESP_LOGI(kTag, "DNS resolved '%s' -> %s", self->mHost, ipStr);
            lwip_freeaddrinfo(res);
        } else {
            ESP_LOGW(kTag, "DNS resolution for '%s' failed: gai_err=%d", self->mHost, gaiErr);
        }

        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(self->mHost);
        bool success = false;
        esp_err_t initErr = esp_netif_sntp_init(&config);
        if (initErr == ESP_OK) {
            // esp_netif_sntp's own lwIP client calls settimeofday() internally on sync —
            // no separate SetClock_RealTime() call needed here.
            esp_err_t syncErr = esp_netif_sntp_sync_wait(kSntpSyncTimeout);
            success = syncErr == ESP_OK;
            esp_netif_sntp_deinit(); // singleton — must deinit before any later init call
            ESP_LOGI(kTag, "NTP fallback query to '%s' %s (sync_wait=%s)", self->mHost,
                     success ? "succeeded" : "failed", esp_err_to_name(syncErr));
        } else {
            ESP_LOGI(kTag, "NTP fallback query to '%s' failed (init=%s)", self->mHost,
                     esp_err_to_name(initErr));
        }

        self->mLastResult.store(success);
        chip::DeviceLayer::PlatformMgr().ScheduleWork(&SwiftTimeSyncDelegate::CompleteOnChipThread,
                                                       reinterpret_cast<intptr_t>(self));
        vTaskDelete(nullptr);
    }

    // Completion callbacks must fire on the CHIP event-loop thread, not the SNTP task.
    static void CompleteOnChipThread(intptr_t arg)
    {
        auto *self = reinterpret_cast<SwiftTimeSyncDelegate *>(arg);
        bool success = self->mLastResult.load();
        auto *callback = self->mCallback;
        self->mQueryInFlight.store(false);
        if (callback != nullptr) {
            callback->mCall(callback->mContext, success);
        }
    }

    char mHost[kMaxHostSize] = {};
    chip::Callback::Callback<OnFallbackNTPCompletion> *mCallback = nullptr;
    std::atomic<bool> mQueryInFlight{false};
    std::atomic<bool> mLastResult{false};
};

SwiftTimeSyncDelegate gTimeSyncDelegate;

} // namespace

extern "C" esp_matter_endpoint_t *esp_matter_enable_time_synchronization(void)
{
    esp_matter::endpoint_t *root = esp_matter::endpoint::get(0);
    esp_matter::cluster::time_synchronization::config_t cfg;
    cfg.delegate = &gTimeSyncDelegate; // real NTP fallback instead of DefaultTimeSyncDelegate's stub
    esp_matter::cluster_t *cluster =
        esp_matter::cluster::time_synchronization::create(root, &cfg, esp_matter::CLUSTER_FLAG_SERVER);

    /* Advertise the TimeZone (TZ) feature: this is how a UI-less Matter device learns
     * local time. With the feature bit set, commissioners (Apple Home, Google Home, Home
     * Assistant, ...) run the kConfigureTimeZone commissioning stage and write their own
     * known local timezone (derived from phone locale / hub server tz) into this device's
     * TimeZone attribute — no GPS or UI needed on our side. */
    esp_matter::cluster::time_synchronization::feature::time_zone::config_t tz_cfg; // time_zone_database = None
    esp_matter::cluster::time_synchronization::feature::time_zone::add(cluster, &tz_cfg);

    return root;
}

extern "C" void esp_matter_time_synchronization_set_default_ntp(const char *host)
{
    using namespace chip::app::Clusters::TimeSynchronization;

    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);

    auto &server = TimeSynchronizationServer::Instance();

    // Don't fight a value a controller may have already written via SetDefaultNTP.
    char existing[kMaxHostSize];
    chip::MutableCharSpan existingSpan(existing);
    if (server.GetDefaultNtp(existingSpan) == CHIP_NO_ERROR) {
        return;
    }

    chip::CharSpan hostSpan(host, strlen(host));
    server.SetDefaultNTP(chip::app::DataModel::Nullable<chip::CharSpan>(hostSpan));
}

extern "C" int64_t esp_matter_time_synchronization_get_local_unix_time(void)
{
    using namespace chip::app::Clusters::TimeSynchronization;

    chip::app::DataModel::Nullable<uint64_t> localTime;
    if (TimeSynchronizationServer::Instance().GetLocalTime(0, localTime) != CHIP_NO_ERROR || localTime.IsNull()) {
        return INT64_MIN;
    }
    uint64_t unixMicros;
    if (!chip::ChipEpochToUnixEpochMicros(localTime.Value(), unixMicros)) {
        return INT64_MIN;
    }
    return static_cast<int64_t>(unixMicros / chip::kMicrosecondsPerSecond);
}

extern "C" void esp_matter_time_synchronization_retry_sync(void)
{
    using namespace chip::app::Clusters::TimeSynchronization;

    // Called from the app's own task, not the CHIP event loop thread — every CHIP API
    // touched below (event logging, CASE session lookup, etc.) asserts the stack is locked.
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);

    // AttemptToGetTime() (the real retry entry point) is private — re-invoking
    // SetTrustedTimeSource() with its own current value re-enters the same path
    // (platform source, then trusted node, then NTP fallback) as the one-shot
    // boot-time attempt, since it's still ungranular.
    auto &server = TimeSynchronizationServer::Instance();
    server.SetTrustedTimeSource(server.GetTrustedTimeSource());
}
