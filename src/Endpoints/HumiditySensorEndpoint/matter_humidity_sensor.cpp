/*
 * Copyright (c) 2026 Nicolas Christe
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "matter_humidity_sensor.h"
#include <esp_matter.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Attributes.h>

extern "C" const uint32_t esp_matter_relative_humidity_measurement_cluster_id =
    chip::app::Clusters::RelativeHumidityMeasurement::Id;
extern "C" const uint32_t esp_matter_relative_humidity_measurement_measured_value_attribute_id =
    chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Id;

extern "C" esp_matter_endpoint_t *esp_matter_endpoint_humidity_sensor_create(
    uint16_t min_measured_value, uint16_t max_measured_value, void *priv_data)
{
    esp_matter::endpoint::humidity_sensor::config_t cfg;
    cfg.relative_humidity_measurement.min_measured_value = nullable<uint16_t>(min_measured_value);
    cfg.relative_humidity_measurement.max_measured_value = nullable<uint16_t>(max_measured_value);
    return esp_matter::endpoint::humidity_sensor::create(
        esp_matter::node::get(),
        &cfg,
        esp_matter::ENDPOINT_FLAG_NONE,
        priv_data);
}
