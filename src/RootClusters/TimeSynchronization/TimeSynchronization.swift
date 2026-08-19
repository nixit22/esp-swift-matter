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

extension MatterDevice {
    /// Enables the Time Synchronization cluster (0x0038) on the root endpoint.
    ///
    /// Uses connectedhomeip's default delegate and Trusted-Time-Source client feature —
    /// once commissioned, if the controller registers itself (or another node) as a Trusted
    /// Time Source, the device fetches UTC time over a CASE session and calls
    /// `settimeofday()` internally. Also picks up whatever `startNTPSync(host:)` (or any
    /// other means) already put in the system clock — no custom delegate needed.
    ///
    /// Call after `MatterDevice()` init and before `run()`.
    public func enableTimeSynchronization() {
        _ = esp_matter_enable_time_synchronization()
    }

    /// Starts SNTP against `host`, independent of the Time Synchronization cluster — this is
    /// the standard esp_matter/connectedhomeip pattern (see e.g. connectedhomeip's
    /// `examples/platform/esp32/time/TimeSync.cpp`), not something layered on top of the
    /// cluster. Runs indefinitely, re-syncing periodically and calling `settimeofday()` on
    /// each sync.
    ///
    /// `host` must be IPv6-reachable (e.g. `"time.google.com"`, `"2.pool.ntp.org"`) on
    /// Thread-only networks — many `pool.ntp.org` entries are IPv4-only and fail silently.
    ///
    /// Call once, any time — does not require `enableTimeSynchronization()` or `run()` to
    /// have been called first.
    public func startNTPSync(host: String) {
        host.withCString { esp_matter_time_synchronization_start_sntp(host: $0) }
    }
}
