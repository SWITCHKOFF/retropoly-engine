// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_log_rate.cpp
//
// Реализация rate limit debug-канала N64 (контракт — n64_log_rate.h).
#include "hal/n64/n64_log_rate.h"

namespace {

bool s_window_open = false;
uint32_t s_window_start_ms = 0u;
uint32_t s_window_count = 0u;
uint32_t s_dropped = 0u;

}  // namespace

extern "C" bool rpe_n64_log_rate_allow(uint32_t now_ms)
{
    if (!s_window_open || (uint32_t)(now_ms - s_window_start_ms) >= RPE_N64_LOG_RATE_WINDOW_MS) {
        s_window_open = true;
        s_window_start_ms = now_ms;
        s_window_count = 0u;
    }

    if (s_window_count >= RPE_N64_LOG_RATE_LIMIT) {
        ++s_dropped;
        return false;
    }

    ++s_window_count;
    return true;
}

extern "C" uint32_t rpe_n64_log_rate_dropped(void)
{
    return s_dropped;
}

extern "C" void rpe_n64_log_rate_reset(void)
{
    s_window_open = false;
    s_window_start_ms = 0u;
    s_window_count = 0u;
    s_dropped = 0u;
}
