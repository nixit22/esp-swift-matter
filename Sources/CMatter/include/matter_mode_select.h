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

/* C facade for the Mode Select endpoint (cluster 0x0050, device type 0x0050). */

#pragma once

#include "matter_core.h"
#include <swift_support.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Mode Select cluster ID. */
extern const uint32_t esp_matter_mode_select_cluster_id;
/** Mode Select CurrentMode attribute ID. */
extern const uint32_t esp_matter_mode_select_current_mode_attribute_id;

/** Create a mode_select endpoint (device type 0x0050).
 *  description:  Label shown in Matter controllers (64 chars max).
 *  initial_mode: Initial CurrentMode value; must match a mode added via
 *                esp_matter_mode_select_add_mode().
 *  priv_data:    Forwarded to attribute callbacks unchanged.
 *
 *  Call esp_matter_mode_select_add_mode() after this to register mode options.
 *  Must be called before esp_matter_start(). Aborts on failure.
 */
SWIFT_NAME("esp_matter_endpoint_mode_select_create(description:initialMode:privData:)")
esp_matter_endpoint_t *
esp_matter_endpoint_mode_select_create(
    const char *description,
    uint8_t     initial_mode,
    void       *priv_data
);

/** Register a mode option for a mode_select endpoint.
 *  endpoint: returned by esp_matter_endpoint_mode_select_create().
 *  mode:     Mode value (0–254). Must be unique for this endpoint.
 *  label:    Human-readable label (32 chars max, UTF-8).
 *
 *  Call once per mode after esp_matter_endpoint_mode_select_create()
 *  and before esp_matter_start().
 */
SWIFT_NAME("esp_matter_mode_select_add_mode(endpoint:mode:label:)")
void
esp_matter_mode_select_add_mode(
    esp_matter_endpoint_t *endpoint,
    uint8_t                mode,
    const char            *label
);

#ifdef __cplusplus
}
#endif
