// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_render_test.cpp
//
// Saturn HAL lifecycle/render skeleton: идемпотентный Platform_Init/Shutdown,
// откат при сбое core/FS, ровно один poll пада за PumpEvents, VDP2-clear
// golden (0x9000 / FNV 0xC4AE5DC5) и явный отказ VDP1-draw (S27–28).
#include "hal/platform.h"
#include "hal/render_backend.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/saturn_fs.h"
#include "hal/saturn/saturn_hal_internal.h"
#include "hal/saturn/saturn_input.h"
#include "hal/saturn/saturn_memory.h"
#include "hal/saturn/saturn_render_utils.h"
#include "hal/saturn/slave_worker.h"
#include "hal/saturn/vdp1_backend.h"
#include "hal/saturn/vdp1_texture.h"
#include "render/renderer.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

#include <string.h>

namespace {

using namespace saturn;

Mat4 make_identity_view()
{
    Mat4 view;
    mat4_identity(&view);
    return view;
}

Mat4 make_perspective()
{
    Mat4 proj;
    const fixed_t fov = fixed_div(fixed_mul(FIXED_PI, fixed_from_int(70)), fixed_from_int(180));
    mat4_perspective(&proj, fov, fixed_div(fixed_from_int(320), fixed_from_int(224)),
                     fixed_from_int(1), fixed_from_int(100));
    return proj;
}

void set_test_camera()
{
    const Mat4 view = make_identity_view();
    const Mat4 proj = make_perspective();
    Render_SetCamera(&view, &proj);
}

class SaturnRenderTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fake_saturn::reset();
        saturn::slave_worker_test_reset();
        saturn::fs_shutdown();
        saturn::input_shutdown();
        saturn::memory_shutdown();
    }

    void TearDown() override
    {
        Render_Shutdown();
        Platform_Shutdown();
    }
};

}  // namespace

TEST_F(SaturnRenderTest, PlatformInitShutdownAreIdempotent)
{
    EXPECT_TRUE(Platform_Init());
    EXPECT_TRUE(Platform_Init());
    EXPECT_EQ(fake_saturn::core_init_calls, 1);
    EXPECT_TRUE(saturn::platform_ready());

    Platform_Shutdown();
    Platform_Shutdown();
    EXPECT_EQ(fake_saturn::core_shutdown_calls, 1);
    EXPECT_FALSE(saturn::platform_ready());

    EXPECT_TRUE(Platform_Init());
    EXPECT_EQ(fake_saturn::core_init_calls, 2);
    Platform_Shutdown();
}

TEST_F(SaturnRenderTest, CoreInitFailureIsReportedAndRetryable)
{
    fake_saturn::core_init_result = false;
    EXPECT_FALSE(Platform_Init());
    EXPECT_EQ(fake_saturn::core_init_calls, 1);
    EXPECT_FALSE(saturn::platform_ready());

    fake_saturn::core_init_result = true;
    EXPECT_TRUE(Platform_Init());
    EXPECT_EQ(fake_saturn::core_init_calls, 2);
}

TEST_F(SaturnRenderTest, FilesystemFailureRollsBackCoreAndMemory)
{
    fake_saturn::fs_available = false;
    EXPECT_FALSE(Platform_Init());
    EXPECT_EQ(fake_saturn::core_shutdown_calls, 1);
    EXPECT_FALSE(saturn::platform_ready());
    EXPECT_EQ(saturn::asset_heap().used(), 0u);

    fake_saturn::fs_available = true;
    EXPECT_TRUE(Platform_Init());
}

TEST_F(SaturnRenderTest, PumpEventsPollsPadExactlyOnceAndLatches)
{
    ASSERT_TRUE(Platform_Init());

    fake_saturn::pad.data = (uint16_t)(~(1u << 10));  // A
    fake_saturn::pad.connected = 1u;
    fake_saturn::pad.regular = 1u;

    EXPECT_FALSE(Input_IsPadButtonDown(0u, INPUT_PAD_CROSS));
    EXPECT_EQ(fake_saturn::pad_read_calls, 0);

    EXPECT_TRUE(Platform_PumpEvents());
    EXPECT_EQ(fake_saturn::pad_read_calls, 1);
    EXPECT_TRUE(Input_IsPadButtonDown(0u, INPUT_PAD_CROSS));
    EXPECT_FALSE(Input_IsPadButtonDown(0u, INPUT_PAD_CIRCLE));

    EXPECT_TRUE(Platform_PumpEvents());
    EXPECT_EQ(fake_saturn::pad_read_calls, 2);
    EXPECT_EQ(saturn::input_poll_count(), 2u);
}

