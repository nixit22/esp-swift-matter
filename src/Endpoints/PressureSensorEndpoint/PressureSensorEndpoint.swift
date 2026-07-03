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
    /// Creates a PressureMeasurement endpoint (Matter device type 0x0305).
    /// Values are direct-cast Float→Int16 in hPa (Matter spec unit 0.1 kPa = 1 hPa).
    /// Aborts if endpoint creation fails.
    init(
        minPressure: Float, maxPressure: Float,
        willUpdateAttribute:
            @escaping (_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
                       _ value: _esp_matter_attr_val_t) -> esp_err_t,
        didUpdateAttribute:
            ((_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
              _ value: _esp_matter_attr_val_t) -> esp_err_t)? = nil
    ) {
        guard let ep = esp_matter_endpoint_pressure_sensor_create(
            minMeasuredValue: Int16(minPressure), maxMeasuredValue: Int16(maxPressure), privData: nil)
        else {
            log.e("Failed to create Pressure Sensor endpoint")
            abort()
        }
        self.init(
            id: esp_matter_endpoint_get_id(ep),
            willUpdateAttribute: willUpdateAttribute,
            didUpdateAttribute: didUpdateAttribute)
    }
}

/// A Matter pressure sensor endpoint (device type 0x0305, PressureMeasurement cluster).
///
/// Creates one Matter endpoint on the active ``MatterDevice`` node and registers it for
/// attribute-change callbacks. `set(_:)` is non-mutating, so the sensor can be stored as `let`.
///
/// ```swift
/// let matter = MatterDevice()
/// let pres = PressureSensorEndpoint(matter, min: 300, max: 1100)
/// matter.run()
/// pres.set(1013.0)
/// ```
public struct PressureSensorEndpoint {
    private let endpoint: MatterDevice.Endpoint

    /// Creates the pressure sensor endpoint and registers it with `matter`.
    ///
    /// - Parameters:
    ///   - matter: The active ``MatterDevice`` node. Must be created before any sensor endpoint.
    ///   - min: Minimum measurable pressure in hPa (default 300).
    ///   - max: Maximum measurable pressure in hPa (default 1100).
    ///   - didUpdateAttribute: Called after a Matter controller writes the MeasuredValue attribute.
    ///     Receives the raw endpoint ID, cluster ID, attribute ID, and new value.
    public init(
        _ matter: MatterDevice,
        min: Float = 300.0,
        max: Float = 1100.0,
        didUpdateAttribute: ((_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
                              _ value: _esp_matter_attr_val_t) -> esp_err_t)? = nil
    ) {
        endpoint = MatterDevice.Endpoint(
            minPressure: min, maxPressure: max,
            willUpdateAttribute: { endpointId, _, _, value in
                log.d("Endpoint \(endpointId) pressure will update \(Double(value.val.i16)) hPa")
                return ESP_OK
            },
            didUpdateAttribute: didUpdateAttribute)
        matter.register(endpoint: endpoint)
    }

    /// Writes `pressure` to the PressureMeasurement/MeasuredValue attribute.
    ///
    /// Pass `nil` to set the attribute to the Matter null sentinel (value not available).
    /// The value is direct-cast to Int16 in hPa (Matter spec unit 0.1 kPa = 1 hPa;
    /// representable range −32767…32767 hPa — far beyond sea-level and high-altitude use).
    public func set(_ pressure: Float?) {
        var value = _esp_matter_attr_val_t(pressure.map { Int16($0) })
        ESP_ERROR_CHECK(
            endpoint.update(
                clusterId: esp_matter_pressure_measurement_cluster_id,
                attributeId: esp_matter_pressure_measurement_measured_value_attribute_id,
                value: &value))
    }
}
