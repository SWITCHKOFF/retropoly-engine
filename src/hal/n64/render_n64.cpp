// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/render_n64.cpp
//
// N64 render backend S43–44 (ADR-015): RSP T&L (transform) + RDP
// (rasterization, hardware Z, perspective-correct texturing, TMEM strips).
//
// Поток кадра:
//   draw_triangle → (split по TMEM-полосам текстуры) → RSP transform
//   (или CPU-fallback) → pending batch;
//   end_frame → rspq_wait → screen-space vertices → RDP triangles
//   (Z test/write, texture upload strip-by-strip) → detach_show.
//
// Контракты:
//   - переполнение TNL-батча/overlay → false + диагностика (без silent drop);
//   - TMEM: верхняя половина (2 KiB) — TLUT, полоса CI8 ≤2 KiB;
//   - Z-буфер: 320×240 RGBA16 в bridge, 18-bit internal RDP depth;
//   - CPU-fallback — только при недоступном RSP или явном toggle, с логом.
#include "hal/render_backend.h"

#include "core/log/log.h"
#include "hal/n64/n64_bridge.h"
#include "hal/n64/n64_memory.h"
#include "hal/n64/n64_render_bridge.h"
#include "hal/n64/rdp_command.h"
#include "hal/n64/n64_render_control.h"
#include "hal/n64/rsp_tnl.h"
#include "render/texture.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

namespace {

constexpr uint32_t N64_MAX_TEXTURES = RENDER_TEXTURE_MAX;
constexpr uint32_t N64_TEXTURE_MAX_DIM = 64u; // TMEM-политика S43–44
constexpr uint32_t N64_OVERLAY_VERTICES = 512u;
constexpr uint32_t N64_STRIP_MAX_KEY = N64_MAX_TEXTURES * 8u + 1u;

struct TextureSlot
{
    bool valid;
    uint32_t width;
    uint32_t height;
    uint8_t* pixels;
    uint16_t* palette;
};

struct PendingTriangle
{
    union
    {
        n64::TnlVertex tnl[3];   /**< RSP-путь: view-space + attrs. */
        RpeN64RdpVertex rdp[3];  /**< CPU-путь / после конверсии. */
    } data;
    uint32_t tnl_slot; /**< Индекс TNL-слота (RSP-путь). */
    uint16_t key;      /**< 0 — untextured; 1+handle*8+strip. */
    bool rsp;          /**< true — вершины в data.tnl, ждут RSP. */
};

bool s_backend_ready = false;
bool s_frame_valid = false;
bool s_force_cpu = false;
bool s_rsp_active = false;
bool s_fallback_logged = false;
Mat4 s_projection;
bool s_projection_valid = false;

uint32_t s_pending_count = 0u;
uint32_t s_draw_calls = 0u;
uint32_t s_tnl_failures_seen = 0u;
uint32_t s_overlay_count = 0u;
uint32_t s_last_tnl_us = 0u;
uint32_t s_last_rdp_us = 0u;
char s_text_overlay[512] = {};

PendingTriangle s_pending[RPE_N64_TNL_MAX_SLOTS];
RpeN64RdpVertex s_overlay[N64_OVERLAY_VERTICES];

alignas(16) uint8_t s_texture_pixels[N64_MAX_TEXTURES][N64_TEXTURE_MAX_DIM * N64_TEXTURE_MAX_DIM];
alignas(16) uint16_t s_texture_palette[N64_MAX_TEXTURES][256];
TextureSlot s_textures[N64_MAX_TEXTURES];

bool is_pow2(uint32_t value)
{
    return value != 0u && (value & (value - 1u)) == 0u;
}

TextureSlot* texture_slot(TextureHandle handle)
{
    if (handle == RENDER_TEXTURE_INVALID || handle > N64_MAX_TEXTURES) {
        return nullptr;
    }
    TextureSlot* slot = &s_textures[handle - 1u];
    return slot->valid ? slot : nullptr;
}

void reset_textures()
{
    memset(s_textures, 0, sizeof(s_textures));
    for (uint32_t i = 0u; i < N64_MAX_TEXTURES; ++i) {
        s_textures[i].pixels = s_texture_pixels[i];
        s_textures[i].palette = s_texture_palette[i];
    }
}

/** Явный отказ backend'а: лог + сброс кадра (без silent drop). */
bool fail_frame(const char* what)
{
    Log_Message(LOG_LEVEL_ERROR, "n64_render", "%s", what);
    s_frame_valid = false;
    return false;
}


bool screen_from_clip(const int32_t clip[4], const n64::TnlVertex& attr, bool textured,
                      uint32_t tex_w, uint32_t tex_h, RpeN64RdpVertex* out)
{
    n64::TnlScreenVertex projected;
    if (!n64::tnl_project_clip(clip, RENDER_FB_WIDTH, RENDER_FB_HEIGHT, &projected)) {
        return false;
    }
    out->x = projected.x;
    out->y = projected.y;
    out->z = projected.z;
    out->r = attr.r;
    out->g = attr.g;
    out->b = attr.b;
    out->a = attr.a;
    out->s = textured ? attr.u * (float)tex_w : 0.0f;
    out->t = textured ? attr.v * (float)tex_h : 0.0f;
    out->invw = projected.invw;
    return true;
}

/** Ключ группы: 0 — untextured; иначе 1 + handle*8 + strip. */
uint16_t make_key(TextureHandle handle, uint32_t strip)
{
    if (handle == RENDER_TEXTURE_INVALID) {
        return 0u;
    }
    return (uint16_t)(1u + (handle - 1u) * 8u + strip);
}

bool enqueue_rdp(const RpeN64RdpVertex v[3], uint16_t key)
{
    if (s_pending_count >= RPE_N64_TNL_MAX_SLOTS) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render",
                    "TNL batch overflow: %u triangles (key=%u)", (unsigned)s_pending_count,
                    (unsigned)key);
        return false;
    }
    PendingTriangle& pending = s_pending[s_pending_count];
    memcpy(pending.data.rdp, v, sizeof(pending.data.rdp));
    pending.tnl_slot = 0u;
    pending.key = key;
    pending.rsp = false;
    ++s_pending_count;
    return true;
}

