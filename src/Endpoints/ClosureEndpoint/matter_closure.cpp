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

#include "matter_closure.h"
#include <esp_matter.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/clusters/closure-control-server/ClosureControlCluster.h>
#include <app/clusters/closure-control-server/ClosureControlClusterDelegate.h>
#include <clusters/closure_control/integration.h>

// Convenience aliases — avoid polluting the global namespace with using namespace.
namespace CC  = chip::app::Clusters::ClosureControl;
namespace DM  = chip::app::DataModel;

/* C++ delegate trampoline for the ClosureControl cluster.
 * Allocated once per endpoint via new; never freed (process lifetime).
 *
 * esp_matter 1.6.0 fixed the 1.5.0 bug this class used to work around (a missing
 * ClusterLogic::Init() call that aborted on the first MoveTo/Stop) — its own
 * ESPMatterClosureControlClusterServerInitCallback (data_model_provider/clusters/
 * closure_control/integration.cpp) now correctly constructs the cluster once
 * MatterClosureControlSetDelegate() has registered a delegate for the endpoint.
 * No custom init callback, ClusterLogic/MatterContext construction, or
 * endpoint→delegate lookup table needed any more — esp_matter.patch (hunks 3+4)
 * adds GetClusterInstance()/MatterClosureControlSetInitialOverallCurrentState()
 * for the two things that glue still doesn't expose publicly. */
class SwiftClosureDelegate : public CC::ClosureControlClusterDelegate {
public:
    closure_move_to_cb_t  moveToCallback  = nullptr;
    matter_command_cb_t   stopCalibrateCb = nullptr;
    closure_is_ready_cb_t isReadyCb       = nullptr;
    void                 *privData        = nullptr;
    uint16_t              endpointId      = 0;

    chip::Protocols::InteractionModel::Status HandleStopCommand() override
    {
        if (stopCalibrateCb) stopCalibrateCb(endpointId, ESP_MATTER_CLOSURE_CMD_STOP, privData);
        return chip::Protocols::InteractionModel::Status::Success;
    }

    chip::Protocols::InteractionModel::Status HandleMoveToCommand(
        const chip::Optional<CC::TargetPositionEnum> & position,
        const chip::Optional<bool>                   & latch,
        const chip::Optional<chip::app::Clusters::Globals::ThreeLevelAutoEnum> & speed) override
    {
        if (moveToCallback) {
            moveToCallback(
                endpointId,
                position.HasValue(), position.HasValue() ? static_cast<uint8_t>(position.Value()) : 0u,
                latch.HasValue(),    latch.HasValue() ? latch.Value() : false,
                speed.HasValue(),    speed.HasValue() ? static_cast<uint8_t>(speed.Value()) : 0u,
                privData);
        }
        return chip::Protocols::InteractionModel::Status::Success;
    }

    chip::Protocols::InteractionModel::Status HandleCalibrateCommand() override
    {
        if (stopCalibrateCb) stopCalibrateCb(endpointId, ESP_MATTER_CLOSURE_CMD_CALIBRATE, privData);
        return chip::Protocols::InteractionModel::Status::Success;
    }

    bool IsReadyToMove() override
    {
        return isReadyCb ? isReadyCb(endpointId, privData) : true;
    }

    chip::ElapsedS GetCalibrationCountdownTime()      override { return 0; }
    chip::ElapsedS GetMovingCountdownTime()           override { return 0; }
    chip::ElapsedS GetWaitingForMotionCountdownTime() override { return 0; }
};

extern "C" esp_matter_endpoint_t *esp_matter_endpoint_closure_create(
    uint32_t              feature_flags,
    uint8_t               initial_position,
    closure_move_to_cb_t  move_to_callback,
    matter_command_cb_t   stop_calibrate_callback,
    closure_is_ready_cb_t is_ready_callback,
    void                 *priv_data)
{
    auto *d           = new SwiftClosureDelegate();
    d->moveToCallback  = move_to_callback;
    d->stopCalibrateCb = stop_calibrate_callback;
    d->isReadyCb       = is_ready_callback;
    d->privData        = priv_data;

    esp_matter::endpoint::closure::config_t cfg;
    cfg.closure_control.feature_flags = feature_flags;
    cfg.closure_control.delegate = d;

    auto *ep = esp_matter::endpoint::closure::create(
        esp_matter::node::get(), &cfg, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
    if (!ep) {
        delete d;
        ChipLogError(AppServer, "Failed to create closure endpoint");
        abort();
    }
    d->endpointId = esp_matter::endpoint::get_id(ep);

    if (initial_position != 0xFF) {
        auto state = DM::MakeNullable(CC::GenericOverallCurrentState(
            chip::MakeOptional(
                DM::MakeNullable(static_cast<CC::CurrentPositionEnum>(initial_position)))));
        CC::MatterClosureControlSetInitialOverallCurrentState(d->endpointId, state);
    }

    return ep;
}

extern "C" esp_err_t esp_matter_closure_set_main_state(uint16_t endpoint_id, uint8_t main_state)
{
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    auto *cluster = CC::GetClusterInstance(endpoint_id);
    if (!cluster) return ESP_ERR_NOT_FOUND;
    CHIP_ERROR err = cluster->SetMainState(static_cast<CC::MainStateEnum>(main_state));
    return err == CHIP_NO_ERROR ? ESP_OK : ESP_FAIL;
}

extern "C" esp_err_t esp_matter_closure_set_current_position(
    uint16_t endpoint_id, bool has_position, uint8_t position)
{
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    auto *cluster = CC::GetClusterInstance(endpoint_id);
    if (!cluster) return ESP_ERR_NOT_FOUND;

    DM::Nullable<CC::GenericOverallCurrentState> state;   // NullNullable by default
    if (has_position) {
        state = DM::MakeNullable(CC::GenericOverallCurrentState(
            chip::MakeOptional(
                DM::MakeNullable(static_cast<CC::CurrentPositionEnum>(position)))));
    }
    CHIP_ERROR err = cluster->SetOverallCurrentState(state);
    return err == CHIP_NO_ERROR ? ESP_OK : ESP_FAIL;
}
