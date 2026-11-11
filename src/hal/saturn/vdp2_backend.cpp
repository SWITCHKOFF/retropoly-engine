// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp2_backend.cpp
//
// Реализация VDP2 NBG1 bitmap-фона (контракты — заголовок).
#include "hal/saturn/vdp2_backend.h"

#include "core/log/log.h"
#include "hal/saturn/jo_saturn_bridge.h"

#include <string.h>

namespace saturn {

namespace {

bool s_inited = false;
fixed_t s_pending_x = 0;
fixed_t s_pending_y = 0;
Vdp2BackendStats s_stats;

bool valid_texture(const TextureAsset* texture)
{
    if (texture == nullptr || texture->pixels == nullptr || texture->palette == nullptr) {
        return false;
    }
    if (texture->width == 0u || texture->height == 0u || (texture->width & 7u) != 0u ||
        (texture->height & 7u) != 0u) {
        return false;
    }
    if (texture->width > VDP2_BITMAP_WIDTH || texture->height > VDP2_BITMAP_HEIGHT) {
        return false;
    }
    return true;
}

int16_t fixed_to_pixels(fixed_t value)
{
    // Округление half away from zero, чтобы scroll не «дрожал» на ±1.
    const int64_t scaled = static_cast<int64_t>(value);
    const int64_t bias = (scaled >= 0) ? (FIXED_ONE / 2) : -(FIXED_ONE / 2);
    const int64_t pixels = (scaled + bias) / FIXED_ONE;
    if (pixels > INT16_MAX) {
        return INT16_MAX;
    }
    if (pixels < INT16_MIN) {
        return INT16_MIN;
    }
    return static_cast<int16_t>(pixels);
}

}  // namespace

bool vdp2_backend_init(void)
{
    if (s_inited) {
        return true;
    }
    if (!rpe_saturn_vdp2_init(VDP2_BACKGROUND_PALETTE)) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp2: NBG1 bitmap init failed");
        return false;
    }
    s_pending_x = 0;
    s_pending_y = 0;
    vdp2_backend_reset_stats();
    s_inited = true;
    return true;
}

void vdp2_backend_shutdown(void)
{
    if (!s_inited) {
        return;
    }
    rpe_saturn_vdp2_shutdown();
    s_inited = false;
}

bool vdp2_backend_upload_background(const TextureAsset* texture)
{
    if (!s_inited || !valid_texture(texture)) {
        ++s_stats.upload_failures;
        return false;
    }
    rpe_saturn_vdp2_set_palette(VDP2_BACKGROUND_PALETTE, texture->palette);
    if (!rpe_saturn_vdp2_upload_background(texture->pixels, static_cast<uint16_t>(texture->width),
                                           static_cast<uint16_t>(texture->height))) {
        ++s_stats.upload_failures;
        return false;
    }
    ++s_stats.uploads;
    return true;
}

void vdp2_backend_set_scroll(fixed_t x, fixed_t y)
{
    s_pending_x = x;
    s_pending_y = y;
}

void vdp2_backend_commit_vblank(void)
{
    if (!s_inited) {
        return;
    }
    const int16_t x = fixed_to_pixels(s_pending_x);
    const int16_t y = fixed_to_pixels(s_pending_y);
    rpe_saturn_vdp2_set_scroll(x, y);
    s_stats.last_scroll_x = x;
    s_stats.last_scroll_y = y;
    ++s_stats.scroll_commits;
}

bool vdp2_backend_available(void)
{
    return s_inited && rpe_saturn_vdp2_available();
}

const Vdp2BackendStats& vdp2_backend_stats(void)
{
    return s_stats;
}

void vdp2_backend_reset_stats(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
}

}  // namespace saturn
