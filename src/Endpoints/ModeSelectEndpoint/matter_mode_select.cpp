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

#include "matter_mode_select.h"
#include <string.h>
#include <esp_matter.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app/clusters/mode-select-server/supported-modes-manager.h>

namespace MS = chip::app::Clusters::ModeSelect;

extern "C" const uint32_t esp_matter_mode_select_cluster_id = MS::Id;
extern "C" const uint32_t esp_matter_mode_select_current_mode_attribute_id =
    MS::Attributes::CurrentMode::Id;

struct ModeEntry {
    uint8_t mode;
    char    label[33];
};

struct EndpointModesTable {
    uint16_t  endpoint_id;
    uint8_t   count;
    ModeEntry entries[8];
    MS::Structs::ModeOptionStruct::Type options[8];
};

class SwiftSupportedModesManager : public MS::SupportedModesManager
{
    using ModeOptionStructType = MS::Structs::ModeOptionStruct::Type;
public:
    static constexpr int kMaxEndpoints = 4;
    EndpointModesTable table[kMaxEndpoints] = {};
    int tableCount = 0;

    EndpointModesTable *findOrCreate(uint16_t eid)
    {
        for (int i = 0; i < tableCount; i++)
            if (table[i].endpoint_id == eid) return &table[i];
        if (tableCount < kMaxEndpoints) {
            table[tableCount] = EndpointModesTable{eid, 0, {}, {}};
            return &table[tableCount++];
        }
        return nullptr;
    }

    void addMode(uint16_t eid, uint8_t mode, const char *label)
    {
        auto *em = findOrCreate(eid);
        if (!em || em->count >= 8) return;
        auto &entry = em->entries[em->count];
        entry.mode = mode;
        strncpy(entry.label, label, 32);
        entry.label[32] = '\0';
        auto &opt = em->options[em->count];
        opt.mode        = mode;
        opt.label       = chip::CharSpan(entry.label, strlen(entry.label));
        opt.semanticTags = chip::app::DataModel::List<
            const MS::Structs::SemanticTagStruct::Type>();
        em->count++;
    }

    ModeOptionsProvider getModeOptionsProvider(chip::EndpointId eid) const override
    {
        for (int i = 0; i < tableCount; i++)
            if (table[i].endpoint_id == eid)
                return ModeOptionsProvider(table[i].options,
                                           table[i].options + table[i].count);
        return ModeOptionsProvider();
    }

    chip::Protocols::InteractionModel::Status getModeOptionByMode(
        chip::EndpointId eid, uint8_t mode, const ModeOptionStructType **dataPtr) const override
    {
        for (int i = 0; i < tableCount; i++) {
            if (table[i].endpoint_id != eid) continue;
            for (int j = 0; j < table[i].count; j++) {
                if (table[i].options[j].mode == mode) {
                    *dataPtr = &table[i].options[j];
                    return chip::Protocols::InteractionModel::Status::Success;
                }
            }
            return chip::Protocols::InteractionModel::Status::InvalidCommand;
        }
        return chip::Protocols::InteractionModel::Status::UnsupportedEndpoint;
    }
};

static SwiftSupportedModesManager gModesManager;

extern "C" esp_matter_endpoint_t *esp_matter_endpoint_mode_select_create(
    const char *description, uint8_t initial_mode, void *priv_data)
{
    esp_matter::endpoint::mode_select::config_t cfg;
    strncpy(cfg.mode_select.description, description, 64);
    cfg.mode_select.description[64] = '\0';
    cfg.mode_select.current_mode    = initial_mode;

    auto *ep = esp_matter::endpoint::mode_select::create(
        esp_matter::node::get(), &cfg, esp_matter::ENDPOINT_FLAG_NONE, priv_data);
    if (!ep) {
        ChipLogError(AppServer, "Failed to create mode_select endpoint");
        abort();
    }
    MS::setSupportedModesManager(&gModesManager);
    return ep;
}

extern "C" void esp_matter_mode_select_add_mode(
    esp_matter_endpoint_t *endpoint, uint8_t mode, const char *label)
{
    uint16_t eid = esp_matter::endpoint::get_id(
        reinterpret_cast<esp_matter::endpoint_t *>(endpoint));
    gModesManager.addMode(eid, mode, label);
}
