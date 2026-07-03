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

#include "matter_water_valve.h"
#include <esp_matter.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app/clusters/valve-configuration-and-control-server/valve-configuration-and-control-delegate.h>

extern "C" const uint32_t esp_matter_valve_configuration_and_control_cluster_id =
    chip::app::Clusters::ValveConfigurationAndControl::Id;
extern "C" const uint32_t esp_matter_valve_configuration_and_control_current_state_attribute_id =
    chip::app::Clusters::ValveConfigurationAndControl::Attributes::CurrentState::Id;

/* C++ delegate trampoline for ValveConfigurationAndControl.
 * Allocated once per endpoint via new; never freed (process lifetime). */
class SwiftValveDelegate : public chip::app::Clusters::ValveConfigurationAndControl::Delegate {
public:
    matter_command_cb_t callback  = nullptr;
    void    *privData   = nullptr;
    uint16_t endpointId = 0;

    chip::app::DataModel::Nullable<chip::Percent>
    HandleOpenValve(chip::app::DataModel::Nullable<chip::Percent>) override
    {
        if (callback) callback(endpointId, ESP_MATTER_VALVE_CMD_OPEN, privData);
        return chip::app::DataModel::NullNullable;
    }

    CHIP_ERROR HandleCloseValve() override
    {
        if (callback) callback(endpointId, ESP_MATTER_VALVE_CMD_CLOSE, privData);
        return CHIP_NO_ERROR;
    }

    void HandleRemainingDurationTick(uint32_t) override {}
};

extern "C" esp_matter_endpoint_t *esp_matter_endpoint_water_valve_create(
    matter_command_cb_t command_callback, void *priv_data)
{
    auto *delegate = new SwiftValveDelegate();
    delegate->callback  = command_callback;
    delegate->privData  = priv_data;

    esp_matter::endpoint::water_valve::config_t cfg;
    cfg.valve_configuration_and_control.delegate = delegate;

    auto *ep = esp_matter::endpoint::water_valve::create(
        esp_matter::node::get(), &cfg, esp_matter::ENDPOINT_FLAG_NONE, nullptr);

    delegate->endpointId = esp_matter::endpoint::get_id(ep);
    return ep;
}
