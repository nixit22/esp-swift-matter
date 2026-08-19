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
 *  Uses connectedhomeip's stock DefaultTimeSyncDelegate unmodified — its
 *  UpdateTimeFromPlatformSource() already succeeds as soon as the system clock is set (by
 *  esp_matter_time_synchronization_start_sntp() or any other means), which is the standard
 *  esp_matter/connectedhomeip pattern: SNTP runs independently of Matter, and the delegate
 *  just notices. Also enables the Trusted-Time-Source client feature, so the cluster responds
 *  correctly if/when a controller sends SetTrustedTimeSource — but nothing here proactively
 *  retries or nudges that path; see matter-time-test/TIME-SYNC.md for why that turned out not
 *  to be worth chasing (the controller-resync gap it works around is a known, widely-reported
 *  ecosystem issue that's being fixed in controllers, e.g. matter.js's TimeSyncManager and a
 *  Home Assistant custom component — not something devices are expected to work around).
 *
 *  Does not advertise the TimeZone (TZ) feature or expose local/wall-clock time — that's a
 *  Matter-controller-supplied UTC offset for display purposes only, unrelated to a device's
 *  actual geographic location. If you need real local solar time (e.g. sunrise/sunset), use
 *  plain UTC (time()/gettimeofday(), synced via esp_matter_time_synchronization_start_sntp())
 *  plus the device's own known latitude/longitude — Matter has no cluster for the latter.
 */
SWIFT_NAME("esp_matter_enable_time_synchronization()")
esp_matter_endpoint_t *
esp_matter_enable_time_synchronization(void);

/** Starts SNTP against `host`, independent of the Matter TimeSynchronization cluster — this
 *  is the standard esp_matter/connectedhomeip pattern (see e.g. connectedhomeip's
 *  examples/platform/esp32/time/TimeSync.cpp), not something layered on top of the cluster.
 *  Runs indefinitely, re-syncing periodically and calling settimeofday() on each sync.
 *
 *  `host` must be IPv6-reachable (e.g. "time.google.com", "2.pool.ntp.org") on Thread-only
 *  networks — many pool.ntp.org entries are IPv4-only and fail silently.
 *
 *  Call once, any time — does not require esp_matter_enable_time_synchronization() or
 *  esp_matter_start() to have run first.
 */
SWIFT_NAME("esp_matter_time_synchronization_start_sntp(host:)")
void
esp_matter_time_synchronization_start_sntp(const char *host);

#ifdef __cplusplus
}
#endif
