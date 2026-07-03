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
    /// Creates a PowerSource endpoint (Matter device type 0x0011) with Battery feature.
    /// BatPercentRemaining (0–200) and BatVoltage (mV) optional attributes are pre-added.
    /// Aborts if endpoint creation fails.
    init(
        order: UInt8,
        willUpdateAttribute:
            @escaping (_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
                       _ value: _esp_matter_attr_val_t) -> esp_err_t,
        didUpdateAttribute:
            ((_ endpointId: UInt16, _ clusterId: UInt32, _ attributeId: UInt32,
              _ value: _esp_matter_attr_val_t) -> esp_err_t)? = nil
    ) {
        guard let ep = esp_matter_endpoint_power_source_create(order: order, privData: nil) else {
            log.e("Failed to create PowerSource endpoint")
            abort()
        }
        self.init(
            id: esp_matter_endpoint_get_id(ep),
            willUpdateAttribute: willUpdateAttribute,
            didUpdateAttribute: didUpdateAttribute)
    }
}

/// A Matter PowerSource endpoint (device type 0x0011, PowerSource cluster with Battery feature).
///
/// Reports battery state-of-charge and voltage to Matter controllers.
/// `set(percent:voltageMv:)` is non-mutating, so the endpoint can be stored as `let`.
///
/// ```swift
/// let matter = MatterDevice()
/// let battery = PowerSourceEndpoint(matter)
/// matter.run()
/// battery.set(percent: 85, voltageMv: 4050)
/// ```
public struct PowerSourceEndpoint {
    private let endpoint: MatterDevice.Endpoint

    /// Creates the PowerSource endpoint and registers it with `matter`.
    ///
    /// - Parameters:
    ///   - matter: The active ``MatterDevice`` node. Must be created before any endpoints.
    ///   - order: Priority order among power sources (0 = highest, default 1).
    public init(_ matter: MatterDevice, order: UInt8 = 1) {
        endpoint = MatterDevice.Endpoint(
            order: order,
            willUpdateAttribute: { endpointId, _, _, _ in ESP_OK })
        matter.register(endpoint: endpoint)
    }

    /// Writes BatPercentRemaining and BatVoltage to the PowerSource cluster.
    ///
    /// `percent` is in 0–100 % and is scaled ×2 before writing (Matter uses 0–200, half-percent
    /// resolution). Pass `nil` for either argument to set that attribute to the Matter null sentinel.
    public func set(percent: UInt8?, voltageMv: UInt32? = nil) {
        var pctVal = _esp_matter_attr_val_t(percent.map { UInt8(min(UInt16($0) * 2, 200)) })
        ESP_ERROR_CHECK(
            endpoint.update(
                clusterId: esp_matter_power_source_cluster_id,
                attributeId: esp_matter_power_source_bat_percent_remaining_attribute_id,
                value: &pctVal))

        var mvVal = _esp_matter_attr_val_t(voltageMv)
        ESP_ERROR_CHECK(
            endpoint.update(
                clusterId: esp_matter_power_source_cluster_id,
                attributeId: esp_matter_power_source_bat_voltage_attribute_id,
                value: &mvVal))
    }
}