bool enqueue_rsp(const n64::TnlVertex v[3], uint16_t key, const RpeN64TnlSlot* matrix)
{
    if (s_pending_count >= RPE_N64_TNL_MAX_SLOTS) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render",
                    "TNL batch overflow: %u triangles (key=%u)", (unsigned)s_pending_count,
                    (unsigned)key);
        return false;
    }

    const uint32_t index = s_pending_count;
    RpeN64TnlSlot* input = rpe_n64_tnl_input_slot(index);
    if (input == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "TNL input slot %u unavailable",
                    (unsigned)index);
        return false;
    }
    n64::tnl_vertices_to_slot(v, input);
    if (!rpe_n64_tnl_transform(index, matrix)) {        Log_Message(LOG_LEVEL_ERROR, "n64_render", "TNL transform enqueue failed (slot %u)",
                    (unsigned)index);
        return false;
    }

    PendingTriangle& pending = s_pending[index];
    memcpy(pending.data.tnl, v, sizeof(pending.data.tnl));
    pending.tnl_slot = index;
    pending.key = key;
    pending.rsp = true;
    ++s_pending_count;
    return true;
}

/** Одна группа (texture/strip или untextured): upload + draw. */
bool draw_group(uint16_t key)
{
    const bool textured = (key != 0u);
    TextureSlot* tex = nullptr;
    uint32_t strip = 0u;
    if (textured) {
        const uint32_t handle_index = (uint32_t)(key - 1u) / 8u;
        strip = (uint32_t)(key - 1u) % 8u;
        if (handle_index >= N64_MAX_TEXTURES) {
            return false;
        }
        tex = &s_textures[handle_index];
        if (!tex->valid) {
            Log_Message(LOG_LEVEL_ERROR, "n64_render", "texture handle %u invalid at draw",
                        (unsigned)(handle_index + 1u));
            return false;
        }
        const uint32_t strip_w = n64::tnl_texture_strip_width(tex->width, tex->height);
        if (strip_w == 0u) {
            return false;
        }
        const uint32_t sx = strip * strip_w;
        const uint32_t sw = (sx + strip_w <= tex->width) ? strip_w : (tex->width - sx);
        if (sw == 0u ||
            !rpe_n64_rdp_upload_texture_strip(tex->pixels, tex->palette, tex->width,
                                              tex->height, sx, sw)) {
            Log_Message(LOG_LEVEL_ERROR, "n64_render",
                        "TMEM strip upload failed: handle=%u strip=%u (%ux%u)",
                        (unsigned)(handle_index + 1u), (unsigned)strip,
                        (unsigned)tex->width, (unsigned)tex->height);
            return false;
        }
    }

    if (!rpe_n64_rdp_begin_group(textured)) {
        return false;
    }
    for (uint32_t i = 0u; i < s_pending_count; ++i) {
        PendingTriangle& pending = s_pending[i];
        if (pending.key != key || pending.rsp) {
            continue;
        }
        if (!rpe_n64_rdp_draw_triangle(pending.data.rdp, textured)) {
            return false;
        }
        ++s_draw_calls;
    }
    return true;
}

