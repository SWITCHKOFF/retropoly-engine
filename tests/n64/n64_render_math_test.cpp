// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/n64_render_math_test.cpp
//
// Host-тесты чистой математики S43–44: RSP T&L Q16.16, экранная проекция
// (Z-encoding), TMEM-полосы/тайлинг, UV-клиппинг, FNV и lighting-эталон.
#include <gtest/gtest.h>

#include "hal/n64/rsp_tnl.h"

#include <limits>
#include <string.h>

namespace {

n64::TnlVertex make_vertex(float x, float y, float z, float u, float v)
{
    n64::TnlVertex out = {};
    out.x = x;
    out.y = y;
    out.z = z;
    out.u = u;
    out.v = v;
    out.r = 1.0f;
    out.g = 1.0f;
    out.b = 1.0f;
    out.a = 1.0f;
    return out;
}

}  // namespace

TEST(N64RenderMath, SlotRoundtripAndMatrix)
{
    n64::TnlVertex verts[3] = {
        make_vertex(1.0f, 2.0f, -5.0f, 0.0f, 0.0f),
        make_vertex(-2.0f, 1.0f, -7.0f, 0.5f, 0.0f),
        make_vertex(0.5f, -3.0f, -9.0f, 1.0f, 1.0f),
    };
    RpeN64TnlSlot slot;
    n64::tnl_vertices_to_slot(verts, &slot);
    EXPECT_EQ(n64::tnl_slot_component(slot, 0u), 65536);
    EXPECT_EQ(n64::tnl_slot_component(slot, 1u), 131072);
    EXPECT_EQ(n64::tnl_slot_component(slot, 2u), -327680);
    EXPECT_EQ(n64::tnl_slot_component(slot, 3u), 65536);
    EXPECT_EQ(n64::tnl_slot_component(slot, 11u), 65536);
    EXPECT_EQ(n64::tnl_slot_component(slot, 12u), 0);

    // GL column-major identity.
    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };
    n64::TnlMatrix matrix;
    ASSERT_TRUE(n64::tnl_matrix_from_mvp_gl(identity, &matrix));
    int32_t clip[3][4];
    n64::tnl_transform_clip_cpu(matrix, verts, clip);
    for (uint32_t v = 0u; v < 3u; ++v) {
        EXPECT_EQ(clip[v][0], n64::tnl_slot_component(slot, v * 4u + 0u));
        EXPECT_EQ(clip[v][1], n64::tnl_slot_component(slot, v * 4u + 1u));
        EXPECT_EQ(clip[v][2], n64::tnl_slot_component(slot, v * 4u + 2u));
        EXPECT_EQ(clip[v][3], 65536);
    }
}

TEST(N64RenderMath, MatrixRejectsNullAndNonFinite)
{
    n64::TnlMatrix matrix;
    EXPECT_FALSE(n64::tnl_matrix_from_mvp_gl(nullptr, &matrix));
    const float too_big[16] = {1e9f};
    EXPECT_FALSE(n64::tnl_matrix_from_mvp_gl(too_big, &matrix));
    const float nan_matrix[16] = {std::numeric_limits<float>::quiet_NaN()};
    EXPECT_FALSE(n64::tnl_matrix_from_mvp_gl(nan_matrix, &matrix));
}

TEST(N64RenderMath, ProjectionZAndInvW)
{
    n64::TnlScreenVertex out = {};
    const int32_t center[4] = {0, 0, 0, 65536};
    ASSERT_TRUE(n64::tnl_project_clip(center, 320u, 240u, &out));
    EXPECT_FLOAT_EQ(out.x, 160.0f);
    EXPECT_FLOAT_EQ(out.y, 120.0f);
    EXPECT_FLOAT_EQ(out.z, 0.5f);
    EXPECT_FLOAT_EQ(out.invw, 1.0f);

    const int32_t top_right_near[4] = {65536, 65536, -65536, 65536};
    ASSERT_TRUE(n64::tnl_project_clip(top_right_near, 320u, 240u, &out));
    EXPECT_FLOAT_EQ(out.x, 320.0f);
    EXPECT_FLOAT_EQ(out.y, 0.0f);
    EXPECT_FLOAT_EQ(out.z, 0.0f);

    const int32_t far[4] = {0, 0, 65536, 65536};
    ASSERT_TRUE(n64::tnl_project_clip(far, 320u, 240u, &out));
    EXPECT_FLOAT_EQ(out.z, 1.0f);

    const int32_t behind[4] = {0, 0, 0, 0};
    EXPECT_FALSE(n64::tnl_project_clip(behind, 320u, 240u, &out));
    const int32_t negative[4] = {0, 0, 0, -65536};
    EXPECT_FALSE(n64::tnl_project_clip(negative, 320u, 240u, &out));
}

