// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/render_saturn.cpp
//
// Render-backend Saturn (S23–24): VDP2 backdrop + каркас командного буфера.
// Публичные Render_* остаются в ядре (src/render/renderer.cpp), как на PS1;
// HAL реализует только внутренний hal/render_backend.h.
//
// VDP1 (полигоны, CLUT/8bpp, triangle→quad) — S27–28. До этого backend
// честно возвращает false на draw/texture/readback (silent drop запрещён),
// а BeginFrame/Clear меняют однотонный backdrop VDP2. Golden-цвет очистки:
// (0,0,32) → RGB555 0x9000; FNV-1a хэш кадра — saturn_render_utils.h.
#include "hal/render_backend.h"

#include "core/log/log.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/saturn_command_buffer.h"
#include "hal/saturn/saturn_hal_internal.h"
#include "hal/saturn/saturn_render_utils.h"

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
    s_draw_calls = 0u;
    s_inited = true;
    rpe_saturn_set_background_color(saturn::CLEAR_GOLDEN_WORD);
    return true;
}

void hal_render_backend_shutdown(void)
{
    saturn::command_buffer().reset();
    s_inited = false;
    s_draw_calls = 0u;
}

void hal_render_backend_set_camera(const Mat4* view, const Mat4* proj)
{
    (void)view;
    (void)proj;
    // VDP1-проекция — S27–28; skeleton не хранит камеру.
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
    hal_render_backend_clear(r, g, b);
}

void hal_render_backend_clear(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_inited) {
        return;
    }
    rpe_saturn_set_background_color(saturn::rgb888_to_saturn(r, g, b));
}

bool hal_render_backend_create_texture(TextureHandle handle, uint32_t w, uint32_t h,
                                       const uint8_t* pixels, const uint16_t* palette)
{
    (void)handle;
    (void)w;
    (void)h;
    (void)pixels;
    (void)palette;
    Log_Message(LOG_LEVEL_WARNING, "saturn_hal", "VDP1 texture: S27–28");
    return false;  // явный отказ, не silent drop
}

void hal_render_backend_destroy_texture(TextureHandle handle)
{
    (void)handle;
}

bool hal_render_backend_draw_triangle(const HalRenderBackendVertex vertices[3], const float* mvp,
                                      TextureHandle texture)
{
    (void)vertices;
    (void)mvp;
    (void)texture;
    Log_Message(LOG_LEVEL_WARNING, "saturn_hal", "VDP1 triangle: S27–28");
    return false;  // явный отказ, не silent drop
}

bool hal_render_backend_draw_overlay(const HalRenderBackendOverlayVertex* vertices, uint32_t count)
{
    (void)vertices;
    (void)count;
    Log_Message(LOG_LEVEL_WARNING, "saturn_hal", "VDP2 overlay: S27–28");
    return false;  // явный отказ, не silent drop
}

void hal_render_backend_end_frame(void)
{
    // VDP1/VDP2 sync/flip выполняет jo_core_run().
}

bool hal_render_backend_read_pixels(uint8_t* out_rgba, uint32_t buffer_bytes)
{
    (void)out_rgba;
    (void)buffer_bytes;
    return false;  // readback VDP1 — S27–28 (golden S23–24 — screenshot)
}

uint32_t hal_render_backend_draw_call_count(void)
{
    return s_draw_calls;
}
