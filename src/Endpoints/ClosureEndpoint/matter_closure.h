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

/* C facade for the closure endpoint (Matter 1.5, device type 0x0230,
 * ClosureControl cluster). */

#pragma once

#include "matter_core.h"
#include <swift_support.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Feature flag bits for esp_matter_endpoint_closure_create().
 *  Values match ClosureControl::Feature enum bits exactly so they can be
 *  forwarded directly to ClusterConformance::FeatureMap::SetRaw().
 *  At least POSITIONING or MOTION_LATCHING must be set. */
#define ESP_MATTER_CLOSURE_FEATURE_POSITIONING       0x00000001u
#define ESP_MATTER_CLOSURE_FEATURE_MOTION_LATCHING   0x00000002u
#define ESP_MATTER_CLOSURE_FEATURE_INSTANTANEOUS     0x00000004u
#define ESP_MATTER_CLOSURE_FEATURE_SPEED             0x00000008u
#define ESP_MATTER_CLOSURE_FEATURE_VENTILATION       0x00000010u
#define ESP_MATTER_CLOSURE_FEATURE_PEDESTRIAN        0x00000020u
#define ESP_MATTER_CLOSURE_FEATURE_CALIBRATION       0x00000040u
#define ESP_MATTER_CLOSURE_FEATURE_PROTECTION        0x00000080u
#define ESP_MATTER_CLOSURE_FEATURE_MANUALLY_OPERABLE 0x00000100u

/** command_id values for the stop_calibrate_callback. */
#define ESP_MATTER_CLOSURE_CMD_STOP      0x00u
#define ESP_MATTER_CLOSURE_CMD_CALIBRATE 0x02u

/** TargetPositionEnum raw values delivered to closure_move_to_cb_t. */
#define ESP_MATTER_CLOSURE_TARGET_FULLY_CLOSED    0x00u
#define ESP_MATTER_CLOSURE_TARGET_FULLY_OPEN      0x01u
#define ESP_MATTER_CLOSURE_TARGET_PEDESTRIAN      0x02u
#define ESP_MATTER_CLOSURE_TARGET_VENTILATION     0x03u
#define ESP_MATTER_CLOSURE_TARGET_SIGNATURE       0x04u

/** CurrentPositionEnum raw values for esp_matter_closure_set_current_position(). */
#define ESP_MATTER_CLOSURE_CURRENT_FULLY_CLOSED   0x00u
#define ESP_MATTER_CLOSURE_CURRENT_FULLY_OPEN     0x01u
#define ESP_MATTER_CLOSURE_CURRENT_PARTIALLY_OPEN 0x02u
#define ESP_MATTER_CLOSURE_CURRENT_PEDESTRIAN     0x03u
#define ESP_MATTER_CLOSURE_CURRENT_VENTILATION    0x04u
#define ESP_MATTER_CLOSURE_CURRENT_SIGNATURE      0x05u

/** MainStateEnum raw values for esp_matter_closure_set_main_state(). */
#define ESP_MATTER_CLOSURE_STATE_STOPPED            0x00u
#define ESP_MATTER_CLOSURE_STATE_MOVING             0x01u
#define ESP_MATTER_CLOSURE_STATE_WAITING_FOR_MOTION 0x02u
#define ESP_MATTER_CLOSURE_STATE_ERROR              0x03u
#define ESP_MATTER_CLOSURE_STATE_CALIBRATING        0x04u
#define ESP_MATTER_CLOSURE_STATE_PROTECTED          0x05u
#define ESP_MATTER_CLOSURE_STATE_DISENGAGED         0x06u
#define ESP_MATTER_CLOSURE_STATE_SETUP_REQUIRED     0x07u

/** Fired when the controller sends a MoveTo command.
 *  has_* flags indicate whether the optional field was present in the command.
 *  position: ESP_MATTER_CLOSURE_TARGET_* (TargetPositionEnum raw value).
 *  latch:    true = latch closed, false = unlatch.
 *  speed:    ThreeLevelAutoEnum raw value (0=Low 1=Medium 2=High 3=Auto).
 *  May be NULL.
 */
typedef void (*closure_move_to_cb_t)(
    uint16_t endpoint_id,
    bool has_position, uint8_t position,
    bool has_latch,    bool latch,
    bool has_speed,    uint8_t speed,
    void *priv_data
);

/** Polled by ClusterLogic while MainState is WaitingForMotion.
 *  Return false to hold the closure in waiting state (e.g. obstacle detected).
 *  NULL defaults to always-ready.
 */
typedef bool (*closure_is_ready_cb_t)(uint16_t endpoint_id, void *priv_data);

/** Create a closure endpoint (device type 0x0230, ClosureControl cluster).
 *
 *  feature_flags:           OR of ESP_MATTER_CLOSURE_FEATURE_* bits.
 *                           Must include POSITIONING or MOTION_LATCHING.
 *  initial_position:        ESP_MATTER_CLOSURE_CURRENT_* value known at boot,
 *                           or 0xFF if position is unknown. Providing a value lets
 *                           MoveTo commands succeed immediately after run().
 *  move_to_callback:        Called on MoveTo command. May be NULL.
 *  stop_calibrate_callback: Called with ESP_MATTER_CLOSURE_CMD_STOP or
 *                           ESP_MATTER_CLOSURE_CMD_CALIBRATE. May be NULL.
 *  is_ready_callback:       Polled in WaitingForMotion state. NULL = always ready.
 *  priv_data:               Forwarded to all callbacks unchanged.
 *
 *  Must be called before esp_matter_start(). Aborts on failure.
 */
SWIFT_NAME("esp_matter_endpoint_closure_create(featureFlags:initialPosition:moveToCallback:stopCalibrateCallback:isReadyCallback:privData:)")
esp_matter_endpoint_t *
esp_matter_endpoint_closure_create(
    uint32_t              feature_flags,
    uint8_t               initial_position,
    closure_move_to_cb_t  move_to_callback,
    matter_command_cb_t   stop_calibrate_callback,
    closure_is_ready_cb_t is_ready_callback,
    void                 *priv_data
);

/** Update the ClosureControl MainState attribute.
 *  main_state: one of ESP_MATTER_CLOSURE_STATE_*.
 *  Acquires the CHIP stack lock internally. Do NOT hold the lock when calling.
 */
SWIFT_NAME("esp_matter_closure_set_main_state(endpointId:mainState:)")
esp_err_t esp_matter_closure_set_main_state(uint16_t endpoint_id, uint8_t main_state);

/** Update ClosureControl OverallCurrentState.position.
 *  Pass has_position=false to report position as unknown (null).
 *  position: one of ESP_MATTER_CLOSURE_CURRENT_*.
 *  Acquires the CHIP stack lock internally. Do NOT hold the lock when calling.
 */
SWIFT_NAME("esp_matter_closure_set_current_position(endpointId:hasPosition:position:)")
esp_err_t esp_matter_closure_set_current_position(
    uint16_t endpoint_id, bool has_position, uint8_t position);

#ifdef __cplusplus
}
#endif
