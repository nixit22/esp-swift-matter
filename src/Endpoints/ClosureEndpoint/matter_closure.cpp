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
#include <app/clusters/closure-control-server/closure-control-cluster-delegate.h>
#include <app/clusters/closure-control-server/closure-control-cluster-logic.h>
#include <app/clusters/closure-control-server/closure-control-cluster-matter-context.h>
#include <app/clusters/closure-control-server/closure-control-server.h>

// Convenience aliases — avoid polluting the global namespace with using namespace.
namespace CC  = chip::app::Clusters::ClosureControl;
namespace DM  = chip::app::DataModel;

/* C++ delegate trampoline for the ClosureControl cluster.
 * Allocated once per endpoint via new; never freed (process lifetime).
 * clusterLogic is populated by SwiftClosureControlDelegateInitCB during esp_matter_start(). */
class SwiftClosureDelegate : public CC::DelegateBase {
public:
    closure_move_to_cb_t  moveToCallback  = nullptr;
    matter_command_cb_t   stopCalibrateCb = nullptr;
    closure_is_ready_cb_t isReadyCb       = nullptr;
    void                 *privData        = nullptr;
    uint16_t              endpointId      = 0;
    uint8_t               initialPosition = 0xFF;   // 0xFF == null/unknown
    uint32_t              featureFlags    = 0;
    CC::ClusterLogic     *clusterLogic    = nullptr;

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

// Up to 4 closure endpoints; each slot holds one delegate.
static SwiftClosureDelegate *gClosureTable[4] = {};

static SwiftClosureDelegate *findClosureDelegate(uint16_t endpoint_id)
{
    for (int i = 0; i < 4; i++) {
        if (gClosureTable[i] && gClosureTable[i]->endpointId == endpoint_id)
            return gClosureTable[i];
    }
    return nullptr;
}

// Custom init CB called during esp_matter_start(). Fixes the esp_matter 1.5.0 bug
// where ClosureControlDelegateInitCB creates ClusterLogic but never calls Init(),
// causing VerifyOrDieWithMsg() aborts on the first MoveTo or Stop command.
static void SwiftClosureControlDelegateInitCB(void *delegate, uint16_t endpoint_id)
{
    if (!delegate) return;
    auto *d = static_cast<SwiftClosureDelegate *>(delegate);

    auto *context = new CC::MatterContext(endpoint_id);
    auto *logic   = new CC::ClusterLogic(*d, *context);

    CC::ClusterConformance conformance;
    conformance.FeatureMap().SetRaw(d->featureFlags);

    CC::ClusterInitParameters initParams;
    if (d->initialPosition != 0xFF) {
        initParams.mOverallCurrentState = DM::MakeNullable(
            CC::GenericOverallCurrentState(
                chip::MakeOptional(
                    DM::MakeNullable(static_cast<CC::CurrentPositionEnum>(d->initialPosition)))));
    }

    if (logic->Init(conformance, initParams) != CHIP_NO_ERROR) {
        ChipLogError(AppServer, "ClosureControl ClusterLogic::Init failed");
        return;
    }
    auto *server = new CC::Interface(endpoint_id, *logic);
    if (server->Init() != CHIP_NO_ERROR) {
        ChipLogError(AppServer, "ClosureControl Interface::Init failed");
        return;
    }
    d->clusterLogic = logic;
}

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
    d->initialPosition = initial_position;
    d->featureFlags    = feature_flags;

    esp_matter::endpoint::closure::config_t cfg;
    cfg.closure_control.feature_flags = feature_flags;
    // delegate = nullptr so the stock (buggy) ClosureControlDelegateInitCB is not registered.
    // We register our own CB below via set_delegate_and_init_callback.
    cfg.closure_control.delegate = nullptr;

    auto *ep = esp_matter::endpoint::closure::create(
        esp_matter::node::get(), &cfg, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
    if (!ep) {
        delete d;
        ChipLogError(AppServer, "Failed to create closure endpoint");
        abort();
    }
    d->endpointId = esp_matter::endpoint::get_id(ep);

    // Register our custom init CB that correctly calls ClusterLogic::Init().
    esp_matter::cluster_t *cluster =
        esp_matter::cluster::get(ep, chip::app::Clusters::ClosureControl::Id);
    if (cluster) {
        esp_matter::cluster::set_delegate_and_init_callback(
            cluster, SwiftClosureControlDelegateInitCB, d);
    }

    // Store delegate for state-update functions (setMainState / setCurrentPosition).
    bool stored = false;
    for (int i = 0; i < 4; i++) {
        if (!gClosureTable[i]) { gClosureTable[i] = d; stored = true; break; }
    }
    if (!stored) {
        // Table full (>4 closure endpoints): setMainState/setCurrentPosition will return
        // ESP_ERR_NOT_FOUND for this endpoint since findClosureDelegate() can never find it.
        ChipLogError(AppServer, "gClosureTable full — closure endpoint %u state updates will fail",
                     d->endpointId);
    }
    return ep;
}

extern "C" esp_err_t esp_matter_closure_set_main_state(uint16_t endpoint_id, uint8_t main_state)
{
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    auto *d = findClosureDelegate(endpoint_id);
    if (!d || !d->clusterLogic) return ESP_ERR_NOT_FOUND;
    CHIP_ERROR err = d->clusterLogic->SetMainState(
        static_cast<CC::MainStateEnum>(main_state));
    return err == CHIP_NO_ERROR ? ESP_OK : ESP_FAIL;
}

extern "C" esp_err_t esp_matter_closure_set_current_position(
    uint16_t endpoint_id, bool has_position, uint8_t position)
{
    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    auto *d = findClosureDelegate(endpoint_id);
    if (!d || !d->clusterLogic) return ESP_ERR_NOT_FOUND;

    DM::Nullable<CC::GenericOverallCurrentState> state;   // NullNullable by default
    if (has_position) {
        state = DM::MakeNullable(CC::GenericOverallCurrentState(
            chip::MakeOptional(
                DM::MakeNullable(static_cast<CC::CurrentPositionEnum>(position)))));
    }
    CHIP_ERROR err = d->clusterLogic->SetOverallCurrentState(state);
    return err == CHIP_NO_ERROR ? ESP_OK : ESP_FAIL;
}
