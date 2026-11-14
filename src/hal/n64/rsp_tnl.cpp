// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/rsp_tnl.cpp
//
// Реализация чистой математики RSP T&L (контракт — rsp_tnl.h).
#include "hal/n64/rsp_tnl.h"

#include <math.h>
#include <string.h>

namespace n64 {

namespace {

constexpr float TNL_FIXED_SCALE = 65536.0f;
constexpr float TNL_FIXED_LIMIT = 32767.0f;

/** Ближайшая степень двойки ≤ value (value ≥ 1). */
uint32_t floor_pow2(uint32_t value)
{
    uint32_t result = 1u;
    while ((result << 1u) <= value) {
        result <<= 1u;
    }
    return result;
}

void slot_component_to_fixed(int32_t fixed, int16_t* out_i, uint16_t* out_f)
{
    const uint32_t bits = (uint32_t)fixed;
    *out_i = (int16_t)(bits >> 16);
    *out_f = (uint16_t)(bits & 0xFFFFu);
}

TnlVertex lerp_vertex(const TnlVertex& a, const TnlVertex& b, float t)
{
    TnlVertex out;
    out.x = a.x + (b.x - a.x) * t;
    out.y = a.y + (b.y - a.y) * t;
    out.z = a.z + (b.z - a.z) * t;
    out.u = a.u + (b.u - a.u) * t;
    out.v = a.v + (b.v - a.v) * t;
    out.r = a.r + (b.r - a.r) * t;
    out.g = a.g + (b.g - a.g) * t;
    out.b = a.b + (b.b - a.b) * t;
    out.a = a.a + (b.a - a.a) * t;
    return out;
}

/**
 * Sutherland–Hodgman: одна плоскость u >= bound (keep_ge) или u <= bound.
 * @return число вершин полигона (≤ in_count + 1).
 */
uint32_t clip_plane_u(const TnlVertex* in, uint32_t in_count, bool keep_ge, float bound,
                      TnlVertex* out)
{
    uint32_t count = 0u;
    for (uint32_t i = 0u; i < in_count; ++i) {
        const TnlVertex& a = in[i];
        const TnlVertex& b = in[(i + 1u) % in_count];
        const bool a_inside = keep_ge ? (a.u >= bound) : (a.u <= bound);
        const bool b_inside = keep_ge ? (b.u >= bound) : (b.u <= bound);
        if (a_inside) {
            out[count++] = a;
        }
        if (a_inside != b_inside) {
            float denom = b.u - a.u;
            float t = (denom != 0.0f) ? ((bound - a.u) / denom) : 0.0f;
            if (t < 0.0f) {
                t = 0.0f;
            } else if (t > 1.0f) {
                t = 1.0f;
            }
            out[count++] = lerp_vertex(a, b, t);
        }
    }
    return count;
}

}  // namespace

float tnl_fixed_to_float(int32_t value)
{
    return (float)value / TNL_FIXED_SCALE;
}

int32_t tnl_float_to_fixed(float value)
{
    // Сатурация до представимого Q16.16; границы int32.
    if (value > TNL_FIXED_LIMIT) {
        value = TNL_FIXED_LIMIT;
    } else if (value < -TNL_FIXED_LIMIT) {
        value = -TNL_FIXED_LIMIT;
    }
    return (int32_t)lroundf(value * TNL_FIXED_SCALE);
}

bool tnl_matrix_from_mvp_gl(const float* mvp_gl, TnlMatrix* out)
{
    if (mvp_gl == nullptr || out == nullptr) {
        return false;
    }
    // GL-матрица уже column-major: RSP-микрокод трактует 4 группы по 4
    // компоненты как столбцы и вычисляет M·v (см. rsp_tnl.S). Транспонирование
    // НЕ выполняется.
    for (uint32_t i = 0u; i < 16u; ++i) {
        const float value = mvp_gl[i];
        if (!(value == value)) { // NaN
            return false;
        }
        if (value > TNL_FIXED_LIMIT || value < -TNL_FIXED_LIMIT) {
            return false;
        }
        out->m[i] = (int32_t)lroundf(value * TNL_FIXED_SCALE);
    }
    return true;
}

void tnl_matrix_to_slot(const TnlMatrix& matrix, RpeN64TnlSlot* out)
{
    if (out == nullptr) {
        return;
    }
    memset(out, 0, sizeof(*out));
    // RSP-микрокод умножает 4 группы компонент (столбцы GL) на x/y/z/w.
    // Компоненты идут в порядке GL column-major, без транспонирования.
    for (uint32_t i = 0u; i < 16u; ++i) {
        slot_component_to_fixed(matrix.m[i], &out->c[i / 8u].i[i % 8u],
                               &out->c[i / 8u].f[i % 8u]);
    }
}

void tnl_vertices_to_slot(const TnlVertex v[3], RpeN64TnlSlot* out)
{
    if (v == nullptr || out == nullptr) {
        return;
    }
    memset(out, 0, sizeof(*out));
    int32_t components[16] = {};
    for (uint32_t k = 0u; k < 3u; ++k) {
        components[k * 4u + 0u] = tnl_float_to_fixed(v[k].x);
        components[k * 4u + 1u] = tnl_float_to_fixed(v[k].y);
        components[k * 4u + 2u] = tnl_float_to_fixed(v[k].z);
        components[k * 4u + 3u] = TNL_ONE;
    }
    for (uint32_t i = 0u; i < 16u; ++i) {
        slot_component_to_fixed(components[i], &out->c[i / 8u].i[i % 8u],
                               &out->c[i / 8u].f[i % 8u]);
    }
}

void tnl_vec4_to_slot(const float v[4], RpeN64TnlSlot* out)
{
    if (v == nullptr || out == nullptr) {
        return;
    }
    memset(out, 0, sizeof(*out));
    for (uint32_t i = 0u; i < 4u; ++i) {
        const int32_t fixed = tnl_float_to_fixed(v[i]);
        slot_component_to_fixed(fixed, &out->c[0].i[i], &out->c[0].f[i]);
    }
}

int32_t tnl_slot_component(const RpeN64TnlSlot& slot, uint32_t index)
{
    if (index >= 16u) {
        return 0;
    }
    const RpeN64TnlSlotHalf& half = slot.c[index / 8u];
    const uint32_t bits = ((uint32_t)(uint16_t)half.i[index % 8u] << 16) | half.f[index % 8u];
    return (int32_t)bits;
}

void tnl_transform_clip_cpu(const TnlMatrix& matrix, const TnlVertex v[3], int32_t out_clip[3][4])
{
    if (v == nullptr || out_clip == nullptr) {
        return;
    }
    // RSP: lane j = Σ_i m[i*4+j]·v[i] (i — группа компонент/столбец GL).
    for (uint32_t k = 0u; k < 3u; ++k) {
        const int32_t in[4] = {tnl_float_to_fixed(v[k].x), tnl_float_to_fixed(v[k].y),
                               tnl_float_to_fixed(v[k].z), TNL_ONE};
        for (uint32_t j = 0u; j < 4u; ++j) {
            int64_t sum = 0;
            for (uint32_t i = 0u; i < 4u; ++i) {
                sum += (int64_t)matrix.m[i * 4u + j] * in[i];
            }
            out_clip[k][j] = (int32_t)(sum >> 16);
        }
    }
}

bool tnl_project_clip(const int32_t clip[4], uint32_t width, uint32_t height,
                      TnlScreenVertex* out)
{
    if (clip == nullptr || out == nullptr || width == 0u || height == 0u || clip[3] <= 0) {
        return false;
    }
    // clip[w] — Q16.16; 1/w_actual = 65536 / w_fixed.
    const float invw = 65536.0f / (float)clip[3];
    const float ndc_x = (float)clip[0] * invw / 65536.0f;
    const float ndc_y = (float)clip[1] * invw / 65536.0f;
    const float ndc_z = (float)clip[2] * invw / 65536.0f;

    out->x = (ndc_x * 0.5f + 0.5f) * (float)width;
    out->y = (0.5f - ndc_y * 0.5f) * (float)height;
    float z = ndc_z * 0.5f + 0.5f;
    if (z < 0.0f) {
        z = 0.0f;
    } else if (z > 1.0f) {
        z = 1.0f;
    }
    out->z = z;
    out->invw = invw;
    return true;
}

uint32_t tnl_texture_strip_width(uint32_t width, uint32_t height)
{
    if (width == 0u || height == 0u || height > 256u) {
        return 0u;
    }
    // CI8: 1 байт/texel; RDP TMEM для CI8 — 2 KiB (верхняя половина под TLUT).
    uint32_t max_pixels = RPE_N64_TMEM_TEXTURE_BYTES / height;
    if (max_pixels > RPE_N64_TEX_TILE_MAX) {
        max_pixels = RPE_N64_TEX_TILE_MAX;
    }
    if (max_pixels > width) {
        max_pixels = width;
    }
    if (max_pixels < 8u) {
        return 0u;
    }
    // RDP-текстуры и upload-полосы — степень двойки (tile mask/wrap).
    return floor_pow2(max_pixels);
}

uint32_t tnl_texture_strip_count(uint32_t width, uint32_t height)
{
    const uint32_t strip = tnl_texture_strip_width(width, height);
    if (strip == 0u) {
        return 0u;
    }
    return (width + strip - 1u) / strip;
}

uint32_t tnl_tmem_bytes(uint32_t strip_width, uint32_t height)
{
    // 256-entry TLUT (CI8) = 512 B постоянно в верхней половине TMEM.
    return strip_width * height + 512u;
}

uint32_t tnl_clip_to_u_range(const TnlTriangle& in, float u0, float u1, TnlTriangle out[4])
{
    if (out == nullptr || !(u1 > u0)) {
        return 0u;
    }

    TnlVertex poly0[4];
    TnlVertex poly1[5];
    // Сначала u >= u0, затем u <= u1.
    const uint32_t count0 = clip_plane_u(in.v, 3u, true, u0, poly0);
    if (count0 < 3u) {
        return 0u;
    }
    const uint32_t count1 = clip_plane_u(poly0, count0, false, u1, poly1);
    if (count1 < 3u) {
        return 0u;
    }

    // Веер: треугольник + до 2 дополнительных (выпуклый полигон ≤ 5 вершин).
    uint32_t tri_count = 0u;
    for (uint32_t i = 1u; i + 1u < count1 && tri_count < 4u; ++i) {
        out[tri_count].v[0] = poly1[0];
        out[tri_count].v[1] = poly1[i];
        out[tri_count].v[2] = poly1[i + 1u];
        ++tri_count;
    }
    return tri_count;
}

uint32_t tnl_fnv1a32(const void* data, size_t size)
{
    if (data == nullptr || size == 0u) {
        return 2166136261u;
    }
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 2166136261u;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

void tnl_light_diffuse_cpu(const float normal[3], const float light_dir[3],
                           const uint8_t light_color[3], int32_t out_q16[3])
{
    if (normal == nullptr || light_dir == nullptr || light_color == nullptr || out_q16 == nullptr) {
        return;
    }
    const int32_t nx = tnl_float_to_fixed(normal[0]);
    const int32_t ny = tnl_float_to_fixed(normal[1]);
    const int32_t nz = tnl_float_to_fixed(normal[2]);
    const int32_t lx = tnl_float_to_fixed(light_dir[0]);
    const int32_t ly = tnl_float_to_fixed(light_dir[1]);
    const int32_t lz = tnl_float_to_fixed(light_dir[2]);
    int64_t dot = ((int64_t)nx * lx + (int64_t)ny * ly + (int64_t)nz * lz) >> 16;
    if (dot < 0) {
        dot = 0;
    }
    for (uint32_t c = 0u; c < 3u; ++c) {
        out_q16[c] = (int32_t)(dot * light_color[c]);
    }
}

void tnl_light_cpu(const float normal[3], const float light_dir[3], const uint8_t ambient[3],
                   const uint8_t light_color[3], uint8_t out_rgb[3])
{
    if (normal == nullptr || light_dir == nullptr || ambient == nullptr ||
        light_color == nullptr || out_rgb == nullptr) {
        return;
    }
    float diffuse = normal[0] * light_dir[0] + normal[1] * light_dir[1] +
                    normal[2] * light_dir[2];
    if (diffuse < 0.0f) {
        diffuse = 0.0f;
    } else if (diffuse > 1.0f) {
        diffuse = 1.0f;
    }
    for (uint32_t c = 0u; c < 3u; ++c) {
        float value = (float)ambient[c] + (float)light_color[c] * diffuse;
        if (value < 0.0f) {
            value = 0.0f;
        } else if (value > 255.0f) {
            value = 255.0f;
        }
        out_rgb[c] = (uint8_t)lroundf(value);
    }
}

}  // namespace n64