void reset_frame_state()
{
    s_pending_count = 0u;
    s_overlay_count = 0u;
    s_draw_calls = 0u;
    s_tnl_failures_seen = rpe_n64_tnl_failures();
}

}  // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

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
    if (!rpe_n64_render_init()) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "backend init: RSP/RDP bridge unavailable");
        return false;
    }

    reset_textures();
    n64::rdp_command_buffer().reset();
    s_backend_ready = true;
    s_frame_valid = false;
    s_force_cpu = false;
    s_projection_valid = false;
    s_rsp_active = rpe_n64_tnl_available();
    s_fallback_logged = false;
    reset_frame_state();

    Log_Message(LOG_LEVEL_INFO, "n64_render",
                "backend init: 320x240, Z RGBA16, %s, TNL slots=%u, TMEM texture=%u B",
                s_rsp_active ? "RSP T&L" : "CPU fallback (RSP unavailable)",
                (unsigned)rpe_n64_tnl_slot_capacity(),
                (unsigned)RPE_N64_TMEM_TEXTURE_BYTES);
    return true;
}

void hal_render_backend_shutdown(void)
{
    n64::rdp_command_buffer().reset();
    rpe_n64_render_shutdown();
    reset_textures();
    s_backend_ready = false;
    s_frame_valid = false;
    s_rsp_active = false;
    s_pending_count = 0u;
    s_overlay_count = 0u;
}

void hal_render_backend_set_camera(const Mat4* view, const Mat4* proj)
{
    (void)view;
    if (proj == nullptr) {
        s_projection_valid = false;
        return;
    }
    // Ядро подаёт view-space вершины + полный MVP; N64-бэкенду нужна
    // ТОЛЬКО проекция (иначе view применится дважды). View уже в вершинах.
    s_projection = *proj;
    s_projection_valid = true;
}

void hal_render_backend_begin_frame(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_backend_ready) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "begin_frame before backend init");
        return;
    }
    reset_frame_state();
    n64::rdp_command_buffer().reset();
    if (!n64::rdp_command_buffer().append(n64::RDP_OPCODE_BEGIN_FRAME, nullptr, 0u)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "begin_frame command rejected (%s)",
                    n64::RdpCommandBuffer::diag_string(n64::rdp_command_buffer().diag()));
        s_frame_valid = false;
        return;
    }
    if (!rpe_n64_frame_begin(r, g, b)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "VI frame begin failed");
        s_frame_valid = false;
        return;
    }
    s_frame_valid = true;
}

