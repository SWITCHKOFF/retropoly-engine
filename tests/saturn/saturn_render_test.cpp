// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_render_test.cpp
//
// Saturn HAL lifecycle/render skeleton: идемпотентный Platform_Init/Shutdown,
// откат при сбое core/FS, ровно один poll пада за PumpEvents, VDP2-clear
// golden (0x9000 / FNV 0xC4AE5DC5) и явный отказ VDP1-draw (S27–28).
#include "hal/platform.h"
#include "hal/render_backend.h"
#include "hal/saturn/saturn_fs.h"
#include "hal/saturn/saturn_hal_internal.h"
#include "hal/saturn/saturn_input.h"
#include "hal/saturn/saturn_memory.h"
#include "hal/saturn/saturn_render_utils.h"
#include "hal/saturn/slave_worker.h"
#include "render/renderer.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

namespace {

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

TEST_F(SaturnRenderTest, Vdp1DrawIsExplicitlyRejected)
{
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(320u, 224u));

    HalRenderBackendVertex vertices[3] = {};
    float mvp[16] = {};
    EXPECT_FALSE(hal_render_backend_draw_triangle(vertices, mvp, RENDER_TEXTURE_INVALID));
    EXPECT_FALSE(hal_render_backend_create_texture(1u, 64u, 64u, nullptr, nullptr));
    EXPECT_FALSE(hal_render_backend_draw_overlay(nullptr, 0u));
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
