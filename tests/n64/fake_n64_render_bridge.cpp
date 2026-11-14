// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/fake_n64_render_bridge.cpp
//
// Host-двойник render-моста N64 (S43–44): TNL-слоты считаются CPU-эталоном,
// RDP-вызовы учитываются счётчиками, readback даёт детерминированный паттерн.
// Позволяет тестировать render_n64.cpp без libdragon/RSP/RDP.
#include "hal/n64/n64_render_bridge.h"

#include <string.h>

#include "hal/n64/rsp_tnl.h"

namespace {

RpeN64TnlSlot s_input[RPE_N64_TNL_MAX_SLOTS];
RpeN64TnlSlot s_output[RPE_N64_TNL_MAX_SLOTS];
bool s_ready = false;
bool s_tnl_ready = true;
uint32_t s_transform_calls = 0u;
uint32_t s_wait_calls = 0u;
uint32_t s_upload_calls = 0u;
uint32_t s_triangle_calls = 0u;
uint32_t s_overlay_calls = 0u;
uint32_t s_last_strip_sx = 0u;
uint32_t s_last_strip_sw = 0u;
uint32_t s_last_upload_bytes = 0u;
bool s_last_textured = false;
uint32_t s_ticks = 0u;

n64::TnlVertex slot_vertex(const RpeN64TnlSlot& slot, uint32_t v)
{
    n64::TnlVertex out = {};
    out.x = n64::tnl_fixed_to_float(n64::tnl_slot_component(slot, v * 4u + 0u));
    out.y = n64::tnl_fixed_to_float(n64::tnl_slot_component(slot, v * 4u + 1u));
    out.z = n64::tnl_fixed_to_float(n64::tnl_slot_component(slot, v * 4u + 2u));
    return out;
}

}  // namespace

