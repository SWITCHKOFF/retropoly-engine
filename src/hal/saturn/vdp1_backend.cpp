// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp1_backend.cpp
//
// VDP1-путь (контракты — заголовок, ADR-010).
#include "hal/saturn/vdp1_backend.h"

#include "core/log/log.h"
#include "core/math/mat4.h"
#include "core/math/vec4.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/saturn_render_utils.h"
#include "hal/saturn/triangle_to_quad.h"
#include "hal/saturn/vdp1_texture.h"

#include <string.h>

namespace saturn {

namespace {

constexpr uint32_t kScreenWidth = 320u;
constexpr uint32_t kScreenHeight = 224u;
constexpr uint32_t kCropTop = 8u;  // reference 320×240 → Saturn 320×224

bool s_inited = false;
bool s_camera_set = false;
Mat4 s_proj;

uint8_t s_gouraud_parity = 0u;
uint32_t s_gouraud_used[2] = {0u, 0u};
uint64_t s_gouraud_keys[2][VDP1_GOURAUD_SET_SLOTS];

Vdp1BackendStats s_stats;

uint8_t attr_u8_from_float01(float value)
{
    const fixed_t v = fixed_from_float(value);
    const int64_t scaled = (static_cast<int64_t>(v) * 255 + 32768) >> 16;
    if (scaled <= 0) {
        return 0u;
    }
    if (scaled >= 255) {
        return 255u;
    }
    return static_cast<uint8_t>(scaled);
}

fixed_t clamp_unit(fixed_t value)
{
    if (value < 0) {
        return 0;
    }
    if (value > FIXED_ONE) {
        return FIXED_ONE;
    }
    return value;
}

/**
 * Проекция view-space вершины в экран Saturn 320×224 (y вниз, crop 8).
 *
 * @return false, если w ≤ 0 (точка за камерой; ядро такие уже отбросило).
 */
bool project_vertex(const HalRenderBackendVertex& src, TriQuadVertex* out)
{
    const Vec4 clip = mat4_transform(&s_proj, Vec4{fixed_from_float(src.x), fixed_from_float(src.y),
                                                   fixed_from_float(src.z), FIXED_ONE});
    if (clip.w <= 0) {
        return false;
    }

    const fixed_t ndc_x = fixed_div(clip.x, clip.w);
    const fixed_t ndc_y = fixed_div(clip.y, clip.w);

    // sx = ndc_x·160 + 160; sy_ref = 120 − ndc_y·120; Saturn — центральные
    // 224 строки reference-кадра (ADR-008 §4): sy = sy_ref − 8.
    out->x = fixed_add(fixed_mul(ndc_x, fixed_from_int(160)), fixed_from_int(160));
    out->y = fixed_sub(fixed_from_int(120), fixed_mul(ndc_y, fixed_from_int(120)));
    out->y = fixed_sub(out->y, fixed_from_int(static_cast<int32_t>(kCropTop)));
    out->u = clamp_unit(fixed_from_float(src.u));
    out->v = clamp_unit(fixed_from_float(src.v));
    out->r = attr_u8_from_float01(src.r);
    out->g = attr_u8_from_float01(src.g);
    out->b = attr_u8_from_float01(src.b);
    out->a = attr_u8_from_float01(src.a);
    return true;
}

bool quad_inside_screen(const Vdp1Quad& quad)
{
    for (uint32_t i = 0u; i < 4u; ++i) {
        if (quad.x[i] < 0 || quad.x[i] >= static_cast<int16_t>(kScreenWidth) || quad.y[i] < 0 ||
            quad.y[i] >= static_cast<int16_t>(kScreenHeight)) {
            return false;
        }
    }
    return true;
}

/** Один канал RGB555: 16 — нейтральный (текстура/базовый цвет не меняется). */
uint16_t gouraud_channel_offset(uint16_t base, uint16_t color, uint32_t shift)
{
    const int32_t b = static_cast<int32_t>((base >> shift) & 0x1Fu);
    const int32_t c = static_cast<int32_t>((color >> shift) & 0x1Fu);
    int32_t v = 16 + c - b;
    if (v < 0) {
        v = 0;
    } else if (v > 31) {
        v = 31;
    }
    return static_cast<uint16_t>(static_cast<uint16_t>(v) << shift);
}

/**
 * VDP1 Gouraud-таблица — не абсолютные цвета, а аддитивные смещения
 * (16 = нейтраль; см. Jo Engine model GOUR и Mednafen GourauderTheTerrible).
 *
 * Для flat-полигона база — цвет первой вершины, поэтому таблица даёт
 * абсолютные цвета всех четырёх углов. Для текстуры база — белый: таблица
 * работает как per-vertex тинт/освещение, не уничтожая текстуру.
 */
uint16_t gouraud_offset(uint16_t base, uint16_t color)
{
    return static_cast<uint16_t>(gouraud_channel_offset(base, color, 0u) |
                                 gouraud_channel_offset(base, color, 5u) |
                                 gouraud_channel_offset(base, color, 10u));
}

/** Площадь quad'а (абсолютная, 2×area/2); для degenerate quad = площадь треугольника. */
uint32_t quad_area_pixels(const Vdp1Quad& quad)
{
    int64_t area2 = 0;
    for (uint32_t i = 0u; i < 4u; ++i) {
        const uint32_t j = (i + 1u) & 3u;
        area2 += static_cast<int64_t>(quad.x[i]) * quad.y[j] -
                 static_cast<int64_t>(quad.x[j]) * quad.y[i];
    }
    if (area2 < 0) {
        area2 = -area2;
    }
    return static_cast<uint32_t>(area2 / 2);
}

uint64_t gouraud_key(const uint16_t colors[4])
{
    return static_cast<uint64_t>(colors[0]) | (static_cast<uint64_t>(colors[1]) << 16) |
           (static_cast<uint64_t>(colors[2]) << 32) | (static_cast<uint64_t>(colors[3]) << 48);
}

/**
 * Выделение Gouraud-таблицы текущего кадра (с дедупликацией по 4 цветам).
 *
 * @param colors 4 × RGB555 без MSB.
 * @param out_slot Выход: абсолютный индекс таблицы (для grda).
 * @return false — набор кадра исчерпан (32 уникальных таблицы).
 */
bool allocate_gouraud(const uint16_t colors[4], uint16_t* out_slot)
{
    const uint8_t parity = s_gouraud_parity;
    const uint64_t key = gouraud_key(colors);
    for (uint32_t i = 0u; i < s_gouraud_used[parity]; ++i) {
        if (s_gouraud_keys[parity][i] == key) {
            *out_slot = static_cast<uint16_t>(parity * VDP1_GOURAUD_SET_SLOTS + i);
            return true;
        }
    }
    if (s_gouraud_used[parity] >= VDP1_GOURAUD_SET_SLOTS) {
        return false;
    }

    const uint32_t index = s_gouraud_used[parity]++;
    s_gouraud_keys[parity][index] = key;
    const uint16_t slot = static_cast<uint16_t>(parity * VDP1_GOURAUD_SET_SLOTS + index);
    rpe_saturn_vdp1_write_gouraud(slot, colors);
    *out_slot = slot;
    return true;
}

bool emit_quad(const Vdp1Quad& quad, const Vdp1Texture* texture)
{
    Vdp1Command cmd;
    memset(&cmd, 0, sizeof(cmd));

    uint16_t pmod = VDP1_PMOD_END_CODE_DISABLE | VDP1_PMOD_TRANSPARENT_DISABLE;
    uint16_t grda = 0u;
    uint16_t gouraud_slot = 0u;
    const uint16_t gouraud_base = (texture != nullptr) ? 0x7FFFu : quad.colors[0];
    uint16_t gouraud_colors[4];
    for (uint32_t i = 0u; i < 4u; ++i) {
        gouraud_colors[i] = gouraud_offset(gouraud_base, quad.colors[i]);
    }
    const bool gouraud_ok = allocate_gouraud(gouraud_colors, &gouraud_slot);
    if (gouraud_ok) {
        pmod |= VDP1_PMOD_GOURAUD;
        grda = static_cast<uint16_t>((VDP1_GOURAUD_BASE + gouraud_slot * 8u) / 8u);
    } else {
        ++s_stats.gouraud_fallbacks;
    }

    if (quad_inside_screen(quad)) {
        pmod |= VDP1_PMOD_PRE_CLIPPING_DISABLE;
    }

    if (texture != nullptr) {
        // Проверка source-прямоугольника: u0 кратна 8, width кратна 8,
        // прямоугольник не выходит за pattern.
        if ((quad.u0 & 7u) != 0u || (quad.width & 7u) != 0u || quad.width == 0u ||
            quad.height == 0u || static_cast<uint32_t>(quad.u0) + quad.width > texture->width ||
            static_cast<uint32_t>(quad.v0) + quad.height > texture->height) {
            Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1: source rect вне текстуры");
            return false;
        }

        cmd.ctrl = VDP1_CMD_DISTORTED_SPRITE;
        cmd.pmod = static_cast<uint16_t>(pmod | VDP1_PMOD_COLOR_MODE_256);
        cmd.colr = static_cast<uint16_t>(texture->clut * 256u);
        const uint32_t source_offset = static_cast<uint32_t>(quad.v0) * texture->stride + quad.u0;
        const uint32_t source_address =
            static_cast<uint32_t>(texture->src_addr8) + source_offset / 8u;
        if (source_address > 0xFFFFu) {
            Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1: texture address overflow");
            return false;
        }
        cmd.srca = static_cast<uint16_t>(source_address);
        cmd.size = static_cast<uint16_t>(((quad.width / 8u) << 8) | quad.height);
    } else {
        cmd.ctrl = VDP1_CMD_POLYGON;
        cmd.pmod = static_cast<uint16_t>(pmod | VDP1_PMOD_COLOR_MODE_RGB);
        cmd.colr = static_cast<uint16_t>(quad.colors[0] | 0x8000u);
    }

    cmd.grda = grda;
    cmd.xa = quad.x[0];
    cmd.ya = quad.y[0];
    cmd.xb = quad.x[1];
    cmd.yb = quad.y[1];
    cmd.xc = quad.x[2];
    cmd.yc = quad.y[2];
    cmd.xd = quad.x[3];
    cmd.yd = quad.y[3];
    cmd.link = 0u;
    cmd.reserved = 0u;

    if (!vdp1_command_list().append(cmd)) {
        ++s_stats.overflows;
        return false;
    }
    s_stats.pixels_last += quad_area_pixels(quad);
    return true;
}

}  // namespace

bool vdp1_backend_init(void)
{
    if (s_inited) {
        return true;
    }
    if (!rpe_saturn_vdp1_init()) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1: core init failed");
        return false;
    }
    if (!vdp1_command_list().inited()) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1: command arena не подключена");
        return false;
    }

    vdp1_textures().init();
    vdp1_command_list().reset();
    s_camera_set = false;
    memset(&s_proj, 0, sizeof(s_proj));
    s_gouraud_parity = 0u;
    s_gouraud_used[0] = 0u;
    s_gouraud_used[1] = 0u;
    vdp1_backend_reset_stats();
    s_inited = true;
    return true;
}

