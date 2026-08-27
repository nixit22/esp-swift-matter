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

#include "matter_time_synchronization.h"
#include <esp_matter.h>
#include <app/clusters/time-synchronization-server/DefaultTimeSyncDelegate.h>

namespace {

class EspMatterTimeSyncDelegate : public chip::app::Clusters::TimeSynchronization::DefaultTimeSyncDelegate
{
public:
    esp_matter_time_sync_callback_t onTimeSync = nullptr;

    void UTCTimeAvailabilityChanged(uint64_t time) override
    {
        if (onTimeSync != nullptr)
        {
            onTimeSync();
        }
    }
};

EspMatterTimeSyncDelegate gDelegate;

} // namespace

extern "C" esp_matter_endpoint_t *esp_matter_enable_time_synchronization(esp_matter_time_sync_callback_t on_time_sync)
{
    gDelegate.onTimeSync = on_time_sync;
    esp_matter::endpoint_t *root = esp_matter::endpoint::get(0);
    esp_matter::cluster::time_synchronization::config_t cfg;
    cfg.delegate = &gDelegate;
    esp_matter::cluster::time_synchronization::create(root, &cfg, esp_matter::CLUSTER_FLAG_SERVER);
    return root;
}
