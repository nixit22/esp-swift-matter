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

@_exported import ESP_Matter
import Platform

private let log = Logger(tag: "Matter")

/// Heap-allocated closure box shared by all delegate-pattern endpoints (valve, etc.).
///
/// Allocated with `Unmanaged.passRetained` and passed as `priv_data` to the C++ delegate
/// trampoline; the trampoline calls `dispatch(endpointId, commandId)`. Never freed — the
/// Matter node and its delegates live for the process lifetime.
final class CommandCallbacks {
    let dispatch: (_ endpointId: UInt16, _ commandId: UInt8) -> Void
    init(_ dispatch: @escaping (UInt16, UInt8) -> Void) { self.dispatch = dispatch }
}

/// Entry point for the ESP-Matter stack on an ESP32 device.
///
/// Create one instance before any sensor endpoints, then call ``run()`` to start the stack.
/// Only one `MatterDevice` instance may exist per device — creating a second replaces the singleton
/// and breaks callback dispatch for endpoints registered on the first.
///
/// ```swift
/// let matter = MatterDevice()
/// let temp = TemperatureSensorEndpoint(matter, min: -40, max: 85)
/// matter.run()
/// temp.set(23.5)
/// ```
public final class MatterDevice {

    /// Internal representation of a single Matter endpoint registered with the node.
    ///
    /// Sensor types (`TemperatureSensorEndpoint`, `HumiditySensorEndpoint`, `PressureSensorEndpoint`) create one
    /// `Endpoint` per instance and register it with the owning ``MatterDevice`` node so C attribute
    /// callbacks can dispatch to the right handler by endpoint ID.
    struct Endpoint {
        let id: UInt16
        let willUpdateAttribute:
            ((_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32, _ value: _esp_matter_attr_val_t) -> esp_err_t)?
        let didUpdateAttribute:
            ((_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32, _ value: _esp_matter_attr_val_t) -> esp_err_t)?

        /// Writes `value` to the Matter attribute identified by `clusterId` / `attributeId`.
        func update(clusterId: UInt32, attributeId: UInt32, value: inout _esp_matter_attr_val_t) -> esp_err_t {
            return esp_matter_attribute_update(endpointId: id, clusterId: clusterId, attributeId: attributeId, value: &value)
        }
    }

    /// Matter stack events delivered to the ``run(onEvent:)`` callback.
    public enum Event: UInt8 {
        /// Commissioning via the controller app succeeded; the device now has a fabric.
        case commissioningComplete       = 0
        /// A BLE pairing session was opened (QR code scanned).
        case commissioningSessionStarted = 1
        /// BLE pairing session closed — either succeeded or timed out.
        case commissioningSessionStopped = 2
        /// Commissioning window is open; the QR code / manual code is active.
        case commissioningWindowOpened   = 3
        /// Commissioning window closed.
        case commissioningWindowClosed   = 4
        /// Any CHIP event type not mapped to the cases above.
        case unknown                     = 0xFF
    }

    private var endpoints: [UInt16: Endpoint] = [:]
    private var onEvent: ((Event) -> Void)?

    /// Adds `endpoint` to the callback dispatch table, keyed by endpoint ID.
    func register(endpoint: Endpoint) {
        endpoints[endpoint.id] = endpoint
    }

    /// Creates the root Matter node and registers the node-level attribute callback.
    ///
    /// The node label is derived from the last two bytes of the device's 802.15.4 MAC address.
    /// Aborts if node creation fails (e.g. called before `esp_matter` is initialised).
    ///
    /// `self` is passed as `privData` to the C node callback so attribute callbacks can reach
    /// the endpoint registry without global state. The retain is intentional: the Matter node
    /// lives for the process lifetime and the callback fires until device reset.
    public init() {
        log.setLogLevel(ESP_LOG_DEBUG)
        let nodeLabel = "Device \(chipId4String())"
        let node = nodeLabel.withCString { cstr in
            esp_matter_create_node(
                nodeLabel: UnsafeMutablePointer(mutating: cstr),
                willUpdate: { endpoint_id, cluster_id, attribute_id, val, priv_data in
                    guard let val, let priv_data else { return ESP_OK }
                    let matter = Unmanaged<MatterDevice>.fromOpaque(priv_data).takeUnretainedValue()
                    guard let ep = matter.endpoints[endpoint_id] else { return ESP_OK }
                    return ep.willUpdateAttribute?(endpoint_id, cluster_id, attribute_id, val.pointee) ?? ESP_OK
                },
                didUpdate: { endpoint_id, cluster_id, attribute_id, val, priv_data in
                    guard let val, let priv_data else { return ESP_OK }
                    let matter = Unmanaged<MatterDevice>.fromOpaque(priv_data).takeUnretainedValue()
                    guard let ep = matter.endpoints[endpoint_id] else { return ESP_OK }
                    return ep.didUpdateAttribute?(endpoint_id, cluster_id, attribute_id, val.pointee) ?? ESP_OK
                },
                privData: Unmanaged.passRetained(self).toOpaque()
            )
        }
        if node == nil {
            log.e("Failed to create Matter node")
            abort()
        }
    }

