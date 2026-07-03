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
    /// Creates a TemperatureMeasurement endpoint (Matter device type 0x0302).
    /// Values are scaled ×100 → Int16 (0.01 °C resolution). Aborts if endpoint creation fails.
    init(
        minTemperature: Float, maxTemperature: Float,
        willUpdateAttribute:
            @escaping (_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
                       _ value: _esp_matter_attr_val_t) -> esp_err_t,
        didUpdateAttribute:
            ((_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
              _ value: _esp_matter_attr_val_t) -> esp_err_t)? = nil
    ) {
        guard let ep = esp_matter_endpoint_temperature_sensor_create(
            minMeasuredValue: Int16(minTemperature * 100), maxMeasuredValue: Int16(maxTemperature * 100), privData: nil)
        else {
            log.e("Failed to create Temperature Sensor endpoint")
            abort()
        }
        self.init(
            id: esp_matter_endpoint_get_id(ep),
            willUpdateAttribute: willUpdateAttribute,
            didUpdateAttribute: didUpdateAttribute)
    }
}

/// A Matter temperature sensor endpoint (device type 0x0302, TemperatureMeasurement cluster).
///
/// Creates one Matter endpoint on the active ``MatterDevice`` node and registers it for
/// attribute-change callbacks. `set(_:)` is non-mutating, so the sensor can be stored as `let`.
///
/// ```swift
/// let matter = MatterDevice()
/// let temp = TemperatureSensorEndpoint(matter, min: -40, max: 85)
/// matter.run()
/// temp.set(23.5)
/// ```
public struct TemperatureSensorEndpoint {
    private let endpoint: MatterDevice.Endpoint

    /// Creates the temperature sensor endpoint and registers it with `matter`.
    ///
    /// - Parameters:
    ///   - matter: The active ``MatterDevice`` node. Must be created before any sensor endpoint.
    ///   - min: Minimum measurable temperature in °C (default −55).
    ///   - max: Maximum measurable temperature in °C (default 125).
    ///   - didUpdateAttribute: Called after a Matter controller writes the MeasuredValue attribute.
    ///     Receives the raw endpoint ID, cluster ID, attribute ID, and new value.
    public init(
        _ matter: MatterDevice,
        min: Float = -55.0,
        max: Float = 125.0,
        didUpdateAttribute: ((_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
                              _ value: _esp_matter_attr_val_t) -> esp_err_t)? = nil
    ) {
        endpoint = MatterDevice.Endpoint(
            minTemperature: min, maxTemperature: max,
            willUpdateAttribute: { endpointId, _, _, value in
                log.d("Endpoint \(endpointId) temperature will update \(Double(value.val.i16) / 100.0)°C")
                return ESP_OK
            },
            didUpdateAttribute: didUpdateAttribute)
        matter.register(endpoint: endpoint)
    }

    /// Writes `temperature` to the TemperatureMeasurement/MeasuredValue attribute.
    ///
    /// Pass `nil` to set the attribute to the Matter null sentinel (value not available).
    /// The value is scaled ×100 before writing (resolution 0.01 °C).
    public func set(_ temperature: Float?) {
        var value = _esp_matter_attr_val_t(temperature.map { Int16($0 * 100) })
        ESP_ERROR_CHECK(
            endpoint.update(
                clusterId: esp_matter_temperature_measurement_cluster_id,
                attributeId: esp_matter_temperature_measurement_measured_value_attribute_id,
                value: &value))
    }
}
