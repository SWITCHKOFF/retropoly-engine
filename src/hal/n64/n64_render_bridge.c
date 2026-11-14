// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_render_bridge.c
//
// libdragon/RSPQ/RDPQ-реализация render-моста (Спринт 43–44).
// Единственный render-TU, включающий libdragon: RSP T&L overlay, RDP
// triangle stream, TMEM-полосы текстур, Z-буфер и framebuffer readback.
#include "hal/n64/n64_render_bridge.h"

#include <libdragon.h>

#include <string.h>

#include "hal/n64/n64_bridge.h"

// rsp_tnl.S → rsp_tnl.text.o/.data.o (CMake повторяет правило n64.mk).
// DEFINE_RSP_UCODE использует zero-size массивы (границы секции) и
// неконстантный initializer — это контракт libdragon, не наш код.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
DEFINE_RSP_UCODE(rpe_tnl);
#pragma GCC diagnostic pop

#define RPE_N64_TNL_CMD_TRANSFORM 0u

static bool s_render_ready = false;
static uint32_t s_tnl_overlay_id = 0u;
static bool s_tnl_ready = false;
static uint32_t s_tnl_failures = 0u;

static RpeN64TnlSlot s_tnl_input[RPE_N64_TNL_MAX_SLOTS] __attribute__((aligned(16)));
static RpeN64TnlSlot s_tnl_output[RPE_N64_TNL_MAX_SLOTS] __attribute__((aligned(16)));
static RpeN64TnlSlot s_tnl_matrix[RPE_N64_TNL_MAX_SLOTS] __attribute__((aligned(16)));

static uint16_t s_tlut[256] __attribute__((aligned(8)));

// ---------------------------------------------------------------------------
// T&L
// ---------------------------------------------------------------------------

static void tnl_flush_input(uint32_t index)
{
    data_cache_hit_writeback(&s_tnl_input[index], sizeof(s_tnl_input[index]));
}

bool rpe_n64_render_init(void)
{
    if (s_render_ready) {
        return true;
    }
    if (!rpe_n64_core_ready()) {
        return false;
    }

    // rdpq_init() уже вызвал rspq_init(); повторный вызов идемпотентен.
    rspq_init();
    s_tnl_overlay_id = rspq_overlay_register(&rpe_tnl);
    s_tnl_ready = (s_tnl_overlay_id != 0u);
    s_tnl_failures = 0u;
    memset(s_tnl_input, 0, sizeof(s_tnl_input));
    memset(s_tnl_output, 0, sizeof(s_tnl_output));
    memset(s_tnl_matrix, 0, sizeof(s_tnl_matrix));

    if (s_tnl_ready) {
        debugf("[INFO][n64_tnl] RSP T&L overlay registered id=%lu\n",
               (unsigned long)(s_tnl_overlay_id >> 28));
    } else {
        debugf("[ERROR][n64_tnl] RSP T&L overlay registration failed\n");
    }

    s_render_ready = true;
    return true;
}

void rpe_n64_render_shutdown(void)
{
    if (!s_render_ready) {
        return;
    }
    rspq_wait();
    if (s_tnl_ready) {
        rspq_overlay_unregister(s_tnl_overlay_id);
        s_tnl_ready = false;
        s_tnl_overlay_id = 0u;
    }
    rspq_wait();
    s_render_ready = false;
}

bool rpe_n64_render_ready(void)
{
    return s_render_ready;
}

bool rpe_n64_tnl_available(void)
{
    return s_render_ready && s_tnl_ready;
}

uint32_t rpe_n64_tnl_slot_capacity(void)
{
    return s_render_ready ? RPE_N64_TNL_MAX_SLOTS : 0u;
}

RpeN64TnlSlot* rpe_n64_tnl_input_slot(uint32_t index)
{
    if (!s_tnl_ready || index >= RPE_N64_TNL_MAX_SLOTS) {
        ++s_tnl_failures;
        return NULL;
    }
    return &s_tnl_input[index];
}

