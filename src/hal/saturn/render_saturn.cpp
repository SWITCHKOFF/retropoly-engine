// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/render_saturn.cpp
//
// Render-backend Saturn: HAL-lifecycle + VDP2 backdrop/clear; VDP1-полигоны,
// текстуры/CLUT и Gouraud — vdp1_backend.cpp (Спринт 27–28).
//
// Публичные Render_* остаются в ядре (src/render/renderer.cpp); HAL реализует
// только внутренний hal/render_backend.h. Golden-цвет очистки:
// (0,0,32) → RGB555 0x9000; FNV-1a хэш кадра — saturn_render_utils.h.
#include "hal/render_backend.h"

#include "core/log/log.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/saturn_command_buffer.h"
#include "hal/saturn/saturn_hal_internal.h"
#include "hal/saturn/saturn_render_utils.h"
#include "hal/saturn/vdp1_backend.h"
#include "hal/saturn/vdp2_backend.h"

namespace {

constexpr uint32_t kWidth = saturn::SCREEN_WIDTH;
constexpr uint32_t kHeight = saturn::SCREEN_HEIGHT;

bool s_inited = false;
uint32_t s_draw_calls = 0u;

}  // namespace

bool hal_render_backend_init(uint32_t width, uint32_t height)
{
    if (!saturn::platform_ready()) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal",
                    "render backend init before Platform_Init (lifecycle violation)");
        return false;
    }
    if (s_inited) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "render backend double init");
        return false;
    }
    if (width != kWidth || height != kHeight) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal",
                    "render backend: ожидается 320x224 (ADR-008), получено %ux%u",
                    static_cast<unsigned>(width), static_cast<unsigned>(height));
        return false;
    }

    saturn::command_buffer().reset();
    if (!saturn::vdp1_backend_init()) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "VDP1 backend init failed");
        return false;
    }
    s_draw_calls = 0u;
    s_inited = true;
    rpe_saturn_set_background_color(saturn::CLEAR_GOLDEN_WORD);
    rpe_saturn_vdp1_set_erase_color(saturn::CLEAR_GOLDEN_WORD);
    return true;
}

void hal_render_backend_shutdown(void)
{
    saturn::vdp1_backend_shutdown();
    saturn::command_buffer().reset();
    s_inited = false;
    s_draw_calls = 0u;
}

void hal_render_backend_set_camera(const Mat4* view, const Mat4* proj)
{
    saturn::vdp1_backend_set_camera(view, proj);
}

void hal_render_backend_begin_frame(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_inited) {
        Log_Message(LOG_LEVEL_WARNING, "saturn_hal",
                    "begin_frame before render backend init (ignored)");
        return;
    }
    s_draw_calls = 0u;
    saturn::command_buffer().reset();
    const uint16_t color = saturn::rgb888_to_saturn(r, g, b);
    // При активном VDP2-фоне VDP1 framebuffer стирается в transparent,
    // иначе он непрозрачно закрывает NBG1.
    const uint16_t erase = saturn::vdp2_backend_available() ? 0u : color;
    saturn::vdp1_backend_begin_frame(erase);
    rpe_saturn_set_background_color(color);
}

void hal_render_backend_clear(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_inited) {
        return;
    }
    const uint16_t color = saturn::rgb888_to_saturn(r, g, b);
    rpe_saturn_set_background_color(color);
    rpe_saturn_vdp1_set_erase_color(saturn::vdp2_backend_available() ? 0u : color);
}

bool hal_render_backend_create_texture(TextureHandle handle, uint32_t w, uint32_t h,
                                       const uint8_t* pixels, const uint16_t* palette)
{
    if (!s_inited) {
        return false;
    }
    return saturn::vdp1_backend_create_texture(handle, w, h, pixels, palette);
}

void hal_render_backend_destroy_texture(TextureHandle handle)
{
    saturn::vdp1_backend_destroy_texture(handle);
}

bool hal_render_backend_draw_triangle(const HalRenderBackendVertex vertices[3], const float* mvp,
                                      TextureHandle texture)
{
    if (!s_inited) {
        return false;
    }
    return saturn::vdp1_backend_draw_triangle(vertices, mvp, texture);
}

bool hal_render_backend_draw_overlay(const HalRenderBackendOverlayVertex* vertices, uint32_t count)
{
    if (!s_inited) {
        return false;
    }
    return saturn::vdp1_backend_draw_overlay(vertices, count);
}

void hal_render_backend_end_frame(void)
{
    if (!s_inited) {
        return;
    }
    saturn::vdp1_backend_end_frame();
    s_draw_calls = saturn::vdp1_backend_stats().commands_last;
}

bool hal_render_backend_read_pixels(uint8_t* out_rgba, uint32_t buffer_bytes)
{
    (void)out_rgba;
    (void)buffer_bytes;
    // RGBA8-снимок 320×240 (307 KiB static) на Saturn не держим: golden
    // VDP1 — rpe_saturn_vdp1_fb_hash() (FNV по 320×224 RGB555) и ручной
    // скриншот эмулятора (docs/SATURN.md §12). Явный отказ, не silent drop.
    return false;
}

uint32_t hal_render_backend_draw_call_count(void)
{
    return s_draw_calls;
}
