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
#include <app/clusters/time-synchronization-server/DefaultTimeSyncDelegate.h>

#include <esp_netif_sntp.h>
#include <esp_log.h>

#include <cstring>
#include <algorithm>

namespace {

constexpr char kTag[] = "MatterTimeSync";
constexpr size_t kMaxHostSize = 128;

chip::app::Clusters::TimeSynchronization::DefaultTimeSyncDelegate gDelegate;

char gSntpHost[kMaxHostSize] = {};

// Runs on lwIP's SNTP task, not the CHIP event-loop thread.
void SntpSyncCallback(struct timeval *)
{
    ESP_LOGI(kTag, "SNTP synchronized ('%s')", gSntpHost);
}

} // namespace

extern "C" esp_matter_endpoint_t *esp_matter_enable_time_synchronization(void)
{
    esp_matter::endpoint_t *root = esp_matter::endpoint::get(0);
    esp_matter::cluster::time_synchronization::config_t cfg;
    cfg.delegate = &gDelegate;
    esp_matter::cluster::time_synchronization::create(root, &cfg, esp_matter::CLUSTER_FLAG_SERVER);
    return root;
}

extern "C" void esp_matter_time_synchronization_start_sntp(const char *host)
{
    size_t len = std::min(strlen(host), sizeof(gSntpHost) - 1);
    memcpy(gSntpHost, host, len);
    gSntpHost[len] = '\0';

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(gSntpHost);
    config.sync_cb = SntpSyncCallback;
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "esp_netif_sntp_init('%s') failed: %s", gSntpHost, esp_err_to_name(err));
    }
}