void vdp1_backend_shutdown(void)
{
    if (!s_inited) {
        return;
    }
    vdp1_textures().shutdown();
    rpe_saturn_vdp1_shutdown();
    vdp1_command_list().reset();
    s_camera_set = false;
    s_inited = false;
}

void vdp1_backend_set_camera(const Mat4* view, const Mat4* proj)
{
    (void)view;  // view уже применён ядром: вершины приходят view-space.
    if (proj == nullptr) {
        s_camera_set = false;
        return;
    }
    s_proj = *proj;
    s_camera_set = true;
}

void vdp1_backend_begin_frame(uint16_t erase_color)
{
    if (!s_inited) {
        return;
    }
    vdp1_command_list().reset();
    s_stats.pixels_last = 0u;
    s_gouraud_parity ^= 1u;
    s_gouraud_used[s_gouraud_parity] = 0u;
    rpe_saturn_vdp1_begin_frame();
    rpe_saturn_vdp1_set_erase_color(erase_color);
}

bool vdp1_backend_create_texture(TextureHandle handle, uint32_t width, uint32_t height,
                                 const uint8_t* pixels, const uint16_t* palette)
{
    if (!s_inited) {
        return false;
    }
    const bool ok = vdp1_textures().create(handle, width, height, pixels, palette);
    s_stats.texture_clamps = vdp1_textures().height_clamped_count();
    if (!ok) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1 texture create failed: %s",
                    Vdp1TextureRegistry::diag_string(vdp1_textures().diag()));
    }
    return ok;
}

