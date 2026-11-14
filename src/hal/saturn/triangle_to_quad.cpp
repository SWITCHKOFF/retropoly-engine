// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/triangle_to_quad.cpp
//
// Реализация политики triangle→quad (контракты — заголовок, ADR-010).
#include "hal/saturn/triangle_to_quad.h"

#include "hal/saturn/saturn_render_utils.h"

namespace saturn {

namespace {

constexpr int32_t kCornerTL = 0;

struct CornerUV
{
    fixed_t u;
    fixed_t v;
};

int32_t floor_fixed_to_int(fixed_t v)
{
    const int32_t whole = fixed_to_int(v);
    // fixed_to_int усекает к нулю; для отрицательных остаток делает floor
    // на единицу меньше.
    if (v < 0 && static_cast<int64_t>(whole) * FIXED_ONE != v) {
        return whole - 1;
    }
    return whole;
}

int32_t ceil_fixed_to_int(fixed_t v)
{
    const int32_t whole = fixed_to_int(v);
    if (v > 0 && static_cast<int64_t>(whole) * FIXED_ONE != v) {
        return whole + 1;
    }
    return whole;
}

uint16_t pack_gouraud_color(const TriQuadVertex& v)
{
    return static_cast<uint16_t>(rgb888_to_saturn(v.r, v.g, v.b) & 0x7FFFu);
}

int32_t clamp_int32(int64_t v)
{
    if (v > INT32_MAX) {
        return INT32_MAX;
    }
    if (v < INT32_MIN) {
        return INT32_MIN;
    }
    return static_cast<int32_t>(v);
}

int16_t clamp_int16(int32_t v)
{
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(v);
}

/** @return Индекс ближайшего угла и квадрат расстояния (в fixed²). */
int32_t nearest_corner(fixed_t u, fixed_t v, const CornerUV corners[4], int64_t* out_distance)
{
    int32_t best = kCornerTL;
    int64_t best_distance = INT64_MAX;
    for (int32_t c = 0; c < 4; ++c) {
        const int64_t du = static_cast<int64_t>(u) - static_cast<int64_t>(corners[c].u);
        const int64_t dv = static_cast<int64_t>(v) - static_cast<int64_t>(corners[c].v);
        const int64_t distance = du * du + dv * dv;
        if (distance < best_distance) {
            best_distance = distance;
            best = c;
        }
    }
    if (out_distance != nullptr) {
        *out_distance = best_distance;
    }
    return best;
}

bool points_all_outside(int32_t min_x, int32_t max_x, int32_t min_y, int32_t max_y)
{
    return (max_x < 0) || (min_x > 319) || (max_y < 0) || (min_y > 223);
}

}  // namespace

int32_t tri_quad_round_fixed(fixed_t v)
{
    const int64_t scale = FIXED_ONE;
    const int64_t bias = (v >= 0) ? (scale / 2) : -(scale / 2);
    return clamp_int32((static_cast<int64_t>(v) + bias) / scale);
}

bool triangle_to_quad(const TriQuadInput* in, Vdp1Quad* out, TriQuadDiag* diag)
{
    if (in == nullptr || out == nullptr) {
        if (diag != nullptr) {
            *diag = TRI_QUAD_NULL_ARGUMENT;
        }
        return false;
    }
    *out = Vdp1Quad{};

    const bool textured = (in->texture_width != 0u);
    if (textured) {
        if (in->texture_width > VDP1_TEXTURE_MAX_DIM || in->texture_height > VDP1_TEXTURE_MAX_DIM ||
            in->texture_width < 8u || in->texture_height < 1u || (in->texture_width & 7u) != 0u) {
            if (diag != nullptr) {
                *diag = TRI_QUAD_INVALID_TEXTURE;
            }
            return false;
        }
    }

    if (!textured) {
        // Нетекстурированный путь: VDP1 polygon, вырожденный quad v0,v1,v2,v2.
        int16_t xs[4];
        int16_t ys[4];
        uint16_t colors[4];
        for (uint32_t i = 0u; i < 4u; ++i) {
            const uint32_t src = (i < 3u) ? i : 2u;
            xs[i] = clamp_int16(tri_quad_round_fixed(in->v[src].x));
            ys[i] = clamp_int16(tri_quad_round_fixed(in->v[src].y));
            colors[i] = pack_gouraud_color(in->v[src]);
        }

        int32_t min_x = xs[0], max_x = xs[0], min_y = ys[0], max_y = ys[0];
        for (uint32_t i = 1u; i < 4u; ++i) {
            min_x = (xs[i] < min_x) ? xs[i] : min_x;
            max_x = (xs[i] > max_x) ? xs[i] : max_x;
            min_y = (ys[i] < min_y) ? ys[i] : min_y;
            max_y = (ys[i] > max_y) ? ys[i] : max_y;
        }
        if (points_all_outside(min_x, max_x, min_y, max_y)) {
            if (diag != nullptr) {
                *diag = TRI_QUAD_OUT_OF_SCREEN;
            }
            return false;
        }

        const int64_t area2 =
            static_cast<int64_t>(xs[0]) * ys[1] - static_cast<int64_t>(xs[1]) * ys[0] +
            static_cast<int64_t>(xs[1]) * ys[2] - static_cast<int64_t>(xs[2]) * ys[1] +
            static_cast<int64_t>(xs[2]) * ys[0] - static_cast<int64_t>(xs[0]) * ys[2];
        if (area2 == 0) {
            if (diag != nullptr) {
                *diag = TRI_QUAD_DEGENERATE;
            }
            return false;
        }

        for (uint32_t i = 0u; i < 4u; ++i) {
            out->x[i] = xs[i];
            out->y[i] = ys[i];
            out->colors[i] = colors[i];
        }
        out->textured = false;
        if (diag != nullptr) {
            *diag = TRI_QUAD_OK;
        }
        return true;
    }

    const fixed_t tex_w = fixed_from_int(static_cast<int32_t>(in->texture_width));
    const uint32_t texture_height = (in->texture_height > VDP1_PATTERN_MAX_HEIGHT)
                                        ? VDP1_PATTERN_MAX_HEIGHT
                                        : in->texture_height;
    const fixed_t tex_h = fixed_from_int(static_cast<int32_t>(texture_height));

    fixed_t u_tex[3];
    fixed_t v_tex[3];
    for (uint32_t i = 0u; i < 3u; ++i) {
        u_tex[i] = fixed_mul(in->v[i].u, tex_w);
        v_tex[i] = fixed_mul(in->v[i].v, tex_h);
        if (u_tex[i] < 0) {
            u_tex[i] = 0;
        }
        if (u_tex[i] > tex_w) {
            u_tex[i] = tex_w;
        }
        if (v_tex[i] < 0) {
            v_tex[i] = 0;
        }
        if (v_tex[i] > tex_h) {
            v_tex[i] = tex_h;
        }
    }

    // Исходный прямоугольник: bbox UV, X — кратно 8, Y — 1 тексель.
    fixed_t u_min = u_tex[0], u_max = u_tex[0];
    fixed_t v_min = v_tex[0], v_max = v_tex[0];
    for (uint32_t i = 1u; i < 3u; ++i) {
        u_min = (u_tex[i] < u_min) ? u_tex[i] : u_min;
        u_max = (u_tex[i] > u_max) ? u_tex[i] : u_max;
        v_min = (v_tex[i] < v_min) ? v_tex[i] : v_min;
        v_max = (v_tex[i] > v_max) ? v_tex[i] : v_max;
    }

    int32_t u0 = floor_fixed_to_int(u_min);
    int32_t u1 = ceil_fixed_to_int(u_max);
    int32_t v0 = floor_fixed_to_int(v_min);
    int32_t v1 = ceil_fixed_to_int(v_max);
    if (u0 < 0) {
        u0 = 0;
    }
    if (u1 > static_cast<int32_t>(in->texture_width)) {
        u1 = static_cast<int32_t>(in->texture_width);
    }
    if (v0 < 0) {
        v0 = 0;
    }
    if (v1 > static_cast<int32_t>(texture_height)) {
        v1 = static_cast<int32_t>(texture_height);
    }
    u0 &= ~7;
    u1 = (u1 + 7) & ~7;
    if (u1 > static_cast<int32_t>(in->texture_width)) {
        u1 = static_cast<int32_t>(in->texture_width);
    }
    if (u1 <= u0) {
        u1 = u0 + 8;
        if (u1 > static_cast<int32_t>(in->texture_width)) {
            u0 = static_cast<int32_t>(in->texture_width) - 8;
            u1 = static_cast<int32_t>(in->texture_width);
        }
    }
    if (v1 <= v0) {
        v1 = v0 + 1;
        if (v1 > static_cast<int32_t>(texture_height)) {
            v0 = static_cast<int32_t>(texture_height) - 1;
            v1 = static_cast<int32_t>(texture_height);
        }
    }
    if (u0 < 0 || v0 < 0 || u1 <= u0 || v1 <= v0 || (u1 - u0) > 256 || (v1 - v0) > 255) {
        if (diag != nullptr) {
            *diag = TRI_QUAD_INVALID_TEXTURE;
        }
        return false;
    }

    const CornerUV corners[4] = {
        {fixed_from_int(u0), fixed_from_int(v0)},
        {fixed_from_int(u1), fixed_from_int(v0)},
        {fixed_from_int(u1), fixed_from_int(v1)},
        {fixed_from_int(u0), fixed_from_int(v1)},
    };

    // Каждый угол обязан быть в пределах допуска от одного из углов
    // исходного прямоугольника; углы обязаны быть различными.
    int32_t corner_of_vertex[3];
    bool used[4] = {false, false, false, false};
    const int64_t tolerance = static_cast<int64_t>(TRI_QUAD_CORNER_TOLERANCE_TEXELS) * FIXED_ONE;
    const int64_t tolerance_sq = tolerance * tolerance;
    for (uint32_t i = 0u; i < 3u; ++i) {
        int64_t distance = 0;
        const int32_t corner = nearest_corner(u_tex[i], v_tex[i], corners, &distance);
        if (distance > tolerance_sq || used[corner]) {
            if (diag != nullptr) {
                *diag = TRI_QUAD_UNSUPPORTED_UV;
            }
            return false;
        }
        used[corner] = true;
        corner_of_vertex[i] = corner;
    }

    int32_t vertex_of_corner[4] = {-1, -1, -1, -1};
    for (uint32_t i = 0u; i < 3u; ++i) {
        vertex_of_corner[corner_of_vertex[i]] = static_cast<int32_t>(i);
    }

    int16_t xs[4];
    int16_t ys[4];
    uint16_t colors[4];
    for (int32_t c = 0; c < 4; ++c) {
        int32_t source = vertex_of_corner[c];
        // Недостающий угол дублирует следующий назначенный (по кругу
        // TL→TR→BR→BL). Последовательность из 4 точек остаётся циклическим
        // обходом трёх реальных углов: VDP1 получает вырожденный quad с
        // корректной UV-разметкой.
        for (int32_t step = 1; source < 0 && step <= 4; ++step) {
            source = vertex_of_corner[(c + step) & 3];
        }
        if (source < 0) {
            if (diag != nullptr) {
                *diag = TRI_QUAD_UNSUPPORTED_UV;
            }
            return false;
        }
        xs[c] = clamp_int16(tri_quad_round_fixed(in->v[source].x));
        ys[c] = clamp_int16(tri_quad_round_fixed(in->v[source].y));
        colors[c] = pack_gouraud_color(in->v[source]);
    }

    int32_t min_x = xs[0], max_x = xs[0], min_y = ys[0], max_y = ys[0];
    for (uint32_t i = 1u; i < 4u; ++i) {
        min_x = (xs[i] < min_x) ? xs[i] : min_x;
        max_x = (xs[i] > max_x) ? xs[i] : max_x;
        min_y = (ys[i] < min_y) ? ys[i] : min_y;
        max_y = (ys[i] > max_y) ? ys[i] : max_y;
    }
    if (points_all_outside(min_x, max_x, min_y, max_y)) {
        if (diag != nullptr) {
            *diag = TRI_QUAD_OUT_OF_SCREEN;
        }
        return false;
    }

    // Площадь всего quad (для degenerate-проверки; winding не навязываем:
    // backface уже отброшен ядром, VDP1 не делает backface culling).
    int64_t area2 = 0;
    for (uint32_t i = 0u; i < 4u; ++i) {
        const uint32_t j = (i + 1u) & 3u;
        area2 += static_cast<int64_t>(xs[i]) * ys[j] - static_cast<int64_t>(xs[j]) * ys[i];
    }
    if (area2 == 0) {
        if (diag != nullptr) {
            *diag = TRI_QUAD_DEGENERATE;
        }
        return false;
    }

    for (uint32_t i = 0u; i < 4u; ++i) {
        out->x[i] = xs[i];
        out->y[i] = ys[i];
        out->colors[i] = colors[i];
    }
    out->u0 = static_cast<uint16_t>(u0);
    out->v0 = static_cast<uint16_t>(v0);
    out->width = static_cast<uint16_t>(u1 - u0);
    out->height = static_cast<uint16_t>(v1 - v0);
    out->textured = true;
    if (diag != nullptr) {
        *diag = TRI_QUAD_OK;
    }
    return true;
}

const char* tri_quad_diag_string(TriQuadDiag diag)
{
    switch (diag) {
        case TRI_QUAD_OK:
            return "ok";
        case TRI_QUAD_NULL_ARGUMENT:
            return "null argument";
        case TRI_QUAD_DEGENERATE:
            return "degenerate";
        case TRI_QUAD_OUT_OF_SCREEN:
            return "outside screen";
        case TRI_QUAD_UNSUPPORTED_UV:
            return "unsupported uv";
        case TRI_QUAD_INVALID_TEXTURE:
            return "invalid texture";
        case TRI_QUAD_COUNT:
        default:
            return "unknown";
    }
}

}  // namespace saturn
