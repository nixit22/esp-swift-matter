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

/** Invoked whenever the Time Synchronization cluster's UTCTime attribute is set — the first
 *  successful sync (Trusted-Time-Source fetch, or a controller's SetUTCTime command) and any
 *  later correction. By the time this fires, System::SystemClock().SetClock_RealTime() has
 *  already run, so the callback carries no time value — read gettimeofday()/time() directly.
 *  (The cluster's internal value is Chip-epoch microseconds, not Unix epoch, so there's nothing
 *  useful to hand back without duplicating connectedhomeip's private epoch-conversion helper.)
 */
typedef void (*esp_matter_time_sync_callback_t)(void);

/** C wrapper for esp_matter::cluster::time_synchronization::create on the root endpoint.
 *  Uses connectedhomeip's stock DefaultTimeSyncDelegate, subclassed only to forward
 *  UTCTimeAvailabilityChanged() to on_time_sync (pass NULL if you don't need the notification).
 *  Otherwise unmodified — its UpdateTimeFromPlatformSource() already succeeds as soon as the
 *  system clock is set (by any means — e.g. the standalone esp-swift-sntp component, or the
 *  Trusted-Time-Source client feature below), which is the standard esp_matter/connectedhomeip
 *  pattern: clock sync runs independently of Matter, and the delegate just notices. Also enables
 *  the Trusted-Time-Source client feature, so the cluster responds correctly if/when a controller
 *  sends SetTrustedTimeSource — but nothing here proactively retries or nudges that path; see
 *  matter-time-test/TIME-SYNC.md for why that turned out not to be worth chasing (the
 *  controller-resync gap it works around is a known, widely-reported ecosystem issue that's
 *  being fixed in controllers, e.g. matter.js's TimeSyncManager and a Home Assistant custom
 *  component — not something devices are expected to work around).
 *
 *  Does not advertise the TimeZone (TZ) feature or expose local/wall-clock time — that's a
 *  Matter-controller-supplied UTC offset for display purposes only, unrelated to a device's
 *  actual geographic location. If you need real local solar time (e.g. sunrise/sunset), use
 *  plain UTC (time()/gettimeofday(), synced independently — e.g. via esp-swift-sntp) plus the
 *  device's own known latitude/longitude — Matter has no cluster for the latter.
 */
SWIFT_NAME("esp_matter_enable_time_synchronization(onTimeSync:)")
esp_matter_endpoint_t *
esp_matter_enable_time_synchronization(esp_matter_time_sync_callback_t on_time_sync);

#ifdef __cplusplus
}
#endif