RpeN64TnlSlot* rpe_n64_tnl_output_slot(uint32_t index)
{
    if (!s_tnl_ready || index >= RPE_N64_TNL_MAX_SLOTS) {
        ++s_tnl_failures;
        return NULL;
    }
    return &s_tnl_output[index];
}

bool rpe_n64_tnl_transform(uint32_t index, const RpeN64TnlSlot* matrix)
{
    if (!s_tnl_ready || index >= RPE_N64_TNL_MAX_SLOTS || matrix == NULL) {
        ++s_tnl_failures;
        return false;
    }
    tnl_flush_input(index);

    // Матрица — per-slot RDRAM-буфер: CPU не перезаписывает её до выполнения
    // команды (в отличие от одного общего DMEM-буфера).
    memcpy(&s_tnl_matrix[index], matrix, sizeof(s_tnl_matrix[index]));
    data_cache_hit_writeback(&s_tnl_matrix[index], sizeof(s_tnl_matrix[index]));
    rspq_write(s_tnl_overlay_id, RPE_N64_TNL_CMD_TRANSFORM,
               PhysicalAddr(&s_tnl_input[index]) & 0xFFFFFF,
               PhysicalAddr(&s_tnl_matrix[index]) & 0xFFFFFF,
               PhysicalAddr(&s_tnl_output[index]) & 0xFFFFFF);
    return true;
}

void rpe_n64_tnl_wait(void)
{
    if (!s_tnl_ready) {
        return;
    }
    rspq_wait();
    // RSP-DMA писал в RDRAM мимо D-cache: сбросить грязные линии перед
    // чтением output-слотов CPU.
    data_cache_hit_invalidate(s_tnl_output, sizeof(s_tnl_output));
}

uint32_t rpe_n64_tnl_failures(void)
{
    return s_tnl_failures;
}

// ---------------------------------------------------------------------------
// RDP: texture strips / triangles / overlay
// ---------------------------------------------------------------------------

static uint16_t ps1_palette_to_rgba16(uint16_t packed)
{
    // PS1 RGB555+STP → N64 RGBA5551. STP — semi-transparency PS1; MVP
    // прозрачность выключена, поэтому alpha=1 (opaque) для обоих значений.
    const uint16_t r5 = (uint16_t)(packed & 0x1Fu);
    const uint16_t g5 = (uint16_t)((packed >> 5) & 0x1Fu);
    const uint16_t b5 = (uint16_t)((packed >> 10) & 0x1Fu);
    return (uint16_t)((r5 << 11) | (g5 << 6) | (b5 << 1) | 1u);
}

bool rpe_n64_rdp_upload_texture_strip(const uint8_t* pixels_ci8, const uint16_t* palette,
                                      uint32_t width, uint32_t height, uint32_t sx,
                                      uint32_t sw)
{
    if (!s_render_ready || pixels_ci8 == NULL || palette == NULL || width == 0u ||
        height == 0u || sw == 0u || sx >= width || (sx + sw) > width ||
        (sw * height) > RPE_N64_TMEM_TEXTURE_BYTES) {
        return false;
    }

    for (uint32_t i = 0u; i < 256u; ++i) {
        s_tlut[i] = ps1_palette_to_rgba16(palette[i]);
    }
    // RDP читает TMEM-источники через DMA из RDRAM: сбросить кэш CPU.
    data_cache_hit_writeback(s_tlut, sizeof(s_tlut));
    data_cache_hit_writeback((volatile void*)pixels_ci8, width * height);

    rdpq_tex_upload_tlut(s_tlut, 0, 256);

    surface_t tex = surface_make_linear((void*)pixels_ci8, FMT_CI8, (uint16_t)width,
                                        (uint16_t)height);
    const int used = rdpq_tex_upload_sub(TILE0, &tex, NULL, (int)sx, 0, (int)(sx + sw),
                                         (int)height);
    return used > 0 && (uint32_t)used <= RPE_N64_TMEM_TEXTURE_BYTES;
}

