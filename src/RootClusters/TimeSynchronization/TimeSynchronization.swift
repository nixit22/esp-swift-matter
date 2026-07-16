// Copyright (c) 2026 Nicolas Christe
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

import ESP_Matter
import Foundation

extension MatterDevice {
    /// Enables the Time Synchronization cluster (0x0038) on the root endpoint.
    ///
    /// Uses connectedhomeip's default delegate and Trusted-Time-Source client feature —
    /// once commissioned, if the controller registers itself (or another node) as a Trusted
    /// Time Source, the device fetches UTC time over a CASE session and calls
    /// `settimeofday()` internally. No custom delegate or SNTP code needed.
    ///
    /// Call after `MatterDevice()` init and before `run()`.
    public func enableTimeSynchronization() {
        _ = esp_matter_enable_time_synchronization()
    }

    /// Current local wall-clock time, combining the synced UTC clock with the
    /// TimeZone/DSTOffset a commissioner wrote via the TZ feature (enabled by
    /// `enableTimeSynchronization()`). Returns `nil` until both the clock is synced and a
    /// controller has written a timezone.
    ///
    /// The returned `Date`'s `timeIntervalSince1970` is UTC epoch seconds already shifted by
    /// that offset, not a real UTC instant — callers formatting it should account for
    /// `Date.description`'s hardcoded `" +0000"` suffix being misleading here.
    public func localDate() -> Date? {
        let t = esp_matter_time_synchronization_get_local_unix_time()
        return t == Int64.min ? nil : Date(timeIntervalSince1970: TimeInterval(t))
    }

    /// Re-attempts the Time Synchronization cluster's time fetch.
    ///
    /// connectedhomeip only calls this once, on boot (plus once more if a controller resends
    /// `SetTrustedTimeSource`). If that single attempt's CASE session to the trusted time source
    /// node times out — e.g. the Thread mesh hasn't settled yet right after a reboot — there's no
    /// built-in retry, and the device stays unsynced until the next reboot. Callers should poll
    /// this periodically (e.g. from a status-logging loop) while `localDate()`/the system
    /// clock stays unsynced.
    public func retryTimeSync() {
        esp_matter_time_synchronization_retry_sync()
    }

    /// Seeds the Time Synchronization cluster's DefaultNTP attribute with `host`, but only if
    /// nothing has been stored yet (e.g. by a controller's own `SetDefaultNTP` command).
    ///
    /// Without this, the NTP fallback path is never reached at all: connectedhomeip's
    /// `AttemptToGetFallbackNTPTimeFromDelegate()` bails out before calling the delegate if no
    /// `DefaultNTP` value is stored, and no controller observed in practice (Apple Home, Home
    /// Assistant) ever sends `SetDefaultNTP`.
    ///
    /// `host` must be IPv6-reachable (e.g. `"time.google.com"`, `"2.pool.ntp.org"`) — Thread is
    /// an IPv6-only transport, and many `pool.ntp.org` entries are IPv4-only and fail silently.
    ///
    /// Call after `run()`. `TimeSynchronizationServer`'s persistent-storage pointer, which this
    /// reads/writes through, is only set up by `MatterTimeSynchronizationPluginServerInitCallback`,
    /// fired synchronously from `esp_matter_start()` inside `run()` — calling this any earlier
    /// null-derefs in `TimeSyncDataProvider::Load`.
    public func setDefaultNTP(_ host: String) {
        host.withCString { esp_matter_time_synchronization_set_default_ntp(host: $0) }
    }
}
