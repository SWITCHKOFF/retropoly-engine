// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/triangle_to_quad_test.cpp
//
// Политика triangle→quad (ADR-010): вырожденный quad, UV-углы, квантование,
// screen-bound и degenerate reject.
#include "hal/saturn/triangle_to_quad.h"

#include <gtest/gtest.h>

namespace {

using namespace saturn;

fixed_t fx(int32_t v)
{
    return fixed_from_int(v);
}

fixed_t f01(float v)
{
    return fixed_from_float(v);
}

TriQuadVertex make_vertex(int32_t x, int32_t y, float u, float v, uint8_t r = 255u,
                          uint8_t g = 255u, uint8_t b = 255u)
{
    TriQuadVertex out;
    out.x = fx(x);
    out.y = fx(y);
    out.u = f01(u);
    out.v = f01(v);
    out.r = r;
    out.g = g;
    out.b = b;
    out.a = 255u;
    return out;
}

TriQuadInput make_input(const TriQuadVertex& a, const TriQuadVertex& b, const TriQuadVertex& c,
                        uint32_t tw, uint32_t th)
{
    TriQuadInput in;
    in.v[0] = a;
    in.v[1] = b;
    in.v[2] = c;
    in.texture_width = tw;
    in.texture_height = th;
    return in;
}

TEST(TriangleToQuad, TexturedTopRightHalf)
{
    // Треугольник TL,TR,BR — верхняя/правая половина quad'а.
    const TriQuadInput in =
        make_input(make_vertex(0, 0, 0.0f, 0.0f), make_vertex(100, 0, 1.0f, 0.0f),
                   make_vertex(100, 100, 1.0f, 1.0f), 256u, 256u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    ASSERT_TRUE(triangle_to_quad(&in, &quad, &diag)) << tri_quad_diag_string(diag);
    EXPECT_EQ(diag, TRI_QUAD_OK);
    EXPECT_TRUE(quad.textured);
    EXPECT_EQ(quad.u0, 0u);
    EXPECT_EQ(quad.v0, 0u);
    EXPECT_EQ(quad.width, 256u);
    EXPECT_EQ(quad.height, 255u);  // VDP1 pattern height max
    EXPECT_EQ(quad.x[0], 0);
    EXPECT_EQ(quad.y[0], 0);
    EXPECT_EQ(quad.x[1], 100);
    EXPECT_EQ(quad.y[1], 0);
    EXPECT_EQ(quad.x[2], 100);
    EXPECT_EQ(quad.y[2], 100);
    EXPECT_EQ(quad.x[3], quad.x[0]);  // missing BL дублирует следующий TL
    EXPECT_EQ(quad.y[3], quad.y[0]);
}

TEST(TriangleToQuad, TexturedBottomLeftHalf)
{
    const TriQuadInput in =
        make_input(make_vertex(0, 0, 0.0f, 0.0f), make_vertex(100, 0, 1.0f, 0.0f),
                   make_vertex(0, 100, 0.0f, 1.0f), 128u, 128u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    ASSERT_TRUE(triangle_to_quad(&in, &quad, &diag)) << tri_quad_diag_string(diag);
    EXPECT_EQ(quad.width, 128u);
    EXPECT_EQ(quad.height, 128u);
    // missing BR дублирует BL.
    EXPECT_EQ(quad.x[2], quad.x[3]);
    EXPECT_EQ(quad.y[2], quad.y[3]);
}

TEST(TriangleToQuad, TexturedLeftHalfDuplicatesMissingTopRight)
{
    const TriQuadInput in =
        make_input(make_vertex(0, 0, 0.0f, 0.0f), make_vertex(100, 100, 1.0f, 1.0f),
                   make_vertex(0, 100, 0.0f, 1.0f), 256u, 256u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    ASSERT_TRUE(triangle_to_quad(&in, &quad, &diag)) << tri_quad_diag_string(diag);
    // missing TR дублирует следующий BR.
    EXPECT_EQ(quad.x[1], quad.x[2]);
    EXPECT_EQ(quad.y[1], quad.y[2]);
}

TEST(TriangleToQuad, TextureSubrectIsSnappedToEight)
{
    // UV 128..255 по X и 0..128 по Y → source rect 128×128.
    const TriQuadInput in =
        make_input(make_vertex(0, 0, 0.5f, 0.0f), make_vertex(100, 0, 1.0f, 0.0f),
                   make_vertex(100, 100, 1.0f, 0.5f), 256u, 256u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    ASSERT_TRUE(triangle_to_quad(&in, &quad, &diag)) << tri_quad_diag_string(diag);
    EXPECT_EQ(quad.u0, 128u);
    EXPECT_EQ(quad.v0, 0u);
    EXPECT_EQ(quad.width, 128u);
    EXPECT_EQ(quad.height, 128u);
}

TEST(TriangleToQuad, UntexturedUsesOriginalTriangleOrder)
{
    const TriQuadInput in = make_input(make_vertex(10, 10, 0.0f, 0.0f, 255u, 0u, 0u),
                                       make_vertex(50, 10, 0.0f, 0.0f, 0u, 255u, 0u),
                                       make_vertex(10, 50, 0.0f, 0.0f, 0u, 0u, 255u), 0u, 0u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    ASSERT_TRUE(triangle_to_quad(&in, &quad, &diag)) << tri_quad_diag_string(diag);
    EXPECT_FALSE(quad.textured);
    EXPECT_EQ(quad.x[0], 10);
    EXPECT_EQ(quad.x[1], 50);
    EXPECT_EQ(quad.x[2], 10);
    EXPECT_EQ(quad.x[3], quad.x[2]);
    EXPECT_EQ(quad.colors[0], 0x001Fu);  // red RGB555 без MSB
    EXPECT_EQ(quad.colors[1], 0x03E0u);  // green
    EXPECT_EQ(quad.colors[2], 0x7C00u);  // blue
}

TEST(TriangleToQuad, UnsupportedUvIsExplicit)
{
    // Третья UV — середина нижней кромки: не угол прямоугольника.
    const TriQuadInput in =
        make_input(make_vertex(0, 0, 0.0f, 0.0f), make_vertex(100, 0, 1.0f, 0.0f),
                   make_vertex(50, 100, 0.5f, 1.0f), 256u, 256u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    EXPECT_FALSE(triangle_to_quad(&in, &quad, &diag));
    EXPECT_EQ(diag, TRI_QUAD_UNSUPPORTED_UV);
}

TEST(TriangleToQuad, OffScreenIsRejected)
{
    const TriQuadInput in =
        make_input(make_vertex(400, 0, 0.0f, 0.0f), make_vertex(500, 0, 1.0f, 0.0f),
                   make_vertex(500, 100, 1.0f, 1.0f), 256u, 256u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    EXPECT_FALSE(triangle_to_quad(&in, &quad, &diag));
    EXPECT_EQ(diag, TRI_QUAD_OUT_OF_SCREEN);
}

TEST(TriangleToQuad, DegenerateIsRejected)
{
    const TriQuadInput in =
        make_input(make_vertex(10, 10, 0.0f, 0.0f), make_vertex(20, 20, 1.0f, 0.0f),
                   make_vertex(30, 30, 0.0f, 1.0f), 0u, 0u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    EXPECT_FALSE(triangle_to_quad(&in, &quad, &diag));
    EXPECT_EQ(diag, TRI_QUAD_DEGENERATE);
}

TEST(TriangleToQuad, InvalidTextureSizeIsExplicit)
{
    const TriQuadInput in =
        make_input(make_vertex(0, 0, 0.0f, 0.0f), make_vertex(10, 0, 1.0f, 0.0f),
                   make_vertex(10, 10, 1.0f, 1.0f), 12u, 64u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    EXPECT_FALSE(triangle_to_quad(&in, &quad, &diag));
    EXPECT_EQ(diag, TRI_QUAD_INVALID_TEXTURE);
}

TEST(TriangleToQuad, PartialScreenTriangleKeepsVdp1Clipping)
{
    const TriQuadInput in =
        make_input(make_vertex(-20, 10, 0.0f, 0.0f), make_vertex(40, 10, 1.0f, 0.0f),
                   make_vertex(40, 60, 1.0f, 1.0f), 256u, 256u);
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    ASSERT_TRUE(triangle_to_quad(&in, &quad, &diag)) << tri_quad_diag_string(diag);
    EXPECT_LT(quad.x[0], 0);
}

TEST(TriangleToQuad, RoundingIsHalfAwayFromZero)
{
    EXPECT_EQ(tri_quad_round_fixed(fixed_from_float(10.5f)), 11);
    EXPECT_EQ(tri_quad_round_fixed(fixed_from_float(10.4f)), 10);
    EXPECT_EQ(tri_quad_round_fixed(fixed_from_float(-10.5f)), -11);
    EXPECT_EQ(tri_quad_round_fixed(fixed_from_float(-10.4f)), -10);
}

TEST(TriangleToQuad, NullArgumentsAreExplicit)
{
    Vdp1Quad quad;
    TriQuadDiag diag = TRI_QUAD_COUNT;
    EXPECT_FALSE(triangle_to_quad(nullptr, &quad, &diag));
    EXPECT_EQ(diag, TRI_QUAD_NULL_ARGUMENT);
}

}  // namespace