TEST(N64RenderMath, TmemStripTiling)
{
    // 64×64 CI8: 2 KiB текстуры + 512 B TLUT → 32-texel полоса, 2 полосы.
    EXPECT_EQ(n64::tnl_texture_strip_width(64u, 64u), 32u);
    EXPECT_EQ(n64::tnl_texture_strip_count(64u, 64u), 2u);
    EXPECT_EQ(n64::tnl_tmem_bytes(32u, 64u), 2560u);
    // 32×64 CI8 влезает целиком.
    EXPECT_EQ(n64::tnl_texture_strip_width(32u, 64u), 32u);
    EXPECT_EQ(n64::tnl_texture_strip_count(32u, 64u), 1u);
    // 64×32 → 64-texel полоса.
    EXPECT_EQ(n64::tnl_texture_strip_width(64u, 32u), 64u);
    EXPECT_EQ(n64::tnl_texture_strip_count(64u, 32u), 1u);
    // 256×256 (downsample-политика) → 8-texel полосы.
    EXPECT_EQ(n64::tnl_texture_strip_width(256u, 256u), 8u);
    EXPECT_EQ(n64::tnl_texture_strip_count(256u, 256u), 32u);
    // Неподдерживаемые: <8 texels или h>256.
    EXPECT_EQ(n64::tnl_texture_strip_width(4u, 64u), 0u);
    EXPECT_EQ(n64::tnl_texture_strip_width(64u, 512u), 0u);
}

TEST(N64RenderMath, UvStripClipping)
{
    n64::TnlTriangle tri = {};
    tri.v[0] = make_vertex(-1.0f, -1.0f, -10.0f, 0.0f, 0.0f);
    tri.v[1] = make_vertex(1.0f, -1.0f, -10.0f, 0.5f, 0.0f);
    tri.v[2] = make_vertex(0.0f, 1.0f, -10.0f, 1.0f, 1.0f);

    n64::TnlTriangle left[4];
    const uint32_t left_count = n64::tnl_clip_to_u_range(tri, 0.0f, 0.5f, left);
    ASSERT_GE(left_count, 2u); // Треугольник → quad в полосе → 2 треугольника.
    bool left_seam = false;
    for (uint32_t t = 0u; t < left_count; ++t) {
        for (uint32_t v = 0u; v < 3u; ++v) {
            EXPECT_GE(left[t].v[v].u, 0.0f);
            EXPECT_LE(left[t].v[v].u, 0.5001f);
            if (left[t].v[v].u > 0.4999f) {
                left_seam = true;
            }
        }
    }
    EXPECT_TRUE(left_seam);

    n64::TnlTriangle right[4];
    const uint32_t right_count = n64::tnl_clip_to_u_range(tri, 0.5f, 1.0f, right);
    ASSERT_GE(right_count, 2u);
    bool right_seam = false;
    for (uint32_t t = 0u; t < right_count; ++t) {
        for (uint32_t v = 0u; v < 3u; ++v) {
            EXPECT_GE(right[t].v[v].u, 0.4999f);
            EXPECT_LE(right[t].v[v].u, 1.0f);
            if (right[t].v[v].u < 0.5001f) {
                right_seam = true;
            }
        }
    }
    EXPECT_TRUE(right_seam);

    // Полностью вне полосы — 0 треугольников.
    n64::TnlTriangle outside[4];
    EXPECT_EQ(n64::tnl_clip_to_u_range(tri, 1.5f, 2.0f, outside), 0u);
    EXPECT_EQ(n64::tnl_clip_to_u_range(tri, 0.0f, 0.5f, nullptr), 0u);
}

TEST(N64RenderMath, Fnv1aKnownVectors)
{
    EXPECT_EQ(n64::tnl_fnv1a32("a", 1u), 0xE40C292Cu);
    EXPECT_EQ(n64::tnl_fnv1a32("foobar", 6u), 0xBF9CF968u);
    EXPECT_EQ(n64::tnl_fnv1a32(nullptr, 10u), 2166136261u);
}

TEST(N64RenderMath, LightingDiffuseReference)
{
    const float normal[3] = {0.0f, 0.0f, 1.0f};
    const float light[3] = {0.0f, 0.0f, 1.0f};
    const uint8_t white[3] = {255u, 255u, 255u};
    int32_t q16[3] = {};
    n64::tnl_light_diffuse_cpu(normal, light, white, q16);
    EXPECT_EQ(q16[0], 255 * 65536);
    EXPECT_EQ(q16[1], 255 * 65536);
    EXPECT_EQ(q16[2], 255 * 65536);

    const float back[3] = {0.0f, 0.0f, -1.0f};
    n64::tnl_light_diffuse_cpu(normal, back, white, q16);
    EXPECT_EQ(q16[0], 0);
    EXPECT_EQ(q16[1], 0);
    EXPECT_EQ(q16[2], 0);
}

TEST(N64RenderMath, LightingReference)
{
    const float normal[3] = {0.0f, 0.0f, 1.0f};
    const float light[3] = {0.0f, 0.0f, 1.0f};
    const uint8_t ambient[3] = {10u, 20u, 30u};
    const uint8_t color[3] = {200u, 100u, 50u};
    uint8_t out[3] = {};
    n64::tnl_light_cpu(normal, light, ambient, color, out);
    EXPECT_EQ(out[0], 210u);
    EXPECT_EQ(out[1], 120u);
    EXPECT_EQ(out[2], 80u);

    const float back[3] = {0.0f, 0.0f, -1.0f};
    n64::tnl_light_cpu(normal, back, ambient, color, out);
    EXPECT_EQ(out[0], 10u);
    EXPECT_EQ(out[1], 20u);
    EXPECT_EQ(out[2], 30u);
}
