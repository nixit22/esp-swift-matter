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

private let log = Logger(tag: "Matter")

/// Implemented by the client app's enum to describe Mode Select options.
///
/// `RawValue` maps to the Matter `CurrentMode` attribute (`UInt8`).
/// `allModes` is iterated at endpoint creation time to register every selectable option.
///
/// ```swift
/// enum ScheduleOffset: UInt8, ModeSelectMode {
///     case minus30 = 0, minus15 = 1, atTime = 2, plus15 = 3, plus30 = 4
///
///     var label: String {
///         switch self {
///         case .minus30: return "-30 min"
///         case .minus15: return "-15 min"
///         case .atTime:  return "At time"
///         case .plus15:  return "+15 min"
///         case .plus30:  return "+30 min"
///         }
///     }
///
///     static let allModes: [ScheduleOffset] = [.minus30, .minus15, .atTime, .plus15, .plus30]
/// }
/// ```
public protocol ModeSelectMode: RawRepresentable where RawValue == UInt8 {
    /// Human-readable label shown in Matter controllers (32 chars max, UTF-8).
    var label: String { get }
    /// All selectable options, in display order.
    static var allModes: [Self] { get }
}

extension MatterDevice.Endpoint {
    /// Creates a Mode Select endpoint (cluster 0x0050, device type 0x0050).
    /// `onChange` fires with the raw `UInt8` mode value when a controller sends `ChangeToMode`.
    /// Aborts if endpoint creation fails.
    init(
        description: String,
        initialMode: UInt8,
        modes: [(value: UInt8, label: String)],
        onChange: @escaping (_ endpointId: UInt16, _ mode: UInt8) -> Void
    ) {
        guard let ep = description.withCString({ desc in
            esp_matter_endpoint_mode_select_create(
                description: desc,
                initialMode: initialMode,
                privData: nil)
        }) else {
            log.e("Failed to create mode select endpoint")
            abort()
        }
        for mode in modes {
            mode.label.withCString { cLabel in
                esp_matter_mode_select_add_mode(endpoint: ep, mode: mode.value, label: cLabel)
            }
        }
        self.init(
            id: esp_matter_endpoint_get_id(ep),
            willUpdateAttribute: nil,
            didUpdateAttribute: { eid, clusterId, attrId, val in
                if clusterId == esp_matter_mode_select_cluster_id &&
                   attrId == esp_matter_mode_select_current_mode_attribute_id {
                    onChange(eid, esp_matter_attr_val_get_uint8(val))
                }
                return ESP_OK
            }
        )
    }
}

/// A generic Mode Select endpoint (cluster 0x0050, device type 0x0050).
///
/// `Mode` is the client-defined enum conforming to ``ModeSelectMode``. The
/// `onChange` callback delivers a typed `Mode` value; `setCurrentMode` writes
/// one back to the Matter attribute store.
///
/// In Home Assistant this appears as a `select` entity.
///
/// ```swift
/// let dawn = ModeSelectEndpoint<ScheduleOffset>(
///     matter,
///     description: "Dawn Offset",
///     initialMode: .atTime
/// ) { _, offset in
///     store.dawnOffset = offset   // typed ScheduleOffset, not UInt8
/// }
/// matter.run()
/// // Restore from NVS after run():
/// dawn.setCurrentMode(store.dawnOffset)
/// ```
public struct ModeSelectEndpoint<Mode: ModeSelectMode> {
    private let endpoint: MatterDevice.Endpoint

    /// Creates a Mode Select endpoint and registers it with `matter`.
    ///
    /// - Parameters:
    ///   - matter: Active ``MatterDevice`` node. Must be created before endpoints.
    ///   - description: Label shown in Matter controllers (64 chars max).
    ///   - initialMode: Initial `CurrentMode`; must be a member of `Mode.allModes`.
    ///   - onChange: Called from the CHIP event loop when a controller sends `ChangeToMode`.
    ///     Receives the endpoint ID and the typed mode value. Ignored if the raw value has
    ///     no matching `Mode` case.
    public init(
        _ matter: MatterDevice,
        description: String,
        initialMode: Mode,
        onChange: @escaping (_ endpointId: UInt16, _ mode: Mode) -> Void
    ) {
        endpoint = MatterDevice.Endpoint(
            description: description,
            initialMode: initialMode.rawValue,
            modes: Mode.allModes.map { (value: $0.rawValue, label: $0.label) },
            onChange: { eid, raw in
                if let mode = Mode(rawValue: raw) {
                    onChange(eid, mode)
                }
            }
        )
        matter.register(endpoint: endpoint)
    }

    /// Write the current mode to the Matter attribute store.
    ///
    /// Use this to restore a previously stored mode from NVS after boot.
    /// Call after ``MatterDevice/run(onEvent:)``.
    public func setCurrentMode(_ mode: Mode) {
        var val = esp_matter_attr_val_uint8(mode.rawValue)
        ESP_ERROR_CHECK(
            endpoint.update(
                clusterId: esp_matter_mode_select_cluster_id,
                attributeId: esp_matter_mode_select_current_mode_attribute_id,
                value: &val))
    }
}
