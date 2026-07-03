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

/* C facade for the water valve endpoint (device type 0x0042,
 * ValveConfigurationAndControl cluster). */

#pragma once

#include "matter_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ValveConfigurationAndControl cluster ID. */
extern const uint32_t esp_matter_valve_configuration_and_control_cluster_id;
/** ValveConfigurationAndControl::CurrentState attribute ID. */
extern const uint32_t esp_matter_valve_configuration_and_control_current_state_attribute_id;

/** command_id values for esp_matter_endpoint_water_valve_create. */
#define ESP_MATTER_VALVE_CMD_OPEN  0x00   /**< Open command (spec id 0x00) */
#define ESP_MATTER_VALVE_CMD_CLOSE 0x01   /**< Close command (spec id 0x01) */

/** C wrapper for esp_matter::endpoint::water_valve::create (device type 0x0042).
 *  Creates the endpoint and registers a C++ delegate trampoline that calls
 *  command_callback with ESP_MATTER_VALVE_CMD_OPEN or ESP_MATTER_VALVE_CMD_CLOSE.
 */
__attribute__((swift_name("esp_matter_endpoint_water_valve_create(commandCallback:privData:)")))
esp_matter_endpoint_t *
esp_matter_endpoint_water_valve_create(matter_command_cb_t command_callback, void *priv_data);

#ifdef __cplusplus
}
#endif