bool hal_render_backend_create_texture(TextureHandle handle, uint32_t w, uint32_t h,
                                       const uint8_t* pixels_ci8, const uint16_t* palette)
{
    if (handle == RENDER_TEXTURE_INVALID || handle > N64_MAX_TEXTURES || pixels_ci8 == nullptr ||
        palette == nullptr || w == 0u || h == 0u) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "create_texture: invalid arguments");
        return false;
    }
    if (!s_backend_ready) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "create_texture before backend init");
        return false;
    }

    uint32_t tw = (w > N64_TEXTURE_MAX_DIM) ? N64_TEXTURE_MAX_DIM : w;
    uint32_t th = (h > N64_TEXTURE_MAX_DIM) ? N64_TEXTURE_MAX_DIM : h;
    if (tw < 8u || !is_pow2(tw) || !is_pow2(th)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render",
                    "create_texture %ux%u: N64 requires pow2, 8..64 (target %ux%u)", w, h, tw,
                    th);
        return false;
    }

    TextureSlot& slot = s_textures[handle - 1u];
    if (slot.pixels == nullptr) {
        slot.pixels = s_texture_pixels[handle - 1u];
        slot.palette = s_texture_palette[handle - 1u];
    }
    // Nearest downsample (детерминированно; спрайт/UV остаются 0..1).
    for (uint32_t y = 0u; y < th; ++y) {
        const uint32_t sy = (h == th) ? y : (y * h) / th;
        for (uint32_t x = 0u; x < tw; ++x) {
            const uint32_t sx = (w == tw) ? x : (x * w) / tw;
            slot.pixels[y * tw + x] = pixels_ci8[sy * w + sx];
        }
    }
    memcpy(slot.palette, palette, 256u * sizeof(uint16_t));
    slot.width = tw;
    slot.height = th;
    slot.valid = true;

    const uint32_t strip = n64::tnl_texture_strip_width(tw, th);
    if (strip == 0u) {
        slot.valid = false;
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "create_texture: no TMEM strip for %ux%u",
                    tw, th);
        return false;
    }
    Log_Message(LOG_LEVEL_INFO, "n64_render",
                "texture %u: %ux%u → %ux%u CI8, strips=%u×%u, TMEM=%u B", (unsigned)handle, w,
                h, tw, th, (unsigned)strip, (unsigned)(tw / strip),
                (unsigned)n64::tnl_tmem_bytes(strip, th));
    return true;
}

void hal_render_backend_destroy_texture(TextureHandle handle)
{
    if (handle == RENDER_TEXTURE_INVALID || handle > N64_MAX_TEXTURES) {
        return;
    }
    TextureSlot& slot = s_textures[handle - 1u];
    slot.valid = false;
    slot.width = 0u;
    slot.height = 0u;
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
        s_frame_valid = false;
        return;
    }
    rpe_n64_frame_clear(r, g, b);
}

