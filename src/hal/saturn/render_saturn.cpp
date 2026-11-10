// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/render_saturn.cpp
//
// Render-backend Saturn (S21–22 skeleton): VDP2 backdrop-only.
// Публичные Render_* остаются в ядре (src/render/renderer.cpp), как на
// PS1; HAL реализует только внутренний hal/render_backend.h.
//
// VDP1 (полигоны, CLUT/8bpp, triangle→quad) — S27–28. До этого backend
// честно возвращает false на draw/texture/readback (silent drop запрещён),
// а BeginFrame/Clear меняют однотонный backdrop VDP2.
#include "hal/render_backend.h"

#include "core/log/log.h"
#include "hal/saturn/jo_saturn_bridge.h"

namespace {

constexpr uint32_t kWidth = 320u;
constexpr uint32_t kHeight = 224u;
constexpr uint16_t kDefaultBackdrop = static_cast<uint16_t>(RPE_SATURN_COLOR_RGB555 | (8u << 10));

bool s_inited = false;
uint32_t s_draw_calls = 0u;

uint16_t rgb888_to_saturn(uint8_t r, uint8_t g, uint8_t b)
{
    const uint16_t r5 = static_cast<uint16_t>((r / 8u) & 0x1Fu);
    const uint16_t g5 = static_cast<uint16_t>((g / 8u) & 0x1Fu);
    const uint16_t b5 = static_cast<uint16_t>((b / 8u) & 0x1Fu);
    return static_cast<uint16_t>(RPE_SATURN_COLOR_RGB555 | (b5 << 10) | (g5 << 5) | r5);
}

}  // namespace

bool hal_render_backend_init(uint32_t width, uint32_t height)
{
    if (width != kWidth || height != kHeight) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal",
                    "render backend: ожидается 320x224 (ADR-008), получено %ux%u",
                    static_cast<unsigned>(width), static_cast<unsigned>(height));
        return false;
    }
    rpe_saturn_set_background_color(kDefaultBackdrop);
    s_draw_calls = 0u;
    s_inited = true;
    return true;
}

void hal_render_backend_shutdown(void)
{
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
    s_draw_calls = 0u;
    hal_render_backend_clear(r, g, b);
}

void hal_render_backend_clear(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_inited) {
        return;
    }
    rpe_saturn_set_background_color(rgb888_to_saturn(r, g, b));
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

bool hal_render_backend_draw_triangle(const HalRenderBackendVertex vertices[3],
                                       const float* mvp, TextureHandle texture)
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
    return false;  // overlay на Saturn — после рендер-контракта S23–24
}

void hal_render_backend_end_frame(void)
{
    // VDP1/VDP2 sync/flip выполняет jo_core_run().
}

bool hal_render_backend_read_pixels(uint8_t* out_rgba, uint32_t buffer_bytes)
{
    (void)out_rgba;
    (void)buffer_bytes;
    return false;  // readback VDP1 — S27–28 (golden S23–24)
}

uint32_t hal_render_backend_draw_call_count(void)
{
    return s_draw_calls;
}