bool rpe_n64_rdp_begin_group(bool textured)
{
    if (!s_render_ready) {
        return false;
    }
    rdpq_set_mode_standard();
    rdpq_mode_zbuf(true, true);
    if (textured) {
        rdpq_mode_combiner(RDPQ_COMBINER_TEX_SHADE);
        rdpq_mode_persp(true);
        rdpq_mode_tlut(TLUT_RGBA16);
        // N64 policy: hardware bilinear (отличие от PS1 nearest; docs/N64.md).
        rdpq_mode_filter(FILTER_BILINEAR);
    } else {
        rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
        rdpq_mode_persp(false);
        rdpq_mode_tlut(TLUT_NONE);
        rdpq_mode_filter(FILTER_POINT);
    }
    return true;
}

bool rpe_n64_rdp_draw_triangle(const RpeN64RdpVertex v[3], bool textured)
{
    if (!s_render_ready || v == NULL) {
        return false;
    }
    if (textured) {
        rdpq_triangle(&TRIFMT_ZBUF_SHADE_TEX, (const float*)&v[0], (const float*)&v[1],
                      (const float*)&v[2]);
    } else {
        rdpq_triangle(&TRIFMT_ZBUF_SHADE, (const float*)&v[0], (const float*)&v[1],
                      (const float*)&v[2]);
    }
    return true;
}

bool rpe_n64_rdp_begin_overlay_mode(void)
{
    if (!s_render_ready) {
        return false;
    }
    rdpq_set_mode_standard();
    rdpq_mode_zbuf(false, false);
    rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
    rdpq_mode_persp(false);
    rdpq_mode_tlut(TLUT_NONE);
    rdpq_mode_filter(FILTER_POINT);
    return true;
}

bool rpe_n64_rdp_draw_overlay(const RpeN64RdpVertex* vertices, uint32_t count)
{
    if (!s_render_ready || vertices == NULL || count < 3u || (count % 3u) != 0u) {
        return false;
    }
    if (!rpe_n64_rdp_begin_overlay_mode()) {
        return false;
    }

    for (uint32_t i = 0u; i + 2u < count; i += 3u) {
        float out[3][6];
        for (uint32_t k = 0u; k < 3u; ++k) {
            const RpeN64RdpVertex* src = &vertices[i + k];
            out[k][0] = src->x;
            out[k][1] = src->y;
            out[k][2] = src->r;
            out[k][3] = src->g;
            out[k][4] = src->b;
            out[k][5] = src->a;
        }
        rdpq_triangle(&TRIFMT_SHADE, out[0], out[1], out[2]);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Framebuffer readback / timing
// ---------------------------------------------------------------------------

bool rpe_n64_frame_read_pixels(uint8_t* dst_rgba8, uint32_t dst_size)
{
    if (dst_rgba8 == NULL) {
        return false;
    }
    uint32_t stride = 0u;
    uint32_t width = 0u;
    uint32_t height = 0u;
    const void* buffer = rpe_n64_frame_color_buffer(&stride, &width, &height);
    if (buffer == NULL || width == 0u || height == 0u ||
        dst_size < width * height * 4u) {
        return false;
    }

    const uint8_t* src = (const uint8_t*)buffer;
    for (uint32_t y = 0u; y < height; ++y) {
        const uint16_t* row = (const uint16_t*)(src + (size_t)y * stride);
        uint8_t* out = dst_rgba8 + (size_t)y * width * 4u;
        for (uint32_t x = 0u; x < width; ++x) {
            const uint16_t p = row[x];
            const uint8_t r5 = (uint8_t)((p >> 11) & 0x1Fu);
            const uint8_t g5 = (uint8_t)((p >> 6) & 0x1Fu);
            const uint8_t b5 = (uint8_t)((p >> 1) & 0x1Fu);
            out[x * 4u + 0u] = (uint8_t)((r5 << 3) | (r5 >> 2));
            out[x * 4u + 1u] = (uint8_t)((g5 << 3) | (g5 >> 2));
            out[x * 4u + 2u] = (uint8_t)((b5 << 3) | (b5 >> 2));
            out[x * 4u + 3u] = 0xFFu;
        }
    }
    return true;
}

uint64_t rpe_n64_ticks_us(void)
{
    return get_ticks_us();
}
