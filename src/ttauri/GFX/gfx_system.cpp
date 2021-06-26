// Copyright Take Vos 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include "gfx_system.hpp"
#include "gfx_system_vulkan.hpp"
#include "gfx_surface.hpp"
#include "../logger.hpp"
#include <chrono>

namespace tt {

using namespace std;

gfx_device *gfx_system::findBestDeviceForSurface(gfx_surface const &surface)
{
    ttlet lock = std::scoped_lock(gfx_system_mutex);

    int best_score = -1;
    gfx_device *best_device = nullptr;

    for (ttlet &device : devices) {
        ttlet score = device->score(surface);
        if (score >= best_score) {
            best_score = score;
            best_device = device.get();
        }
    }

    if (best_score <= 0) {
        tt_log_fatal("Could not find a graphics device suitable for presenting this window.");
    }
    return best_device;
}

[[nodiscard]] gfx_system *gfx_system::subsystem_init() noexcept
{
    auto tmp = new gfx_system_vulkan();
    tmp->init();
    return tmp;
}

[[nodiscard]] void gfx_system::subsystem_deinit() noexcept
{
    if (auto tmp = _global.exchange(nullptr)) {
        tmp->deinit();
        delete tmp;
    }
}



}