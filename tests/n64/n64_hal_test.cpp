// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/n64_hal_test.cpp
//
// Host-тесты N64 HAL (S39–42): lifecycle/idempotence/time, render skeleton
// (BeginFrame/Clear/EndFrame + явные отказы S43–44) и безопасные no-op.
// Собираются host-компилятором с PLATFORM_N64=1 и fake_n64_bridge.cpp —
// libdragon/toolchain не нужны.
#include <gtest/gtest.h>

#include "fake_n64_bridge.h"
#include "hal/n64/n64_bridge.h"
#include "hal/n64/rdp_command.h"
#include "hal/platform.h"
#include "hal/render_backend.h"

namespace {

class N64HalTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Platform_Shutdown();
        rpe_n64_fake_reset();
    }

    void TearDown() override { Platform_Shutdown(); }
};

TEST_F(N64HalTest, PlatformInitIsIdempotent)
{
    rpe_n64_fake_ticks = 1234u;

    EXPECT_TRUE(Platform_Init());
    EXPECT_TRUE(Platform_Init());  // повторный Init — no-op, true
    EXPECT_EQ(rpe_n64_fake_init_calls, 1);
    EXPECT_TRUE(rpe_n64_core_ready());
    EXPECT_EQ(Platform_GetTimeMs(), 1234u);

    Platform_Shutdown();
    Platform_Shutdown();  // повторный Shutdown — no-op
    EXPECT_EQ(rpe_n64_fake_shutdown_calls, 1);
    EXPECT_FALSE(rpe_n64_core_ready());
    EXPECT_EQ(Platform_GetTimeMs(), 0u);
}

TEST_F(N64HalTest, PlatformReinitAfterShutdownWorks)
{
    EXPECT_TRUE(Platform_Init());
    Platform_Shutdown();
    EXPECT_TRUE(Platform_Init());
    EXPECT_EQ(rpe_n64_fake_init_calls, 2);
    EXPECT_EQ(rpe_n64_fake_shutdown_calls, 1);
    EXPECT_TRUE(rpe_n64_core_ready());
}

TEST_F(N64HalTest, PlatformInitFailureLeavesStateUninitialized)
{
    rpe_n64_fake_init_result = 0;
    EXPECT_FALSE(Platform_Init());
    EXPECT_EQ(rpe_n64_fake_init_calls, 1);
    EXPECT_EQ(Platform_GetTimeMs(), 0u);

    // После отказа повторный Init допустим (полный откат состояния).
    rpe_n64_fake_init_result = 1;
    EXPECT_TRUE(Platform_Init());
    EXPECT_EQ(rpe_n64_fake_init_calls, 2);
}

TEST_F(N64HalTest, PlatformSleepAdvancesPlatformTime)
{
    rpe_n64_fake_ticks = 100u;
    EXPECT_TRUE(Platform_Init());

    Platform_SleepMs(0u);
    EXPECT_EQ(rpe_n64_fake_sleep_calls, 0);

    Platform_SleepMs(25u);
    EXPECT_EQ(rpe_n64_fake_sleep_calls, 1);
    EXPECT_EQ(Platform_GetTimeMs(), 125u);
}

TEST_F(N64HalTest, TenMinuteTimeCounterWrapSafe)
{
    // 10 минут = 600000 мс; uint32-счётчик не переполняется.
    rpe_n64_fake_ticks = 0xFFFFFF00u;
    EXPECT_TRUE(Platform_Init());
    const uint32_t start = Platform_GetTimeMs();

    Platform_SleepMs(600000u);
    const uint32_t end = Platform_GetTimeMs();

    // Wrap-safe беззнаковая разность даёт ровно 10 минут.
    EXPECT_EQ((uint32_t)(end - start), 600000u);
}

TEST_F(N64HalTest, RenderSkeletonBeginsClearsAndEndsFrame)
{
    ASSERT_TRUE(Platform_Init());
    EXPECT_TRUE(Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT));
    EXPECT_EQ(render_diagnostics().last_code, static_cast<uint32_t>(RENDER_DIAG_OK));

    Render_BeginFrame();
    EXPECT_EQ(rpe_n64_fake_frame_begin_calls, 1);
    EXPECT_EQ(render_diagnostics().last_code, static_cast<uint32_t>(RENDER_DIAG_OK));

    Render_Clear(0u, 0u, 32u);
    EXPECT_EQ(rpe_n64_fake_frame_clear_calls, 1);

    Render_EndFrame();
    EXPECT_EQ(rpe_n64_fake_frame_end_calls, 1);
    EXPECT_FALSE(rpe_n64_frame_active());

    Render_Shutdown();
}

