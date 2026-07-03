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

#include "matter_power_source.h"
#include <cstring>
#include <esp_matter.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Attributes.h>

extern "C" const uint32_t esp_matter_power_source_cluster_id =
    chip::app::Clusters::PowerSource::Id;
extern "C" const uint32_t esp_matter_power_source_bat_percent_remaining_attribute_id =
    chip::app::Clusters::PowerSource::Attributes::BatPercentRemaining::Id;
extern "C" const uint32_t esp_matter_power_source_bat_voltage_attribute_id =
    chip::app::Clusters::PowerSource::Attributes::BatVoltage::Id;

extern "C" esp_matter_endpoint_t *
esp_matter_endpoint_power_source_create(uint8_t order, void *priv_data)
{
    esp_matter::endpoint::power_source::config_t cfg;
    cfg.power_source.order = order;
    cfg.power_source.status = 1;  /* PowerSourceStatusEnum::kActive */
    strncpy(cfg.power_source.description, "Battery",
            sizeof(cfg.power_source.description) - 1);
    cfg.power_source.feature_flags =
        esp_matter::cluster::power_source::feature::battery::get_id();
    cfg.power_source.features.battery.bat_charge_level = 0;  /* OK */

    auto *ep = esp_matter::endpoint::power_source::create(
        esp_matter::node::get(), &cfg, esp_matter::ENDPOINT_FLAG_NONE, priv_data);
    if (!ep) return nullptr;

    /* Add optional Battery-feature attributes not included by feature::battery::add(). */
    auto *cluster = esp_matter::cluster::get(
        ep, chip::app::Clusters::PowerSource::Id);
    esp_matter::cluster::power_source::attribute::create_bat_percent_remaining(
        cluster, nullable<uint8_t>(0), nullable<uint8_t>(0), nullable<uint8_t>(200));
    esp_matter::cluster::power_source::attribute::create_bat_voltage(
        cluster, nullable<uint32_t>(0), nullable<uint32_t>(3000), nullable<uint32_t>(4200));

    return ep;
}
