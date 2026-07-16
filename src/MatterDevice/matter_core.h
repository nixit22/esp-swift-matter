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

/*
 * Core C facade: shared types, attribute helpers, node/stack API.
 *
 * The _esp_matter_* types mirror their esp_matter counterparts byte-for-byte
 * so matter_core.cpp can reinterpret_cast / memcpy between them without UB.
 * Do not reorder fields or change member types independently of the upstream
 * esp_matter_attr_val_t definition.
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <esp_err.h>
#include <swift_support.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Nullable base for nullable attribute */
#define _ESP_MATTER_VAL_NULLABLE_BASE 0x80

/** ESP Matter Attribute Value type */
typedef enum
{
    /** Invalid */
    _ESP_MATTER_VAL_TYPE_INVALID = 0,
    /** Boolean */
    _ESP_MATTER_VAL_TYPE_BOOLEAN = 1,
    /** Integer. Mapped to a 32 bit signed integer */
    _ESP_MATTER_VAL_TYPE_INTEGER = 2,
    /** Floating point number */
    _ESP_MATTER_VAL_TYPE_FLOAT = 3,
    /** Array Eg. [1,2,3] */
    _ESP_MATTER_VAL_TYPE_ARRAY = 4,
    /** Char String Eg. "123", Max length 0xFE */
    _ESP_MATTER_VAL_TYPE_CHAR_STRING = 5,
    /** Octet String Eg. [0x01, 0x20], Max length 0xFE */
    _ESP_MATTER_VAL_TYPE_OCTET_STRING = 6,
    /** 8 bit signed integer */
    _ESP_MATTER_VAL_TYPE_INT8 = 7,
    /** 8 bit unsigned integer */
    _ESP_MATTER_VAL_TYPE_UINT8 = 8,
    /** 16 bit signed integer */
    _ESP_MATTER_VAL_TYPE_INT16 = 9,
    /** 16 bit unsigned integer */
    _ESP_MATTER_VAL_TYPE_UINT16 = 10,
    /** 32 bit signed integer */
    _ESP_MATTER_VAL_TYPE_INT32 = 11,
    /** 32 bit unsigned integer */
    _ESP_MATTER_VAL_TYPE_UINT32 = 12,
    /** 64 bit signed integer */
    _ESP_MATTER_VAL_TYPE_INT64 = 13,
    /** 64 bit unsigned integer */
    _ESP_MATTER_VAL_TYPE_UINT64 = 14,
    /** 8 bit enum */
    _ESP_MATTER_VAL_TYPE_ENUM8 = 15,
    /** 8 bit bitmap */
    _ESP_MATTER_VAL_TYPE_BITMAP8 = 16,
    /** 16 bit bitmap */
    _ESP_MATTER_VAL_TYPE_BITMAP16 = 17,
    /** 32 bit bitmap */
    _ESP_MATTER_VAL_TYPE_BITMAP32 = 18,
    /** 16 bit enum */
    _ESP_MATTER_VAL_TYPE_ENUM16 = 19,
    /** Long Char String, Max length 0xFFFE **/
    _ESP_MATTER_VAL_TYPE_LONG_CHAR_STRING = 20,
    /** Long Octet String, Max length 0xFFFE **/
    _ESP_MATTER_VAL_TYPE_LONG_OCTET_STRING = 21,

    /** nullable types **/
    _ESP_MATTER_VAL_TYPE_NULLABLE_BOOLEAN = _ESP_MATTER_VAL_TYPE_BOOLEAN + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_INTEGER = _ESP_MATTER_VAL_TYPE_INTEGER + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_FLOAT = _ESP_MATTER_VAL_TYPE_FLOAT + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_INT8 = _ESP_MATTER_VAL_TYPE_INT8 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_UINT8 = _ESP_MATTER_VAL_TYPE_UINT8 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_INT16 = _ESP_MATTER_VAL_TYPE_INT16 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_UINT16 = _ESP_MATTER_VAL_TYPE_UINT16 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_INT32 = _ESP_MATTER_VAL_TYPE_INT32 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_UINT32 = _ESP_MATTER_VAL_TYPE_UINT32 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_INT64 = _ESP_MATTER_VAL_TYPE_INT64 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_UINT64 = _ESP_MATTER_VAL_TYPE_UINT64 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_ENUM8 = _ESP_MATTER_VAL_TYPE_ENUM8 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_BITMAP8 = _ESP_MATTER_VAL_TYPE_BITMAP8 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_BITMAP16 = _ESP_MATTER_VAL_TYPE_BITMAP16 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_BITMAP32 = _ESP_MATTER_VAL_TYPE_BITMAP32 + _ESP_MATTER_VAL_NULLABLE_BASE,
    _ESP_MATTER_VAL_TYPE_NULLABLE_ENUM16 = _ESP_MATTER_VAL_TYPE_ENUM16 + _ESP_MATTER_VAL_NULLABLE_BASE,
} _esp_matter_val_type_t;

