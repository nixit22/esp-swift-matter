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

import NVS
import Platform

private let log = Logger(tag: "MatterFactoryData")

/// Helpers for populating the `chip-factory` NVS namespace that esp_matter's
/// `ESP32Config`/`GenericDeviceInstanceInfoProvider` reads at commissioning time.
///
/// The namespace and key names used here are fixed by connectedhomeip
/// (`ESP32Config.cpp`) and must never change — they are read by the vendored
/// CHIP stack, not by this library.
public enum MatterFactoryData {
    private static let namespace = "chip-factory"

    /// Ensures `vendor-name`, `product-name`, `hw-ver-str`, and `serial-num` exist in
    /// the `chip-factory` NVS namespace, writing any that are missing. Never overwrites
    /// an already-provisioned value — safe to call on every boot.
    ///
    /// Call once at boot, after `nvs_flash_init()` and before `MatterDevice()`/`matter.run()`.
    ///
    /// - Parameters:
    ///   - vendorName: Human-readable vendor name (`vendor-name` key).
    ///   - productName: Human-readable product name (`product-name` key).
    ///   - hwVersion: Human-readable hardware version string (`hw-ver-str` key).
    ///   - serialNumber: Serial number to persist under `serial-num`. See
    ///     ``macSerial(prefix:)`` for a MAC-derived default.
    ///
    /// Never throws — a factory-data write failure on an already-provisioned device
    /// must never block boot; failures are logged and skipped.
    public static func initialize(
        vendorName: String, productName: String, hwVersion: String,
        serialNumber: String
    ) {
        guard let handle = try? NVS(namespace: namespace) else { return }

        func setIfMissing(_ key: String, _ value: String) -> Bool {
            (try? handle.setStringIfMissing(key, value)) ?? false
        }

        var dirty = false
        dirty = setIfMissing("vendor-name", vendorName) || dirty
        dirty = setIfMissing("product-name", productName) || dirty
        dirty = setIfMissing("hw-ver-str", hwVersion) || dirty
        dirty = setIfMissing("serial-num", serialNumber) || dirty

        if dirty {
            try? handle.commit()
            log.i("chip-factory initialized (vendor=\(vendorName), product=\(productName), hw=\(hwVersion), serial=\(serialNumber))")
        }
    }

    /// Builds `"\(prefix)-XXXXXXXX"` from the last 4 bytes of the device's 802.15.4 MAC
    /// address, hex-encoded, upper-case. Returns `"\(prefix)-00000000"` if the MAC can't
    /// be read (e.g. IEEE802154 disabled).
    public static func macSerial(prefix: String) -> String {
        var mac = [UInt8](repeating: 0, count: 8)
        let err = esp_read_mac(&mac, ESP_MAC_IEEE802154)
        guard err == ESP_OK else { return "\(prefix)-00000000" }
        return "\(prefix)-" + hex2(mac[4]) + hex2(mac[5]) + hex2(mac[6]) + hex2(mac[7])
    }

    private static let hexDigits: [Character] = [
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F",
    ]
    private static func hex2(_ byte: UInt8) -> String {
        "\(hexDigits[Int(byte >> 4)])\(hexDigits[Int(byte & 0xF)])"
    }
}
