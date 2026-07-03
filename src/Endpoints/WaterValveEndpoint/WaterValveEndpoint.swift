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

extension MatterDevice.Endpoint {
    /// Creates a WaterValve endpoint (Matter device type 0x0042).
    /// `onOpen` / `onClose` fire from the CHIP event loop when the controller sends
    /// Open / Close commands via the C++ delegate trampoline. Aborts if endpoint creation fails.
    init(
        onOpen:  @escaping (_ endpointId: UInt16) -> Void,
        onClose: @escaping (_ endpointId: UInt16) -> Void
    ) {
        let cb = CommandCallbacks { eid, cmd in
            if cmd == 0x00 { onOpen(eid) } else { onClose(eid) }
        }
        let ptr = Unmanaged.passRetained(cb).toOpaque()
        guard let ep = esp_matter_endpoint_water_valve_create(
            commandCallback: { eid, cmd, p in
                p.map { Unmanaged<CommandCallbacks>.fromOpaque($0).takeUnretainedValue().dispatch(eid, cmd) }
            },
            privData: ptr)
        else {
            Unmanaged<CommandCallbacks>.fromOpaque(ptr).release()
            log.e("Failed to create Water Valve endpoint")
            abort()
        }
        self.init(id: esp_matter_endpoint_get_id(ep), willUpdateAttribute: nil, didUpdateAttribute: nil)
    }
}

/// A Matter water valve endpoint (device type 0x0042, ValveConfigurationAndControl cluster).
///
/// Creates one Matter endpoint on the active ``MatterDevice`` node. Unlike sensor endpoints,
/// the valve is command-driven: the CHIP cluster server fires `onOpen` / `onClose` when a
/// controller sends the corresponding command. The firmware must then actuate the hardware
/// and call ``setCurrentState(_:)`` to report the result back to Matter.
///
/// ```swift
/// let matter = MatterDevice()
/// let valve = WaterValveEndpoint(matter,
///     onOpen:  { _ in /* open hardware */ valve.setCurrentState(.open)  },
///     onClose: { _ in /* close hardware */ valve.setCurrentState(.closed) })
/// matter.run()
/// ```
public struct WaterValveEndpoint {
    private let endpoint: MatterDevice.Endpoint

    /// Valve states for ``setCurrentState(_:)``.
    public enum State: UInt8 {
        /// Valve fully closed; flow stopped.
        case closed        = 0
        /// Valve fully open; flow active.
        case open          = 1
        /// Valve in motion between open and closed; set by the CHIP cluster server automatically
        /// when it processes an Open or Close command. Firmware should transition to `.open`
        /// or `.closed` once movement completes.
        case transitioning = 2
    }

    /// Creates the water valve endpoint and registers it with `matter`.
    ///
    /// - Parameters:
    ///   - matter: The active ``MatterDevice`` node. Must be created before any endpoint.
    ///   - onOpen: Called from the CHIP event loop when a controller sends the Open command.
    ///   - onClose: Called from the CHIP event loop when a controller sends the Close command.
    public init(
        _ matter: MatterDevice,
        onOpen:  @escaping (_ endpointId: UInt16) -> Void = { _ in },
        onClose: @escaping (_ endpointId: UInt16) -> Void = { _ in }
    ) {
        endpoint = MatterDevice.Endpoint(onOpen: onOpen, onClose: onClose)
        matter.register(endpoint: endpoint)
    }

    /// Writes `state` to the ValveConfigurationAndControl/CurrentState attribute.
    ///
    /// Call this after actuating the hardware to report the valve's actual position.
    /// The CHIP cluster server automatically sets `CurrentState` to `.transitioning`
    /// when it processes an Open or Close command; the firmware should set it to
    /// `.open` or `.closed` once movement completes.
    public func setCurrentState(_ state: State) {
        var value = esp_matter_attr_nullable_enum8(state.rawValue, isNull: false)
        ESP_ERROR_CHECK(
            endpoint.update(
                clusterId: esp_matter_valve_configuration_and_control_cluster_id,
                attributeId: esp_matter_valve_configuration_and_control_current_state_attribute_id,
                value: &value))
    }
}