TEST_F(SaturnRenderTest, GoldenClearWordAndHashAreStable)
{
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(320u, 224u));
    EXPECT_EQ(fake_saturn::background_color, saturn::CLEAR_GOLDEN_WORD);
    EXPECT_EQ(saturn::rgb888_to_saturn(0u, 0u, 32u), saturn::CLEAR_GOLDEN_WORD);
    EXPECT_EQ(saturn::clear_golden_hash(), saturn::CLEAR_GOLDEN_HASH);
    EXPECT_EQ(saturn::CLEAR_GOLDEN_HASH, 0xC4AE5DC5u);

    Render_BeginFrame();
    EXPECT_EQ(fake_saturn::background_color, 0x9000u);
    EXPECT_EQ(render_diagnostics().last_code, (uint32_t)RENDER_DIAG_OK);

    Render_Clear(255u, 0u, 0u);
    EXPECT_EQ(fake_saturn::background_color, 0x801Fu);  // R5=31, MSB set
    Render_Clear(0u, 0u, 32u);
    EXPECT_EQ(fake_saturn::background_color, 0x9000u);

    Render_EndFrame();
    EXPECT_EQ(render_diagnostics().last_code, (uint32_t)RENDER_DIAG_OK);
}

TEST_F(SaturnRenderTest, Vdp1DrawRequiresCameraAndValidTexture)
{
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(320u, 224u));

    HalRenderBackendVertex vertices[3] = {};
    float mvp[16] = {};
    EXPECT_FALSE(hal_render_backend_draw_triangle(vertices, mvp, RENDER_TEXTURE_INVALID));
    EXPECT_TRUE(hal_render_backend_create_texture(1u, 64u, 64u, nullptr, nullptr) == false);
    EXPECT_TRUE(hal_render_backend_draw_overlay(nullptr, 0u) == false);

    set_test_camera();
    Render_BeginFrame();
    vertices[0].x = -1.0f;
    vertices[0].y = -1.0f;
    vertices[0].z = -5.0f;
    vertices[1].x = 1.0f;
    vertices[1].y = -1.0f;
    vertices[1].z = -5.0f;
    vertices[2].x = 1.0f;
    vertices[2].y = 1.0f;
    vertices[2].z = -5.0f;
    for (uint32_t i = 0u; i < 3u; ++i) {
        vertices[i].r = vertices[i].g = vertices[i].b = vertices[i].a = 1.0f;
    }
    EXPECT_TRUE(hal_render_backend_draw_triangle(vertices, mvp, RENDER_TEXTURE_INVALID));
    Render_EndFrame();
    EXPECT_GE(fake_saturn::vdp1_commands.size(), 1u);
}