/** ESP Matter Value */
typedef union
{
    /** Boolean */
    bool b;
    /** Integer */
    int i;
    /** Float */
    float f;
    /** 8 bit signed integer */
    int8_t i8;
    /** 8 bit unsigned integer */
    uint8_t u8;
    /** 16 bit signed integer */
    int16_t i16;
    /** 16 bit unsigned integer */
    uint16_t u16;
    /** 32 bit signed integer */
    int32_t i32;
    /** 32 bit unsigned integer */
    uint32_t u32;
    /** 64 bit signed integer */
    int64_t i64;
    /** 64 bit unsigned integer */
    uint64_t u64;
    /** Array */
    struct
    {
        /** Buffer */
        uint8_t *b;
        /** Data size */
        uint16_t s;
        /** Data count */
        uint16_t n;
        /** Total size */
        uint16_t t;
    } a;
    /** Pointer */
    void *p;
} _esp_matter_val_t;

/** ESP Matter Attribute Value */
typedef struct
{
    /** Type of Value */
    _esp_matter_val_type_t type;
    /** Actual value. Depends on the type */
    _esp_matter_val_t val;
} _esp_matter_attr_val_t;

/* Opaque handle for both node_t* and endpoint_t* pointers from the C++ SDK.
 * Swift never dereferences it — it's passed back to C functions only. */
typedef size_t esp_matter_endpoint_t;

typedef esp_err_t (*callback_t)(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                                _esp_matter_attr_val_t *val, void *priv_data);

/** Generic command callback for delegate-pattern clusters.
 *
 *  command_id encodes the specific command; values are cluster-specific and
 *  documented alongside each create function (see ESP_MATTER_*_CMD_* macros).
 *  The callback always returns void — the C++ delegate trampoline reports
 *  CHIP_NO_ERROR unconditionally, which is correct for simple actuators.
 */
typedef void (*matter_command_cb_t)(uint16_t endpoint_id, uint8_t command_id, void *priv_data);

/** Build a nullable 16-bit signed attribute value. When isNull is true the
 *  value field is ignored and the attribute is set to the Matter null sentinel.
 */
SWIFT_NAME("esp_matter_attr_nullable_int16(_:isNull:)")
_esp_matter_attr_val_t esp_matter_attr_nullable_int16(int16_t value, bool isNull);

/** Build a nullable 16-bit unsigned attribute value. When isNull is true the
 *  value field is ignored and the attribute is set to the Matter null sentinel.
 */
SWIFT_NAME("esp_matter_attr_nullable_uint16(_:isNull:)")
_esp_matter_attr_val_t esp_matter_attr_nullable_uint16(uint16_t value, bool isNull);

/** Build a nullable 8-bit unsigned attribute value. When isNull is true the
 *  value field is ignored and the attribute is set to the Matter null sentinel.
 */
SWIFT_NAME("esp_matter_attr_nullable_uint8(_:isNull:)")
_esp_matter_attr_val_t esp_matter_attr_nullable_uint8(uint8_t value, bool isNull);

/** Build a nullable 32-bit unsigned attribute value. When isNull is true the
 *  value field is ignored and the attribute is set to the Matter null sentinel.
 */