bool hal_render_backend_draw_triangle(const HalRenderBackendVertex vertices[3], const float* mvp_gl,
                                      TextureHandle texture)
{
    if (!s_frame_valid || vertices == nullptr || mvp_gl == nullptr) {
        return false;
    }

    float proj_gl[16];
    if (s_projection_valid) {
        for (uint32_t i = 0u; i < 16u; ++i) {
            proj_gl[i] = fixed_to_float(s_projection.m[i]);
        }
    } else {
        memcpy(proj_gl, mvp_gl, sizeof(proj_gl));
    }
    n64::TnlMatrix matrix;
    if (!n64::tnl_matrix_from_mvp_gl(proj_gl, &matrix)) {
        return fail_frame("mvp conversion failed");
    }
    RpeN64TnlSlot matrix_slot;
    n64::tnl_matrix_to_slot(matrix, &matrix_slot);

    n64::TnlTriangle triangle;
    for (uint32_t v = 0u; v < 3u; ++v) {
        triangle.v[v].x = vertices[v].x;
        triangle.v[v].y = vertices[v].y;
        triangle.v[v].z = vertices[v].z;
        triangle.v[v].u = vertices[v].u;
        triangle.v[v].v = vertices[v].v;
        triangle.v[v].r = vertices[v].r;
        triangle.v[v].g = vertices[v].g;
        triangle.v[v].b = vertices[v].b;
        triangle.v[v].a = vertices[v].a;
    }

    TextureSlot* tex = nullptr;
    uint32_t strip_count = 1u;
    uint32_t strip_w = 0u;
    if (texture != RENDER_TEXTURE_INVALID) {
        tex = texture_slot(texture);
        if (tex == nullptr) {
            Log_Message(LOG_LEVEL_ERROR, "n64_render", "draw_triangle: texture %u invalid",
                        (unsigned)texture);
            return false;
        }
        strip_w = n64::tnl_texture_strip_width(tex->width, tex->height);
        if (strip_w == 0u) {
            return false;
        }
        strip_count = (tex->width + strip_w - 1u) / strip_w;
        if (strip_count > 8u) {
            Log_Message(LOG_LEVEL_ERROR, "n64_render", "draw_triangle: strip_count=%u > 8",
                        (unsigned)strip_count);
            return false;
        }
    }

    n64::TnlTriangle parts[16];
    uint32_t part_count = 0u;
    if (tex == nullptr) {
        parts[part_count++] = triangle;
    } else {
        for (uint32_t strip = 0u; strip < strip_count; ++strip) {
            const uint32_t sx = strip * strip_w;
            const uint32_t sw = (sx + strip_w <= tex->width) ? strip_w : (tex->width - sx);
            const float u0 = (float)sx / (float)tex->width;
            const float u1 = (float)(sx + sw) / (float)tex->width;
            n64::TnlTriangle clipped[4];
            const uint32_t count = n64::tnl_clip_to_u_range(triangle, u0, u1, clipped);
            for (uint32_t i = 0u; i < count && part_count < 16u; ++i) {
                parts[part_count++] = clipped[i];
            }
        }
    }
    if (part_count == 0u) {
        return true; // Полностью вне текстуры (не должно случаться при 0..1 UV).
    }
    if (s_pending_count + part_count > RPE_N64_TNL_MAX_SLOTS) {
        return fail_frame("TNL batch overflow: pending+parts exceeds capacity");
    }

    for (uint32_t p = 0u; p < part_count; ++p) {
        uint32_t strip = 0u;
        uint16_t key = 0u;
        if (tex != nullptr) {
            // Полоса определяется по u-центру суб-треугольника (clip держит
            // суб-треугольник внутри одной полосы).
            const float u_mid =
                (parts[p].v[0].u + parts[p].v[1].u + parts[p].v[2].u) / 3.0f;
            strip = (uint32_t)(u_mid * (float)tex->width) / strip_w;
            if (strip >= strip_count) {
                strip = strip_count - 1u;
            }
            key = make_key(texture, strip);
        }

        bool ok = false;
        if (s_rsp_active && !s_force_cpu) {
            ok = enqueue_rsp(parts[p].v, key, &matrix_slot);
        } else {
            if (!s_fallback_logged) {
                Log_Message(LOG_LEVEL_WARNING, "n64_render",
                            "CPU T&L fallback active (RSP unavailable or forced)");
                s_fallback_logged = true;
            }
            int32_t clip[3][4];
            n64::tnl_transform_clip_cpu(matrix, parts[p].v, clip);
            RpeN64RdpVertex rdp[3];
            bool screen_ok = true;
            for (uint32_t v = 0u; v < 3u; ++v) {
                if (!screen_from_clip(clip[v], parts[p].v[v], tex != nullptr,
                                      tex ? tex->width : 0u, tex ? tex->height : 0u,
                                      &rdp[v])) {
                    screen_ok = false;
                    break;
                }
            }
            ok = screen_ok && enqueue_rdp(rdp, key);
        }
        if (!ok) {
            return fail_frame("triangle submission failed");
        }
    }
    return true;
}

