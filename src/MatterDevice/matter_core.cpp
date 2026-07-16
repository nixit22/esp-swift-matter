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

#include "matter_core.h"
#include <string.h>
#include <esp_matter.h>
#include <platform/ESP32/OpenthreadLauncher.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <app/server/Server.h>

/* Allocated once per node and intentionally never freed: the root node lives
 * for the entire process lifetime and its attribute callback fires until reset.
 * Freeing ctx would cause use-after-free on the next callback. */
struct callback_context
{
    callback_t willUpdate;
    callback_t didUpdate;
    void *user_priv_data;
};

extern "C" esp_matter_endpoint_t *esp_matter_create_node(char *nodeLabel, callback_t willUpdate, callback_t didUpdate, void *privData)
{
    esp_matter::node::config_t node_config = {};

    if (nodeLabel)
    {
        strncpy(node_config.root_node.basic_information.node_label, nodeLabel,
                sizeof(node_config.root_node.basic_information.node_label) - 1);
        node_config.root_node.basic_information.node_label[sizeof(node_config.root_node.basic_information.node_label) - 1] = '\0';
    }

    callback_context *ctx = new callback_context{willUpdate, didUpdate, privData};

    esp_matter::node_t *node = esp_matter::node::create(
        &node_config,
        [](esp_matter::attribute::callback_type type, uint16_t endpoint_id, uint32_t cluster_id,
           uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data) -> esp_err_t
        {
            callback_context *ctx = static_cast<callback_context *>(priv_data);

            // Prefer the per-endpoint priv_data (set by esp_matter_endpoint_set_priv_data);
            // fall back to the node-level pointer for the root endpoint, which has none.
            void *ep_priv = esp_matter::endpoint::get_priv_data(endpoint_id);
            if (!ep_priv) ep_priv = ctx->user_priv_data;

            if (type == esp_matter::attribute::PRE_UPDATE && ctx->willUpdate)
            {
                return ctx->willUpdate(endpoint_id, cluster_id, attribute_id, (_esp_matter_attr_val_t *)val, ep_priv);
            }
            else if (type == esp_matter::attribute::POST_UPDATE && ctx->didUpdate)
            {
                return ctx->didUpdate(endpoint_id, cluster_id, attribute_id, (_esp_matter_attr_val_t *)val, ep_priv);
            }
            return ESP_OK;
        },
        nullptr,
        ctx);

    return node;
}

extern "C" uint16_t esp_matter_endpoint_get_id(esp_matter_endpoint_t *endpoint)
{
    return esp_matter::endpoint::get_id(endpoint);
}

extern "C" esp_err_t esp_matter_endpoint_set_priv_data(uint16_t endpoint_id, void *priv_data)
{
    return esp_matter::endpoint::set_priv_data(endpoint_id, priv_data);
}

extern "C" esp_err_t esp_matter_attribute_update(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                                     _esp_matter_attr_val_t *val)
{
    return esp_matter::attribute::update(endpoint_id, cluster_id, attribute_id,
                                         reinterpret_cast<esp_matter_attr_val_t *>(val));
}

/* Context for the event callback trampoline — allocated once, never freed. */
struct event_callback_context
{
    esp_matter_event_cb_t callback;
    void *arg;
};

static uint8_t translate_event(uint16_t type)
{
    using namespace chip::DeviceLayer::DeviceEventType;
    if (type == kCommissioningComplete)        return 0;
    if (type == kCommissioningSessionStarted)  return 1;
    if (type == kCommissioningSessionStopped)  return 2;
    if (type == kCommissioningWindowOpened)    return 3;
    if (type == kCommissioningWindowClosed)    return 4;
    return 0xFF;
}

extern "C" esp_err_t esp_matter_start(esp_matter_event_cb_t callback, void *callbackArg)
{
    if (callback) {
        auto *ctx = new event_callback_context{callback, callbackArg};
        return esp_matter::start(
            [](const ChipDeviceEvent *event, intptr_t arg) {
                auto *ctx = reinterpret_cast<event_callback_context *>(arg);
                ctx->callback(translate_event(event->Type), ctx->arg);
            },
            reinterpret_cast<intptr_t>(ctx));
    }
    return esp_matter::start(nullptr);
}

extern "C" void esp_matter_print_onboarding_codes(void)
{
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
}

extern "C" bool esp_matter_is_commissioned(void)
{
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
}

extern "C" esp_err_t esp_matter_factory_reset(void)
{
    return esp_matter::factory_reset();
}

extern "C" _esp_matter_attr_val_t esp_matter_attr_nullable_int16(int16_t value, bool isNull)
{
    nullable<int16_t> n = isNull ? nullable<int16_t>() : nullable<int16_t>(value);
    esp_matter_attr_val_t v = esp_matter_nullable_int16(n);
    _esp_matter_attr_val_t out;
    // memcpy is safe: _esp_matter_attr_val_t mirrors esp_matter_attr_val_t byte-for-byte.
    memcpy(&out, &v, sizeof(out));
    return out;
}

extern "C" _esp_matter_attr_val_t esp_matter_attr_nullable_uint16(uint16_t value, bool isNull)
{
    nullable<uint16_t> n = isNull ? nullable<uint16_t>() : nullable<uint16_t>(value);
    esp_matter_attr_val_t v = esp_matter_nullable_uint16(n);
    _esp_matter_attr_val_t out;
    memcpy(&out, &v, sizeof(out));
    return out;
}

extern "C" _esp_matter_attr_val_t esp_matter_attr_nullable_uint8(uint8_t value, bool isNull)
{
    nullable<uint8_t> n = isNull ? nullable<uint8_t>() : nullable<uint8_t>(value);
    esp_matter_attr_val_t v = esp_matter_nullable_uint8(n);
    _esp_matter_attr_val_t out;
    memcpy(&out, &v, sizeof(out));
    return out;
}

extern "C" _esp_matter_attr_val_t esp_matter_attr_nullable_uint32(uint32_t value, bool isNull)
{
    nullable<uint32_t> n = isNull ? nullable<uint32_t>() : nullable<uint32_t>(value);
    esp_matter_attr_val_t v = esp_matter_nullable_uint32(n);
    _esp_matter_attr_val_t out;
    memcpy(&out, &v, sizeof(out));
    return out;
}

extern "C" _esp_matter_attr_val_t esp_matter_attr_nullable_enum8(uint8_t value, bool isNull)
{
    nullable<uint8_t> n = isNull ? nullable<uint8_t>() : nullable<uint8_t>(value);
    esp_matter_attr_val_t v = esp_matter_nullable_enum8(n);
    _esp_matter_attr_val_t out;
    memcpy(&out, &v, sizeof(out));
    return out;
}

extern "C" _esp_matter_attr_val_t esp_matter_attr_val_uint8(uint8_t value)
{
    _esp_matter_attr_val_t out;
    out.type   = _ESP_MATTER_VAL_TYPE_UINT8;
    out.val.u8 = value;
    return out;
}

extern "C" uint8_t esp_matter_attr_val_get_uint8(_esp_matter_attr_val_t val)
{
    return val.val.u8;
}

extern "C" esp_err_t set_default_openthread_platform_config(void)
{
    esp_openthread_platform_config_t config = {
        .radio_config = RADIO_MODE_NATIVE,
        .host_config = HOST_CONNECTION_MODE_NONE,
        .port_config = {
            .storage_partition_name = "nvs",
            .netif_queue_size = 10,
            .task_queue_size = 10,
        }
    };
    return set_openthread_platform_config(&config);
}
