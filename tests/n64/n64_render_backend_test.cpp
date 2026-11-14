// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/n64_render_backend_test.cpp
//
// Host-тесты N64 render backend S43–44 (fake C-мост): RSP T&L batching,
// split по TMEM-полосам, CPU fallback, overlay, overflow и readback.
#include <gtest/gtest.h>

#include "fake_n64_bridge.h"
#include "fake_n64_render_bridge.h"
#include "hal/n64/n64_memory.h"
#include "hal/n64/n64_render_control.h"
#include "hal/render_backend.h"
#include "render/texture.h"

#include "core/math/mat4.h"

#include <string.h>

namespace {

uint8_t s_pixels[64u * 64u];
uint16_t s_palette[256];

HalRenderBackendVertex vertex(float x, float y, float z, float u, float v)
{
    HalRenderBackendVertex out = {};
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

class N64RenderBackendTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Platform_Shutdown();
        rpe_n64_fake_reset();
        rpe_n64_fake_render_reset();
        n64::memory_shutdown();
        rpe_n64_fake_render_available = 1;
        rpe_n64_fake_frame_begin_result = 1;
        ASSERT_TRUE(n64::memory_init());
        ASSERT_TRUE(hal_render_backend_init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT));

        for (uint32_t i = 0u; i < sizeof(s_pixels); ++i) {
            s_pixels[i] = (uint8_t)(i & 0xFFu);
        }
        for (uint32_t i = 0u; i < 256u; ++i) {
            s_palette[i] = (uint16_t)i;
        }
        ASSERT_TRUE(hal_render_backend_create_texture(1u, 64u, 64u, s_pixels, s_palette));

        Mat4 view;
        Mat4 proj;
        mat4_identity(&view);
        mat4_perspective(&proj, fixed_from_float(1.04719755f), fixed_from_float(4.0f / 3.0f),
                         fixed_from_int(1), fixed_from_int(100));
        hal_render_backend_set_camera(&view, &proj);
    }

    void TearDown() override
    {
        hal_render_backend_shutdown();
        n64::memory_shutdown();
        rpe_n64_fake_render_reset();
        rpe_n64_fake_reset();
    }
};

}  // namespace

TEST_F(N64RenderBackendTest, TexturedTriangleSplitsIntoTwoTmemStrips)
{
    hal_render_backend_begin_frame(0u, 0u, 32u);
    const HalRenderBackendVertex v[3] = {
        vertex(-1.0f, -1.0f, -10.0f, 0.0f, 0.0f),
        vertex(1.0f, -1.0f, -10.0f, 1.0f, 0.0f),
        vertex(0.0f, 1.0f, -10.0f, 0.5f, 1.0f),
    };
    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };
    ASSERT_TRUE(hal_render_backend_draw_triangle(v, identity, 1u));
    EXPECT_EQ(rpe_n64_fake_render_transform_calls(), 4u);

    hal_render_backend_end_frame();
    EXPECT_EQ(rpe_n64_fake_render_wait_calls(), 1u);
    EXPECT_EQ(rpe_n64_fake_render_upload_calls(), 2u);
    EXPECT_EQ(rpe_n64_fake_render_triangle_calls(), 4u);
    EXPECT_TRUE(rpe_n64_fake_render_last_textured());
    EXPECT_EQ(rpe_n64_fake_render_last_strip_sw(), 32u);
    EXPECT_LE(rpe_n64_fake_render_last_upload_bytes(), 2560u);
    EXPECT_GE(hal_render_backend_draw_call_count(), 4u);
}

TEST_F(N64RenderBackendTest, CpuFallbackDrawsWithoutRsp)
{
    hal_render_backend_set_cpu_fallback(true);
    EXPECT_TRUE(hal_render_backend_cpu_fallback());
    hal_render_backend_begin_frame(0u, 0u, 32u);
    const HalRenderBackendVertex v[3] = {
        vertex(-1.0f, -1.0f, -10.0f, 0.0f, 0.0f),
        vertex(1.0f, -1.0f, -10.0f, 1.0f, 0.0f),
        vertex(0.0f, 1.0f, -10.0f, 0.5f, 1.0f),
    };
    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };
    ASSERT_TRUE(hal_render_backend_draw_triangle(v, identity, 1u));
    EXPECT_EQ(rpe_n64_fake_render_transform_calls(), 0u);
    hal_render_backend_end_frame();
    EXPECT_EQ(rpe_n64_fake_render_triangle_calls(), 4u);
    EXPECT_EQ(rpe_n64_fake_render_wait_calls(), 0u);
    hal_render_backend_set_cpu_fallback(false);
    EXPECT_FALSE(hal_render_backend_cpu_fallback());
}