void vdp1_backend_destroy_texture(TextureHandle handle)
{
    if (!s_inited) {
        return;
    }
    (void)vdp1_textures().destroy(handle);
    s_stats.texture_clamps = vdp1_textures().height_clamped_count();
}

bool vdp1_backend_draw_triangle(const HalRenderBackendVertex vertices[3], const float* mvp,
                                TextureHandle texture)
{
    (void)mvp;  // Saturn, как PS1: проекция из set_camera, mvp — PC-only.

    if (!s_inited || vertices == nullptr) {
        return false;
    }
    if (!s_camera_set) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1 draw before SetCamera");
        return false;
    }

    const Vdp1Texture* tex = nullptr;
    if (texture != RENDER_TEXTURE_INVALID) {
        tex = vdp1_textures().find(texture);
        if (tex == nullptr) {
            Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1: текстура %u не существует",
                        static_cast<unsigned>(texture));
            return false;
        }
    }

    TriQuadInput in;
    memset(&in, 0, sizeof(in));
    for (uint32_t i = 0u; i < 3u; ++i) {
        if (!project_vertex(vertices[i], &in.v[i])) {
            ++s_stats.screen_rejects;
            return true;  // за камерой: ядро уже отбросило, это не ошибка кадра
        }
    }
    in.texture_width = (tex != nullptr) ? tex->width : 0u;
    in.texture_height = (tex != nullptr) ? tex->height : 0u;

    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_OK;
    if (!triangle_to_quad(&in, &quad, &diag)) {
        if (diag == TRI_QUAD_OUT_OF_SCREEN) {
            ++s_stats.screen_rejects;
            return true;
        }
        if (diag == TRI_QUAD_DEGENERATE) {
            ++s_stats.degenerate;
            return true;
        }
        if (diag == TRI_QUAD_UNSUPPORTED_UV && tex != nullptr) {
            // Явная деградация: UV не в углах текстуры → flat-Gouraud.
            ++s_stats.uv_fallbacks;
            in.texture_width = 0u;
            in.texture_height = 0u;
            tex = nullptr;
            if (!triangle_to_quad(&in, &quad, &diag)) {
                if (diag == TRI_QUAD_OUT_OF_SCREEN) {
                    ++s_stats.screen_rejects;
                } else {
                    ++s_stats.degenerate;
                }
                return true;
            }
        } else {
            Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1 triangle reject: %s",
                        tri_quad_diag_string(diag));
            return false;
        }
    }

    if (!emit_quad(quad, tex)) {
        return false;
    }
    ++s_stats.submitted;
    if (tex != nullptr) {
        ++s_stats.textured;
    } else {
        ++s_stats.flat;
    }
    return true;
}

