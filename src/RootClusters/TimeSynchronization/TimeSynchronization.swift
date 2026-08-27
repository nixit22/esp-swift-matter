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
import Platform

private let log = Logger(tag: "Matter.TimeSync")

extension MatterDevice {
    private static var onTimeSyncHandler: (() -> Void)?

    /// Enables the Time Synchronization cluster (0x0038) on the root endpoint.
    ///
    /// Uses connectedhomeip's default delegate and Trusted-Time-Source client feature —
    /// once commissioned, if the controller registers itself (or another node) as a Trusted
    /// Time Source, the device fetches UTC time over a CASE session and calls
    /// `settimeofday()` internally. Also picks up whatever set the system clock by any other
    /// means (e.g. the standalone `esp-swift-sntp` component) — no custom delegate needed.
    ///
    /// Call after `MatterDevice()` init and before `run()`.
    ///
    /// - Parameter onTimeSync: Called whenever the cluster's `UTCTime` attribute is set — the
    ///   first successful sync (Trusted-Time-Source fetch or a controller's `SetUTCTime`
    ///   command) and any later correction. The system clock is already updated by the time
    ///   this fires, so read `gettimeofday()`/`time()` yourself rather than expecting a value
    ///   here. Only fires for time obtained through this cluster — independent clock sync (e.g.
    ///   `esp-swift-sntp`) doesn't route through it. Registering a new `MatterDevice` replaces
    ///   any previous handler (matches the one-`MatterDevice`-per-device model).
    public func enableTimeSynchronization(onTimeSync: (() -> Void)? = nil) {
        Self.onTimeSyncHandler = onTimeSync
        let root = esp_matter_enable_time_synchronization(onTimeSync: { MatterDevice.onTimeSyncHandler?() })
        if root == nil {
            log.e("enableTimeSynchronization() called before MatterDevice() init — root endpoint doesn't exist")
            abort()
        }
    }
}