SWIFT_NAME("esp_matter_attr_nullable_uint32(_:isNull:)")
_esp_matter_attr_val_t esp_matter_attr_nullable_uint32(uint32_t value, bool isNull);

/** Build a nullable 8-bit enum attribute value (NULLABLE_ENUM8 tag).
 *  Use this for enum8 attributes such as ValveConfigurationAndControl::CurrentState.
 *  When isNull is true the value field is ignored.
 */
SWIFT_NAME("esp_matter_attr_nullable_enum8(_:isNull:)")
_esp_matter_attr_val_t esp_matter_attr_nullable_enum8(uint8_t value, bool isNull);

/** Build a non-nullable uint8 attribute value (UINT8 type tag). */
SWIFT_NAME("esp_matter_attr_val_uint8(_:)")
_esp_matter_attr_val_t esp_matter_attr_val_uint8(uint8_t value);

/** Extract uint8 from a non-nullable uint8 attribute value. */
SWIFT_NAME("esp_matter_attr_val_get_uint8(_:)")
uint8_t esp_matter_attr_val_get_uint8(_esp_matter_attr_val_t val);

esp_err_t set_default_openthread_platform_config(void);

/** Create the root Matter node and register attribute callbacks.
 *  willUpdate fires before an attribute write; didUpdate fires after.
 *  privData is stored as the fallback per-node private pointer; individual
 *  endpoints override it via esp_matter_endpoint_set_priv_data().
 */
SWIFT_NAME("esp_matter_create_node(nodeLabel:willUpdate:didUpdate:privData:)")
esp_matter_endpoint_t *
esp_matter_create_node(char *node_label, callback_t will_update, callback_t did_update, void *priv_data);

SWIFT_NAME("esp_matter_endpoint_get_id( _:)")
uint16_t
esp_matter_endpoint_get_id(esp_matter_endpoint_t *endpoint);

SWIFT_NAME("esp_matter_endpoint_set_priv_data(endpointId:privData:)")
esp_err_t
esp_matter_endpoint_set_priv_data(uint16_t endpoint_id, void *priv_data);

/** Event codes passed to esp_matter_event_cb_t.
 *  These are a simplified mapping of ChipDeviceEvent.Type values; the
 *  translation from CHIP SDK types to these codes happens in matter_core.cpp.
 *
 *  0  commissioningComplete       — commissioning via the commissioner app succeeded
 *  1  commissioningSessionStarted — BLE pairing session opened
 *  2  commissioningSessionStopped — BLE pairing session closed (succeeded or timed-out)
 *  3  commissioningWindowOpened   — commissioning window is open (QR code is active)
 *  4  commissioningWindowClosed   — commissioning window closed
 *  0xFF  unknown                  — any other CHIP event type
 */
typedef void (*esp_matter_event_cb_t)(uint8_t event, void *arg);

/** Start the Matter stack. callback may be NULL.
 *  Blocks until CHIP init completes. Call after all endpoints have been created.
 */
SWIFT_NAME("esp_matter_start(callback:callbackArg:)")
esp_err_t esp_matter_start(esp_matter_event_cb_t callback, void *callbackArg);

/** Print commissioning QR code and manual pairing code via ChipLogProgress.
 *  Safe to call from any task after esp_matter_start() returns.
 */
void esp_matter_print_onboarding_codes(void);

/** Returns true if the device has at least one commissioned fabric (i.e. has
 *  been commissioned). Safe to call from any task after esp_matter_start() returns.
 */
bool esp_matter_is_commissioned(void);

/** Erases all Matter/Thread NVS state and reboots the device.
 *
 *  Wraps esp_matter::factory_reset(), which schedules the reset on the CHIP event loop —
 *  requires chip::Server to be running, so only call after esp_matter_start() returns.
 *  Does not return on success: the device restarts once the reset completes.
 */
esp_err_t esp_matter_factory_reset(void);

SWIFT_NAME("esp_matter_attribute_update(endpointId:clusterId:attributeId:value:)")
esp_err_t esp_matter_attribute_update(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                                     _esp_matter_attr_val_t *val);

#ifdef __cplusplus
}
#endif