bool hal_render_backend_draw_overlay(const HalRenderBackendOverlayVertex* vertices, uint32_t count)
{
    if (!s_frame_valid || vertices == nullptr || count < 3u || (count % 3u) != 0u) {
        return false;
    }
    if (s_overlay_count + count > N64_OVERLAY_VERTICES) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "overlay overflow: %u + %u > %u",
                    (unsigned)s_overlay_count, (unsigned)count,
                    (unsigned)N64_OVERLAY_VERTICES);
        return false;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        RpeN64RdpVertex& dst = s_overlay[s_overlay_count++];
        dst.x = (float)vertices[i].x;
        dst.y = (float)vertices[i].y;
        dst.z = 0.0f;
        dst.r = (float)vertices[i].r / 255.0f;
        dst.g = (float)vertices[i].g / 255.0f;
        dst.b = (float)vertices[i].b / 255.0f;
        dst.a = 1.0f;
        dst.s = 0.0f;
        dst.t = 0.0f;
        dst.invw = 0.0f;
    }
    return true;
}

void hal_render_backend_end_frame(void)
{
    if (!s_frame_valid) {
        return;
    }

    const bool used_rsp = s_rsp_active && !s_force_cpu;
    const uint64_t tnl_start = rpe_n64_ticks_us();
    if (used_rsp && s_pending_count != 0u) {
        rpe_n64_tnl_wait();
        const uint32_t failures = rpe_n64_tnl_failures();
        if (failures != s_tnl_failures_seen) {
            Log_Message(LOG_LEVEL_ERROR, "n64_render",
                        "RSP T&L failures: %u (total %u)", (unsigned)(failures - s_tnl_failures_seen),
                        (unsigned)failures);
            s_tnl_failures_seen = failures;
        }
        // Конверсия clip → screen для всех RSP-записей.
        for (uint32_t i = 0u; i < s_pending_count; ++i) {
            PendingTriangle& pending = s_pending[i];
            if (!pending.rsp) {
                continue;
            }
            TextureSlot* tex = nullptr;
            if (pending.key != 0u) {
                const uint32_t handle_index = (uint32_t)(pending.key - 1u) / 8u;
                if (handle_index < N64_MAX_TEXTURES && s_textures[handle_index].valid) {
                    tex = &s_textures[handle_index];
                } else {
                    Log_Message(LOG_LEVEL_ERROR, "n64_render",
                                "RSP triangle has invalid texture key %u", (unsigned)pending.key);
                    pending.key = 0u;
                }
            }
            RpeN64TnlSlot* output = rpe_n64_tnl_output_slot(pending.tnl_slot);
            if (output == nullptr) {
                pending.key = 0u;
                continue;
            }
            RpeN64RdpVertex rdp[3];
            bool ok = true;
            for (uint32_t v = 0u; v < 3u; ++v) {
                int32_t clip[4];
                for (uint32_t c = 0u; c < 4u; ++c) {
                    clip[c] = n64::tnl_slot_component(*output, v * 4u + c);
                }
                if (!screen_from_clip(clip, pending.data.tnl[v], tex != nullptr,
                                      tex ? tex->width : 0u, tex ? tex->height : 0u,
                                      &rdp[v])) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                const int32_t w0 = n64::tnl_slot_component(*output, 3u);
                const int32_t w1 = n64::tnl_slot_component(*output, 7u);
                const int32_t w2 = n64::tnl_slot_component(*output, 11u);
                Log_Message(LOG_LEVEL_ERROR, "n64_render",
                            "RSP triangle clip w<=0 (slot %u key=%u w=%ld/%ld/%ld z=%ld/%ld/%ld)",
                            (unsigned)pending.tnl_slot, (unsigned)pending.key, (long)w0, (long)w1,
                            (long)w2, (long)pending.data.tnl[0].z, (long)pending.data.tnl[1].z,
                            (long)pending.data.tnl[2].z);
                pending.key = 0u;
                continue;
            }
            memcpy(pending.data.rdp, rdp, sizeof(rdp));
            pending.rsp = false;
        }
    } else if (s_pending_count != 0u) {
        // CPU fallback: вершины уже сконвертированы в draw_triangle.
        for (uint32_t i = 0u; i < s_pending_count; ++i) {
            s_pending[i].rsp = false;
        }
    }

    s_last_tnl_us = (uint32_t)(rpe_n64_ticks_us() - tnl_start);
    const uint64_t rdp_start = rpe_n64_ticks_us();

    // Группировка по texture/strip (Z-буфер делает порядок несущественным).
    for (uint32_t key = 0u; key < N64_STRIP_MAX_KEY; ++key) {
        bool has = false;
        for (uint32_t i = 0u; i < s_pending_count; ++i) {
            if (s_pending[i].key == key && !s_pending[i].rsp) {
                has = true;
                break;
            }
        }
        if (has && !draw_group((uint16_t)key)) {
            Log_Message(LOG_LEVEL_ERROR, "n64_render", "draw group %u failed", (unsigned)key);
            s_frame_valid = false;
        }
    }

    if (s_text_overlay[0] != '\0' && rpe_n64_rdp_begin_overlay_mode()) {
        (void)rpe_n64_text_print(96, 88, s_text_overlay);
    }
    s_last_rdp_us = (uint32_t)(rpe_n64_ticks_us() - rdp_start);
    if (s_overlay_count != 0u) {
        if (!rpe_n64_rdp_draw_overlay(s_overlay, s_overlay_count)) {
            Log_Message(LOG_LEVEL_ERROR, "n64_render", "overlay draw failed");
        }
        s_overlay_count = 0u;
    }

    if (!n64::rdp_command_buffer().append(n64::RDP_OPCODE_END_FRAME, nullptr, 0u)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_render", "end_frame command rejected (%s)",
                    n64::RdpCommandBuffer::diag_string(n64::rdp_command_buffer().diag()));
    }
    rpe_n64_frame_end();
    s_frame_valid = false;
    s_pending_count = 0u;
}