TEST_F(N64RenderBackendTest, UntexturedTriangleDoesNotUpload)
{
    hal_render_backend_begin_frame(0u, 0u, 32u);
    const HalRenderBackendVertex v[3] = {
        vertex(-1.0f, -1.0f, -10.0f, 0.0f, 0.0f),
        vertex(1.0f, -1.0f, -10.0f, 0.0f, 0.0f),
        vertex(0.0f, 1.0f, -10.0f, 0.0f, 0.0f),
    };
    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };
    ASSERT_TRUE(hal_render_backend_draw_triangle(v, identity, RENDER_TEXTURE_INVALID));
    hal_render_backend_end_frame();
    EXPECT_EQ(rpe_n64_fake_render_upload_calls(), 0u);
    EXPECT_EQ(rpe_n64_fake_render_triangle_calls(), 1u);
    EXPECT_FALSE(rpe_n64_fake_render_last_textured());
}

TEST_F(N64RenderBackendTest, OverlayAndReadback)
{
    hal_render_backend_begin_frame(0u, 0u, 32u);
    const HalRenderBackendOverlayVertex overlay[3] = {
        {10, 10, 255, 0, 0},
        {40, 10, 0, 255, 0},
        {10, 40, 0, 0, 255},
    };
    ASSERT_TRUE(hal_render_backend_draw_overlay(overlay, 3u));
    hal_render_backend_end_frame();
    EXPECT_EQ(rpe_n64_fake_render_overlay_calls(), 1u);

    static uint8_t rgba[RENDER_FB_WIDTH * RENDER_FB_HEIGHT * 4u];
    ASSERT_TRUE(hal_render_backend_read_pixels(rgba, sizeof(rgba)));
    EXPECT_EQ(rgba[0], 0u);
    EXPECT_EQ(rgba[3], 255u);
    EXPECT_NE(rgba[4 * 1000u], rgba[4 * 2000u]);
}

TEST_F(N64RenderBackendTest, TextureValidation)
{
    static uint8_t big[256u * 256u];
    memset(big, 0x5A, sizeof(big));
    EXPECT_FALSE(hal_render_backend_create_texture(2u, 4u, 4u, s_pixels, s_palette));
    EXPECT_FALSE(hal_render_backend_create_texture(3u, 48u, 64u, s_pixels, s_palette));
    EXPECT_FALSE(hal_render_backend_create_texture(17u, 64u, 64u, s_pixels, s_palette));
    EXPECT_TRUE(hal_render_backend_create_texture(2u, 256u, 256u, big, s_palette));
}

TEST_F(N64RenderBackendTest, BatchOverflowIsExplicit)
{
    hal_render_backend_begin_frame(0u, 0u, 32u);
    const HalRenderBackendVertex v[3] = {
        vertex(-1.0f, -1.0f, -10.0f, 0.0f, 0.0f),
        vertex(1.0f, -1.0f, -10.0f, 1.0f, 0.0f),
        vertex(0.0f, 1.0f, -10.0f, 0.5f, 1.0f),
    };
    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };
    // 512 слотов / 4 суб-треугольника (2 полосы × quad) = 128; далее false.
    for (uint32_t i = 0u; i < 128u; ++i) {
        ASSERT_TRUE(hal_render_backend_draw_triangle(v, identity, 1u)) << i;
    }
    EXPECT_FALSE(hal_render_backend_draw_triangle(v, identity, 1u));
    // Кадр помечен invalid: end_frame — no-op, без silent drop.
    hal_render_backend_end_frame();
    EXPECT_EQ(rpe_n64_fake_render_wait_calls(), 0u);
}

TEST_F(N64RenderBackendTest, CallsOutsideFrameAreRejected)
{
    EXPECT_FALSE(hal_render_backend_draw_triangle(nullptr, nullptr, RENDER_TEXTURE_INVALID));
    uint8_t rgba[4] = {};
    EXPECT_TRUE(hal_render_backend_read_pixels(rgba, sizeof(rgba)) == false);
}