TEST_F(SaturnRenderTest, PatternCoversCubeFace)
{
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(320u, 224u));
    set_test_camera();

    uint8_t pixels[64u * 64u];
    uint16_t palette[256];
    for (uint32_t i = 0u; i < 64u * 64u; ++i) {
        pixels[i] = static_cast<uint8_t>((i & 0xFEu) + 1u);
    }
    for (uint32_t i = 0u; i < 256u; ++i) {
        palette[i] = static_cast<uint16_t>(i);
    }
    ASSERT_TRUE(hal_render_backend_create_texture(1u, 64u, 64u, pixels, palette));
    ASSERT_NE(vdp1_textures().find(1u), nullptr);
    EXPECT_EQ(vdp1_textures().find(1u)->clut, 1u);

    HalRenderBackendVertex v[3];
    memset(v, 0, sizeof(v));
    v[0].x = -1.0f;
    v[0].y = -1.0f;
    v[0].z = -5.0f;
    v[0].u = 0.0f;
    v[0].v = 0.0f;
    v[1].x = 1.0f;
    v[1].y = -1.0f;
    v[1].z = -5.0f;
    v[1].u = 1.0f;
    v[1].v = 0.0f;
    v[2].x = 1.0f;
    v[2].y = 1.0f;
    v[2].z = -5.0f;
    v[2].u = 1.0f;
    v[2].v = 1.0f;
    for (uint32_t i = 0u; i < 3u; ++i) {
        v[i].r = v[i].g = v[i].b = v[i].a = 1.0f;
    }

    Render_BeginFrame();
    EXPECT_TRUE(hal_render_backend_draw_triangle(v, nullptr, 1u));
    Render_EndFrame();

    ASSERT_EQ(fake_saturn::vdp1_commands.size(), 1u);
    const RpeVdp1Command& cmd = fake_saturn::vdp1_commands[0];
    EXPECT_EQ(cmd.ctrl, 0x0002u);  // textured distorted sprite
    EXPECT_EQ(cmd.colr, 256u);     // palette 1
    EXPECT_EQ(cmd.size, (8u << 8) | 64u);
    EXPECT_NE(cmd.grda, 0u);
}

TEST_F(SaturnRenderTest, OverlayIsSubmittedAfterScene)
{
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(320u, 224u));
    set_test_camera();

    HalRenderBackendVertex v[3];
    memset(v, 0, sizeof(v));
    v[0].z = v[1].z = v[2].z = -5.0f;
    v[0].x = -1.0f;
    v[0].y = -1.0f;
    v[1].x = 1.0f;
    v[1].y = -1.0f;
    v[2].x = 1.0f;
    v[2].y = 1.0f;

    HalRenderBackendOverlayVertex overlay[3] = {
        {10, 10, 255, 0, 0}, {60, 10, 0, 255, 0}, {10, 60, 0, 0, 255}};

    Render_BeginFrame();
    ASSERT_TRUE(hal_render_backend_draw_triangle(v, nullptr, RENDER_TEXTURE_INVALID));
    ASSERT_TRUE(hal_render_backend_draw_overlay(overlay, 3u));
    Render_EndFrame();

    ASSERT_EQ(fake_saturn::vdp1_commands.size(), 2u);
    EXPECT_GT(fake_saturn::vdp1_keys[0], fake_saturn::vdp1_keys[1]);
    EXPECT_EQ(fake_saturn::vdp1_commands[0].ctrl, 0x0004u);
    EXPECT_EQ(fake_saturn::vdp1_commands[1].ctrl, 0x0004u);
}

TEST_F(SaturnRenderTest, ClutSwitchesBetweenPrimitives)
{
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(320u, 224u));
    set_test_camera();

    uint8_t pixels[64u * 64u];
    uint16_t palette[256];
    for (uint32_t i = 0u; i < 64u * 64u; ++i) {
        pixels[i] = static_cast<uint8_t>((i & 0xFEu) + 1u);
    }
    for (uint32_t i = 0u; i < 256u; ++i) {
        palette[i] = static_cast<uint16_t>(i);
    }
    ASSERT_TRUE(hal_render_backend_create_texture(1u, 64u, 64u, pixels, palette));
    ASSERT_TRUE(hal_render_backend_create_texture(2u, 64u, 64u, pixels, palette));
    ASSERT_EQ(vdp1_textures().find(1u)->clut, 1u);
    ASSERT_EQ(vdp1_textures().find(2u)->clut, 2u);

    HalRenderBackendVertex v[3];
    memset(v, 0, sizeof(v));
    v[0].x = -1.0f;
    v[0].y = -1.0f;
    v[0].z = -5.0f;
    v[1].x = 1.0f;
    v[1].y = -1.0f;
    v[1].z = -5.0f;
    v[2].x = 1.0f;
    v[2].y = 1.0f;
    v[2].z = -5.0f;
    v[1].u = 1.0f;
    v[2].u = 1.0f;
    v[2].v = 1.0f;
    for (uint32_t i = 0u; i < 3u; ++i) {
        v[i].r = v[i].g = v[i].b = v[i].a = 1.0f;
    }

    Render_BeginFrame();
    ASSERT_TRUE(hal_render_backend_draw_triangle(v, nullptr, 1u));
    ASSERT_TRUE(hal_render_backend_draw_triangle(v, nullptr, 2u));
    Render_EndFrame();

    ASSERT_EQ(fake_saturn::vdp1_commands.size(), 2u);
    EXPECT_EQ(fake_saturn::vdp1_commands[0].colr, 256u);
    EXPECT_EQ(fake_saturn::vdp1_commands[1].colr, 512u);
}

