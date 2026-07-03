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
    /// Creates a RelativeHumidityMeasurement endpoint (Matter device type 0x0307).
    /// Values are scaled ×100 → UInt16 (0.01 %RH resolution). Aborts if endpoint creation fails.
    init(
        minHumidity: Float, maxHumidity: Float,
        willUpdateAttribute:
            @escaping (_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
                       _ value: _esp_matter_attr_val_t) -> esp_err_t,
        didUpdateAttribute:
            ((_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
              _ value: _esp_matter_attr_val_t) -> esp_err_t)? = nil
    ) {
        guard let ep = esp_matter_endpoint_humidity_sensor_create(
            minMeasuredValue: UInt16(minHumidity * 100), maxMeasuredValue: UInt16(maxHumidity * 100), privData: nil)
        else {
            log.e("Failed to create Humidity Sensor endpoint")
            abort()
        }
        self.init(
            id: esp_matter_endpoint_get_id(ep),
            willUpdateAttribute: willUpdateAttribute,
            didUpdateAttribute: didUpdateAttribute)
    }
}

/// A Matter humidity sensor endpoint (device type 0x0307, RelativeHumidityMeasurement cluster).
///
/// Creates one Matter endpoint on the active ``MatterDevice`` node and registers it for
/// attribute-change callbacks. `set(_:)` is non-mutating, so the sensor can be stored as `let`.
///
/// ```swift
/// let matter = MatterDevice()
/// let hum = HumiditySensorEndpoint(matter, min: 0, max: 100)
/// matter.run()
/// hum.set(65.0)
/// ```
public struct HumiditySensorEndpoint {
    private let endpoint: MatterDevice.Endpoint

    /// Creates the humidity sensor endpoint and registers it with `matter`.
    ///
    /// - Parameters:
    ///   - matter: The active ``MatterDevice`` node. Must be created before any sensor endpoint.
    ///   - min: Minimum measurable relative humidity in %RH (default 0).
    ///   - max: Maximum measurable relative humidity in %RH (default 100).
    ///   - didUpdateAttribute: Called after a Matter controller writes the MeasuredValue attribute.
    ///     Receives the raw endpoint ID, cluster ID, attribute ID, and new value.
    public init(
        _ matter: MatterDevice,
        min: Float = 0.0,
        max: Float = 100.0,
        didUpdateAttribute: ((_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
                              _ value: _esp_matter_attr_val_t) -> esp_err_t)? = nil
    ) {
        endpoint = MatterDevice.Endpoint(
            minHumidity: min, maxHumidity: max,
            willUpdateAttribute: { endpointId, _, _, value in
                log.d("Endpoint \(endpointId) humidity will update \(Double(value.val.u16) / 100.0)%")
                return ESP_OK
            },
            didUpdateAttribute: didUpdateAttribute)
        matter.register(endpoint: endpoint)
    }

    /// Writes `humidity` to the RelativeHumidityMeasurement/MeasuredValue attribute.
    ///
    /// Pass `nil` to set the attribute to the Matter null sentinel (value not available).
    /// The value is scaled ×100 before writing (resolution 0.01 %RH).
    public func set(_ humidity: Float?) {
        var value = _esp_matter_attr_val_t(humidity.map { UInt16($0 * 100) })
        ESP_ERROR_CHECK(
            endpoint.update(
                clusterId: esp_matter_relative_humidity_measurement_cluster_id,
                attributeId: esp_matter_relative_humidity_measurement_measured_value_attribute_id,
                value: &value))
    }
}
