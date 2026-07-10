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

/* C facade for the Time Synchronization cluster (0x0038) on the root endpoint. */

#pragma once

#include "matter_core.h"
#include <stdint.h>
#include <swift_support.h>

#ifdef __cplusplus
extern "C" {
#endif

/** C wrapper for esp_matter::cluster::time_synchronization::create on the root endpoint.
 *  Uses the default (null) delegate — falls back to connectedhomeip's DefaultTimeSyncDelegate,
 *  which enables the Trusted-Time-Source client feature automatically. Also advertises the
 *  TimeZone (TZ) feature, so a commissioner writes its own known local timezone into the
 *  device during commissioning.
 */
SWIFT_NAME("esp_matter_enable_time_synchronization()")
esp_matter_endpoint_t *
esp_matter_enable_time_synchronization(void);

/** Current local wall-clock time as Unix epoch seconds (UTC time + the TimeZone/DSTOffset
 *  the commissioner wrote via the TZ feature). Returns INT64_MIN if no controller has
 *  written a timezone yet (or the clock itself isn't synced).
 */
SWIFT_NAME("esp_matter_time_synchronization_get_local_unix_time()")
int64_t
esp_matter_time_synchronization_get_local_unix_time(void);

/** Re-attempts the Time Synchronization cluster's time fetch (platform source, then trusted
 *  time source node, then NTP fallback — see TimeSynchronizationServer::AttemptToGetTime).
 *
 *  connectedhomeip only calls AttemptToGetTime() once, on the kServerReady boot event (plus
 *  once more if a controller resends SetTrustedTimeSource). If that single attempt's CASE
 *  session to the trusted node times out — e.g. the Thread mesh hasn't settled yet right after
 *  a reboot — there is no built-in retry, and the device is stuck unsynced until next reboot.
 *  Callers should poll this periodically (e.g. from a status-logging loop) while unsynced.
 */
SWIFT_NAME("esp_matter_time_synchronization_retry_sync()")
void
esp_matter_time_synchronization_retry_sync(void);

/** Seeds the cluster's DefaultNTP attribute with `host` if — and only if — nothing has been
 *  stored yet (e.g. by a controller's own SetDefaultNTP command). This exists because
 *  TimeSynchronizationServer::AttemptToGetFallbackNTPTimeFromDelegate() bails out before ever
 *  calling the delegate's NTP fallback if no DefaultNTP value is stored — and no controller
 *  observed in practice (Apple Home, Home Assistant) ever sends SetDefaultNTP. Without this
 *  call, the NTP fallback delegate is never reached at all, regardless of what it implements.
 *
 *  `host` must be IPv6-reachable (e.g. "time.google.com", "2.pool.ntp.org") — Thread is an
 *  IPv6-only transport, and many pool.ntp.org entries are IPv4-only and fail silently.
 *
 *  Call after esp_matter_enable_time_synchronization() and before esp_matter::start().
 */
SWIFT_NAME("esp_matter_time_synchronization_set_default_ntp(host:)")
void
esp_matter_time_synchronization_set_default_ntp(const char *host);

#ifdef __cplusplus
}
#endif