TEST_F(SaturnRenderTest, CommandListOverflowIsExplicit)
{
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(320u, 224u));
    set_test_camera();

    HalRenderBackendVertex v[3];
    memset(v, 0, sizeof(v));
    v[0].x = -1.0f;
    v[0].y = -1.0f;
    v[0].z = -5.0f;
    v[1].x = 1.0f;
    v[1].y = -1.0f;
    v[1].z = -5.0f;
    v[2].x = 1.0f;
    v[2].y = 1.0f;
    v[2].z = -5.0f;
    for (uint32_t i = 0u; i < 3u; ++i) {
        v[i].r = v[i].g = v[i].b = v[i].a = 1.0f;
    }

    Render_BeginFrame();
    bool overflowed = false;
    for (uint32_t i = 0u; i < 600u; ++i) {
        if (!hal_render_backend_draw_triangle(v, nullptr, RENDER_TEXTURE_INVALID)) {
            overflowed = true;
            EXPECT_GE(i, vdp1_command_list().capacity());
            break;
        }
    }
    EXPECT_TRUE(overflowed);
    EXPECT_EQ(vdp1_backend_stats().overflows, 1u);
    Render_EndFrame();
}

TEST_F(SaturnRenderTest, Vdp1FramebufferGoldenHashIsStable)
{
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(320u, 224u));

    fake_saturn::fill_framebuffer(0u, CLEAR_GOLDEN_WORD);
    fake_saturn::fill_framebuffer(1u, CLEAR_GOLDEN_WORD);
    uint32_t hash0 = 0u;
    uint32_t hash1 = 0u;
    EXPECT_TRUE(rpe_saturn_vdp1_fb_hash(0u, &hash0));
    EXPECT_TRUE(rpe_saturn_vdp1_fb_hash(1u, &hash1));
    EXPECT_EQ(hash0, CLEAR_GOLDEN_HASH);
    EXPECT_EQ(hash1, CLEAR_GOLDEN_HASH);
    EXPECT_EQ(rpe_saturn_vdp1_fb_hash(2u, &hash0), false);
}

TEST_F(SaturnRenderTest, BackendRequiresPlatformAndRejectsDoubleInit)
{
    EXPECT_FALSE(hal_render_backend_init(320u, 224u));
    EXPECT_FALSE(Render_Init(320u, 224u));

    ASSERT_TRUE(Platform_Init());
    EXPECT_TRUE(hal_render_backend_init(320u, 224u));
    EXPECT_FALSE(hal_render_backend_init(320u, 224u));
    hal_render_backend_shutdown();
    hal_render_backend_shutdown();
}

TEST_F(SaturnRenderTest, InvalidRenderSizeIsRejected)
{
    ASSERT_TRUE(Platform_Init());
    EXPECT_FALSE(Render_Init(0u, 224u));
    EXPECT_FALSE(Render_Init(320u, 0u));
    EXPECT_TRUE(Render_Init(320u, 224u));
}

TEST_F(SaturnRenderTest, QueriesAfterShutdownAreSafe)
{
    ASSERT_TRUE(Platform_Init());
    fake_saturn::pad.data = 0x0000u;
    fake_saturn::pad.connected = 1u;
    fake_saturn::pad.regular = 1u;
    ASSERT_TRUE(Platform_PumpEvents());
    Platform_Shutdown();

    EXPECT_TRUE(Platform_PumpEvents());
    EXPECT_EQ(fake_saturn::pad_read_calls, 1);  // после shutdown опроса нет
    EXPECT_FALSE(Input_IsPadButtonDown(0u, INPUT_PAD_CROSS));
    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_LEFTX), 0);
    EXPECT_FALSE(Input_IsKeyDown(INPUT_KEY_W));
    Platform_SleepMs(0u);
}
