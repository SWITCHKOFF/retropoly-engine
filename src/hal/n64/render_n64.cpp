// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/render_n64.cpp
//
// N64 render backend — skeleton S41–42 (ADR-014): VI 320×240, clear color,
// BeginFrame/EndFrame через C-мост и bounded RDP-командную арену.
// RSP/RDP-геометрия, TMEM/текстуры, аппаратный Z и engine-overlay —
// S43–44. Все bool-методы рисования возвращают false + диагностику
// (контракт «без silent drop»), ядро рендера фиксирует
// RENDER_DIAG_BACKEND_FAIL.
#include "hal/render_backend.h"

#include "core/log/log.h"
#include "hal/n64/n64_bridge.h"
#include "hal/n64/n64_memory.h"
#include "hal/n64/rdp_command.h"

#include <stddef.h>

namespace {

bool s_backend_ready = false;
bool s_frame_valid = false;
bool s_draw_warned = false;

void clear_frame_error()
{
    s_frame_valid = false;
}

}  // namespace

bool hal_render_backend_init(uint32_t width, uint32_t height)
{
    if (width != RENDER_FB_WIDTH || height != RENDER_FB_HEIGHT) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render",
                    "backend init rejected: %ux%u (N64 reference %ux%u)",
                    static_cast<unsigned>(width), static_cast<unsigned>(height),
                    (unsigned)RENDER_FB_WIDTH, (unsigned)RENDER_FB_HEIGHT);
        return false;
    }
    if (!rpe_n64_render_available()) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "backend init: display/VI not available");
        return false;
    }

    n64::rdp_command_buffer().reset();
    s_backend_ready = true;
    s_frame_valid = false;
    s_draw_warned = false;
    return true;
}

void hal_render_backend_shutdown(void)
{
    n64::rdp_command_buffer().reset();
    s_backend_ready = false;
    s_frame_valid = false;
}

void hal_render_backend_set_camera(const Mat4* view, const Mat4* proj)
{
    (void)view;
    (void)proj;
}

void hal_render_backend_begin_frame(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_backend_ready) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "begin_frame before backend init");
        return;
    }
    n64::rdp_command_buffer().reset();
    if (!n64::rdp_command_buffer().append(n64::RDP_OPCODE_BEGIN_FRAME, nullptr, 0u)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "begin_frame command rejected (%s)",
                    n64::RdpCommandBuffer::diag_string(n64::rdp_command_buffer().diag()));
        clear_frame_error();
        return;
    }
    if (!rpe_n64_frame_begin(r, g, b)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "VI frame begin failed");
        clear_frame_error();
        return;
    }
    s_frame_valid = true;
}

bool hal_render_backend_create_texture(TextureHandle handle, uint32_t w, uint32_t h,
                                       const uint8_t* pixels_ci8, const uint16_t* palette)
{
    (void)handle;
    (void)w;
    (void)h;
    (void)pixels_ci8;
    (void)palette;
    if (!s_draw_warned) {
        Log_Message(LOG_LEVEL_WARNING, "n64_render",
                    "textures/TMEM not implemented until S43–44 (explicit reject)");
        s_draw_warned = true;
    }
    return false;
}

void hal_render_backend_destroy_texture(TextureHandle handle)
{
    (void)handle;
}

void hal_render_backend_clear(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_frame_valid) {
        Log_Message(LOG_LEVEL_WARNING, "n64_render", "clear outside active frame");
        return;
    }
    const uint8_t rgb[3] = {r, g, b};
    if (!n64::rdp_command_buffer().append(n64::RDP_OPCODE_CLEAR, rgb, sizeof(rgb))) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "clear command rejected (%s)",
                    n64::RdpCommandBuffer::diag_string(n64::rdp_command_buffer().diag()));
        clear_frame_error();
        return;
    }
    rpe_n64_frame_clear(r, g, b);
}

bool hal_render_backend_draw_triangle(const HalRenderBackendVertex vertices[3], const float* mvp_gl,
                                      TextureHandle texture)
{
    (void)vertices;
    (void)mvp_gl;
    (void)texture;
    if (!s_draw_warned) {
        Log_Message(LOG_LEVEL_WARNING, "n64_render",
                    "RSP/RDP triangles not implemented until S43–44 (explicit reject)");
        s_draw_warned = true;
    }
    return false;
}

bool hal_render_backend_draw_overlay(const HalRenderBackendOverlayVertex* vertices, uint32_t count)
{
    (void)vertices;
    (void)count;
    return false;
}

void hal_render_backend_end_frame(void)
{
    if (!s_frame_valid) {
        return;
    }
    if (!n64::rdp_command_buffer().append(n64::RDP_OPCODE_END_FRAME, nullptr, 0u)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "end_frame command rejected (%s)",
                    n64::RdpCommandBuffer::diag_string(n64::rdp_command_buffer().diag()));
    }
    rpe_n64_frame_end();
    s_frame_valid = false;
}

bool hal_render_backend_read_pixels(uint8_t* dst_rgba8, uint32_t dst_size)
{
    (void)dst_rgba8;
    (void)dst_size;
    return false;  // Readback появится вместе с RDP-бэкендом.
}

uint32_t hal_render_backend_draw_call_count(void)
{
    return 0u;
}