TEST_F(N64HalTest, RenderSkeletonRejectsWrongSizeAndAllowsReinit)
{
    // Ядро принимает 640×480 по общему контракту, но N64 backend отклоняет
    // не-reference размер явно (NO_WINDOW/backend fail).
    EXPECT_FALSE(Render_Init(640u, 480u));
    EXPECT_EQ(render_diagnostics().last_code, static_cast<uint32_t>(RENDER_DIAG_NO_WINDOW));
    Render_Shutdown();

    EXPECT_TRUE(Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT));
    Render_EndFrame();  // EndFrame без BeginFrame — диагностический no-op.
    EXPECT_EQ(render_diagnostics().last_code,
              static_cast<uint32_t>(RENDER_DIAG_END_FRAME_WITHOUT_BEGIN));
    Render_Shutdown();

    // Повторный Init после Shutdown допустим (double Init в debug — assert).
    EXPECT_TRUE(Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT));
    Render_Shutdown();
}

TEST_F(N64HalTest, RenderSkeletonRejectsUnavailableDisplay)
{
    rpe_n64_fake_render_available = 0;
    EXPECT_FALSE(Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT));
    EXPECT_EQ(render_diagnostics().last_code, static_cast<uint32_t>(RENDER_DIAG_NO_WINDOW));
    Render_Shutdown();
}

TEST_F(N64HalTest, StubDrawAndTextureRejectExplicitly)
{
    uint8_t pixels[1] = {0u};
    uint16_t palette[256] = {};
    HalRenderBackendVertex vertices[3] = {};
    uint8_t readback[4] = {};

    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT));
    Render_BeginFrame();

    EXPECT_FALSE(hal_render_backend_create_texture(1u, 1u, 1u, pixels, palette));
    EXPECT_FALSE(hal_render_backend_draw_triangle(vertices, nullptr, RENDER_TEXTURE_INVALID));
    EXPECT_FALSE(hal_render_backend_draw_overlay(nullptr, 0u));
    EXPECT_FALSE(hal_render_backend_read_pixels(readback, sizeof(readback)));
    EXPECT_EQ(hal_render_backend_draw_call_count(), 0u);

    Render_EndFrame();
    Render_Shutdown();

    // Skeleton кадра завершён без silent drop; рисование явно отклонено.
    EXPECT_EQ(render_diagnostics().last_code, static_cast<uint32_t>(RENDER_DIAG_OK));
}

TEST_F(N64HalTest, StubInputAudioFilesystemAreSafeWithoutDfs)
{
    EXPECT_TRUE(Platform_Init());
    FileData file = FileSystem_LoadFile("CUBE.MSH");
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(file.size, 0u);
    FileSystem_FreeFile(&file);
    FileSystem_FreeFile(nullptr);

    EXPECT_FALSE(Input_IsKeyDown(INPUT_KEY_W));
    EXPECT_FALSE(Input_IsPadButtonDown(0u, INPUT_PAD_START));
    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_LEFTX), 0);

    EXPECT_FALSE(Audio_Init());
    Audio_PlayBeep(440u, 10u);
    Audio_Shutdown();
}

TEST_F(N64HalTest, CommandBufferOverflowIsExplicit)
{
    ASSERT_TRUE(Platform_Init());
    n64::rdp_command_buffer().reset();

    uint8_t payload[1024] = {};
    bool overflowed = false;
    for (uint32_t i = 0u; i < 500u; ++i) {
        if (!n64::rdp_command_buffer().append(n64::RDP_OPCODE_CLEAR, payload, sizeof(payload))) {
            overflowed = true;
            break;
        }
    }
    EXPECT_TRUE(overflowed);
    EXPECT_EQ(n64::rdp_command_buffer().diag(), n64::RDP_COMMAND_OVERFLOW);
    EXPECT_GE(n64::rdp_command_buffer().overflow_count(), 1u);
    EXPECT_LE(n64::rdp_command_buffer().used(), n64::rdp_command_buffer().capacity());

    // Состояние после отказа не меняется (no silent drop/partial write).
    const size_t used = n64::rdp_command_buffer().used();
    EXPECT_FALSE(n64::rdp_command_buffer().append(n64::RDP_OPCODE_CLEAR, payload, sizeof(payload)));
    EXPECT_EQ(n64::rdp_command_buffer().used(), used);

    n64::rdp_command_buffer().reset();
    EXPECT_EQ(n64::rdp_command_buffer().used(), 0u);
    EXPECT_EQ(n64::rdp_command_buffer().diag(), n64::RDP_COMMAND_OK);
}

}  // namespace