bool hal_render_backend_read_pixels(uint8_t* dst_rgba8, uint32_t dst_size)
{
    return rpe_n64_frame_read_pixels(dst_rgba8, dst_size);
}

uint32_t hal_render_backend_draw_call_count(void)
{
    return s_draw_calls;
}

// ---------------------------------------------------------------------------
// Диагностика/тестовая ручка fallback (S43–44 manual gate)
// ---------------------------------------------------------------------------

void hal_render_backend_set_cpu_fallback(bool enabled)
{
    if (s_force_cpu == enabled) {
        return;
    }
    s_force_cpu = enabled;
    s_fallback_logged = false;
    Log_Message(LOG_LEVEL_INFO, "n64_render", "CPU T&L fallback %s",
                enabled ? "enabled" : "disabled");
}

bool hal_render_backend_cpu_fallback(void)
{
    return s_force_cpu || !s_rsp_active;
}

bool hal_render_backend_rsp_active(void)
{
    return s_rsp_active && !s_force_cpu;
}

uint32_t hal_render_backend_last_tnl_us(void)
{
    return s_last_tnl_us;
}

uint32_t hal_render_backend_last_rdp_us(void)
{
    return s_last_rdp_us;
}

void hal_render_backend_set_text_overlay(const char* text)
{
    if (text == nullptr) {
        s_text_overlay[0] = '\0';
        return;
    }
    const size_t length = strlen(text);
    const size_t copy = (length >= sizeof(s_text_overlay)) ? sizeof(s_text_overlay) - 1u : length;
    memcpy(s_text_overlay, text, copy);
    s_text_overlay[copy] = '\0';
}