extern "C" {

bool rpe_n64_render_init(void)
{
    s_ready = true;
    s_transform_calls = 0u;
    s_wait_calls = 0u;
    s_upload_calls = 0u;
    s_triangle_calls = 0u;
    s_overlay_calls = 0u;
    return true;
}

void rpe_n64_render_shutdown(void)
{
    s_ready = false;
}

bool rpe_n64_render_ready(void)
{
    return s_ready;
}

bool rpe_n64_tnl_available(void)
{
    return s_ready && s_tnl_ready;
}

uint32_t rpe_n64_tnl_slot_capacity(void)
{
    return s_ready ? RPE_N64_TNL_MAX_SLOTS : 0u;
}

RpeN64TnlSlot* rpe_n64_tnl_input_slot(uint32_t index)
{
    return (s_ready && index < RPE_N64_TNL_MAX_SLOTS) ? &s_input[index] : nullptr;
}

RpeN64TnlSlot* rpe_n64_tnl_output_slot(uint32_t index)
{
    return (s_ready && index < RPE_N64_TNL_MAX_SLOTS) ? &s_output[index] : nullptr;
}

bool rpe_n64_tnl_transform(uint32_t index, const RpeN64TnlSlot* matrix)
{
    if (!s_ready || matrix == nullptr || index >= RPE_N64_TNL_MAX_SLOTS) {
        return false;
    }
    n64::TnlMatrix mat = {};
    for (uint32_t i = 0u; i < 16u; ++i) {
        mat.m[i] = n64::tnl_slot_component(*matrix, i);
    }
    n64::TnlVertex verts[3] = {
        slot_vertex(s_input[index], 0u),
        slot_vertex(s_input[index], 1u),
        slot_vertex(s_input[index], 2u),
    };
    int32_t clip[3][4];
    n64::tnl_transform_clip_cpu(mat, verts, clip);
    for (uint32_t v = 0u; v < 3u; ++v) {
        for (uint32_t c = 0u; c < 4u; ++c) {
            const uint32_t bits = (uint32_t)clip[v][c];
            s_output[index].c[(v * 4u + c) / 8u].i[(v * 4u + c) % 8u] = (int16_t)(bits >> 16);
            s_output[index].c[(v * 4u + c) / 8u].f[(v * 4u + c) % 8u] = (uint16_t)(bits & 0xFFFFu);
        }
    }
    ++s_transform_calls;
    return true;
}

void rpe_n64_tnl_wait(void)
{
    ++s_wait_calls;
}

uint32_t rpe_n64_tnl_failures(void)
{
    return 0u;
}

bool rpe_n64_rdp_upload_texture_strip(const uint8_t* pixels_ci8, const uint16_t* palette,
                                      uint32_t width, uint32_t height, uint32_t sx,
                                      uint32_t sw)
{
    (void)pixels_ci8;
    (void)palette;
    (void)width;
    (void)height;
    if (sx >= width || sw == 0u || (sw * height) > RPE_N64_TMEM_TEXTURE_BYTES) {
        return false;
    }
    s_last_strip_sx = sx;
    s_last_strip_sw = sw;
    s_last_upload_bytes = sw * height + 512u;
    ++s_upload_calls;
    return true;
}

bool rpe_n64_rdp_begin_group(bool textured)
{
    s_last_textured = textured;
    return true;
}

bool rpe_n64_rdp_draw_triangle(const RpeN64RdpVertex v[3], bool textured)
{
    (void)v;
    (void)textured;
    ++s_triangle_calls;
    return true;
}

bool rpe_n64_rdp_begin_overlay_mode(void)
{
    return true;
}

bool rpe_n64_rdp_draw_overlay(const RpeN64RdpVertex* vertices, uint32_t count)
{
    (void)vertices;
    (void)count;
    ++s_overlay_calls;
    return true;
}

bool rpe_n64_frame_read_pixels(uint8_t* dst_rgba8, uint32_t dst_size)
{
    if (dst_rgba8 == nullptr || dst_size < 320u * 240u * 4u) {
        return false;
    }
    for (uint32_t i = 0u; i < 320u * 240u; ++i) {
        dst_rgba8[i * 4u + 0u] = (uint8_t)(i & 0xFFu);
        dst_rgba8[i * 4u + 1u] = (uint8_t)((i >> 8) & 0xFFu);
        dst_rgba8[i * 4u + 2u] = 0x40u;
        dst_rgba8[i * 4u + 3u] = 0xFFu;
    }
    return true;
}

uint64_t rpe_n64_ticks_us(void)
{
    return ++s_ticks;
}

// --- счётчики для тестов ---------------------------------------------------
uint32_t rpe_n64_fake_render_transform_calls(void)
{
    return s_transform_calls;
}
uint32_t rpe_n64_fake_render_wait_calls(void)
{
    return s_wait_calls;
}
uint32_t rpe_n64_fake_render_upload_calls(void)
{
    return s_upload_calls;
}
uint32_t rpe_n64_fake_render_triangle_calls(void)
{
    return s_triangle_calls;
}
uint32_t rpe_n64_fake_render_overlay_calls(void)
{
    return s_overlay_calls;
}
uint32_t rpe_n64_fake_render_last_strip_sx(void)
{
    return s_last_strip_sx;
}
uint32_t rpe_n64_fake_render_last_strip_sw(void)
{
    return s_last_strip_sw;
}
uint32_t rpe_n64_fake_render_last_upload_bytes(void)
{
    return s_last_upload_bytes;
}
bool rpe_n64_fake_render_last_textured(void)
{
    return s_last_textured;
}
void rpe_n64_fake_render_set_tnl_available(bool available)
{
    s_tnl_ready = available;
}
void rpe_n64_fake_render_reset(void)
{
    s_ready = false;
    s_tnl_ready = true;
    s_transform_calls = 0u;
    s_wait_calls = 0u;
    s_upload_calls = 0u;
    s_triangle_calls = 0u;
    s_overlay_calls = 0u;
    s_last_strip_sx = 0u;
    s_last_strip_sw = 0u;
    s_last_upload_bytes = 0u;
    s_last_textured = false;
    s_ticks = 0u;
}

}  // extern "C"