    /// Starts the Matter stack and prints onboarding codes to the ESP-IDF log.
    ///
    /// Applies the default OpenThread platform configuration, starts the CHIP stack
    /// (blocks until initialized), prints the commissioning QR code and manual pairing
    /// code via ChipLogProgress, then registers `onEvent` for subsequent stack events.
    ///
    /// Call after all sensor endpoints have been created. The host project's sdkconfig
    /// must enable: BT (NimBLE), OpenThread, MBEDTLS_HKDF_C, and a custom partition
    /// table matching the esp_matter NVS layout.
    ///
    /// - Parameter onEvent: Optional callback invoked from the CHIP event loop thread
    ///   when commissioning-related events occur.
    public func run(onEvent: ((Event) -> Void)? = nil) {
        self.onEvent = onEvent
        ESP_ERROR_CHECK(set_default_openthread_platform_config())
        if onEvent != nil {
            ESP_ERROR_CHECK(
                esp_matter_start(
                    callback: { event, arg in
                        guard let arg else { return }
                        let matter = Unmanaged<MatterDevice>.fromOpaque(arg).takeUnretainedValue()
                        matter.onEvent?(Event(rawValue: event) ?? .unknown)
                    },
                    callbackArg: Unmanaged.passUnretained(self).toOpaque()
                )
            )
        } else {
            ESP_ERROR_CHECK(esp_matter_start(callback: nil, callbackArg: nil))
        }
        if !esp_matter_is_commissioned() {
            esp_matter_print_onboarding_codes()
        }
    }

    /// Erases all Matter/Thread NVS state and reboots the device.
    ///
    /// Only call after `run()` returns — the underlying `chip::Server::ScheduleFactoryReset()`
    /// call requires the CHIP event loop to be running. Does not return on success: the
    /// device restarts once the reset completes.
    public func factoryReset() {
        ESP_ERROR_CHECK(esp_matter_factory_reset())
    }
}

/// Returns the last two bytes of the device's 802.15.4 MAC address as four ASCII hex nibbles.
/// Aborts if the MAC is unavailable (i.e. NVS is not initialized or IEEE802154 is disabled).
func chipId4Bytes() -> [UInt8] {
    // Read 8-byte 802.15.4 MAC and fail if it's not available.
    let buf8 = UnsafeMutablePointer<UInt8>.allocate(capacity: 8)
    defer { buf8.deallocate() }
    ESP_ERROR_CHECK(esp_read_mac(buf8, ESP_MAC_IEEE802154))

    let b1 = buf8[6]
    let b2 = buf8[7]
    let hexArray: [UInt8] = Array("0123456789ABCDEF".utf8)

    func hexNibble(_ v: UInt8) -> UInt8 {
        hexArray[Int(v) & 0xF]
    }

    let bytes: [UInt8] = [
        hexNibble(b1 >> 4),
        hexNibble(b1 & 0x0F),
        hexNibble(b2 >> 4),
        hexNibble(b2 & 0x0F),
    ]
    return bytes
}

/// Returns a 4-character ASCII hex string derived from the last two bytes of the
/// device's 802.15.4 MAC address. Uses `String(decoding:as:)` to avoid the large
/// Unicode tables that are not available in the Embedded Swift runtime.
func chipId4String() -> String {
    let bytes = chipId4Bytes()
    return String(decoding: bytes, as: UTF8.self)
}

extension _esp_matter_attr_val_t {
    /// Builds a nullable signed 16-bit attribute value.
    /// Passing `nil` sets the Matter null sentinel (attribute value not available).
    init(_ value: Int16?) {
        self = esp_matter_attr_nullable_int16(value ?? 0, isNull: value == nil)
    }

    /// Builds a nullable unsigned 16-bit attribute value.
    /// Passing `nil` sets the Matter null sentinel (attribute value not available).
    init(_ value: UInt16?) {
        self = esp_matter_attr_nullable_uint16(value ?? 0, isNull: value == nil)
    }

    /// Builds a nullable unsigned 8-bit attribute value.
    /// Passing `nil` sets the Matter null sentinel (attribute value not available).
    init(_ value: UInt8?) {
        self = esp_matter_attr_nullable_uint8(value ?? 0, isNull: value == nil)
    }

    /// Builds a nullable unsigned 32-bit attribute value.
    /// Passing `nil` sets the Matter null sentinel (attribute value not available).
    init(_ value: UInt32?) {
        self = esp_matter_attr_nullable_uint32(value ?? 0, isNull: value == nil)
    }
}