bool vdp1_backend_draw_overlay(const HalRenderBackendOverlayVertex* vertices, uint32_t count)
{
    if (!s_inited || vertices == nullptr || count == 0u || (count % 3u) != 0u) {
        return false;
    }

    for (uint32_t base = 0u; base < count; base += 3u) {
        TriQuadInput in;
        memset(&in, 0, sizeof(in));
        for (uint32_t i = 0u; i < 3u; ++i) {
            const HalRenderBackendOverlayVertex& v = vertices[base + i];
            in.v[i].x = fixed_from_int(v.x);
            in.v[i].y = fixed_from_int(static_cast<int32_t>(v.y) - static_cast<int32_t>(kCropTop));
            in.v[i].u = 0;
            in.v[i].v = 0;
            in.v[i].r = v.r;
            in.v[i].g = v.g;
            in.v[i].b = v.b;
            in.v[i].a = 255u;
        }
        in.texture_width = 0u;
        in.texture_height = 0u;

        Vdp1Quad quad;
        TriQuadDiag diag = TRI_QUAD_OK;
        if (!triangle_to_quad(&in, &quad, &diag)) {
            if (diag == TRI_QUAD_OUT_OF_SCREEN) {
                ++s_stats.screen_rejects;
                continue;
            }
            if (diag == TRI_QUAD_DEGENERATE) {
                ++s_stats.degenerate;
                continue;
            }
            return false;
        }
        if (!emit_quad(quad, nullptr)) {
            return false;
        }
        ++s_stats.submitted;
        ++s_stats.flat;
    }
    return true;
}

void vdp1_backend_end_frame(void)
{
    if (!s_inited) {
        return;
    }
    if (!vdp1_command_list().finalize(0u)) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1: command list finalize failed");
        return;
    }

    const uint32_t count = vdp1_command_list().count();
    for (uint32_t i = 0u; i < count; ++i) {
        const Vdp1Command* cmd = vdp1_command_list().command(i);
        if (cmd == nullptr) {
            break;
        }
        // Ядро уже отсортировало треугольники far→near. Первая поданная
        // команда получает наибольший ключ и рисуется первой; tie-breaker
        // (одинаковый ключ) сохраняет порядок подачи.
        const uint16_t sort_key = static_cast<uint16_t>(count - 1u - i);
        if (!rpe_saturn_vdp1_add_command(sort_key, reinterpret_cast<const RpeVdp1Command*>(cmd))) {
            ++s_stats.pipeline_drops;
            ++s_stats.overflows;
            Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1: pipeline command table full");
            break;
        }
    }
    s_stats.commands_last = count;
    s_stats.gouraud_tables_last = s_gouraud_used[s_gouraud_parity];
    s_stats.texture_clamps = vdp1_textures().height_clamped_count();
}

const Vdp1BackendStats& vdp1_backend_stats(void)
{
    return s_stats;
}

void vdp1_backend_reset_stats(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
}

}  // namespace saturn
