// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/n64_input_test.cpp
//
// N64 joypad → INPUT_PAD_*: полный маппинг, ровно один опрос на кадр,
// deadzone/инверсия Y, отсутствующий контроллер и невалидные индексы.
#include <gtest/gtest.h>

#include "fake_n64_bridge.h"
#include "hal/n64/n64_input.h"
#include "hal/platform.h"

namespace {

struct Mapping
{
    uint32_t raw_bit;
    PadButton button;
};

const Mapping kMappings[] = {
    {RPE_N64_BTN_D_UP, INPUT_PAD_UP},     {RPE_N64_BTN_D_DOWN, INPUT_PAD_DOWN},
    {RPE_N64_BTN_D_LEFT, INPUT_PAD_LEFT}, {RPE_N64_BTN_D_RIGHT, INPUT_PAD_RIGHT},
    {RPE_N64_BTN_A, INPUT_PAD_CROSS},     {RPE_N64_BTN_B, INPUT_PAD_CIRCLE},
    {RPE_N64_BTN_Z, INPUT_PAD_SQUARE},    {RPE_N64_BTN_C_UP, INPUT_PAD_TRIANGLE},
    {RPE_N64_BTN_L, INPUT_PAD_L1},        {RPE_N64_BTN_R, INPUT_PAD_R1},
    {RPE_N64_BTN_C_LEFT, INPUT_PAD_L2},   {RPE_N64_BTN_C_RIGHT, INPUT_PAD_R2},
    {RPE_N64_BTN_START, INPUT_PAD_START}, {RPE_N64_BTN_C_DOWN, INPUT_PAD_SELECT},
};

class N64InputTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Platform_Shutdown();
        rpe_n64_fake_reset();
        ASSERT_TRUE(Platform_Init());
        rpe_n64_fake_pads[0].connected = true;
    }

    void TearDown() override { Platform_Shutdown(); }
};

}  // namespace

TEST_F(N64InputTest, EveryN64ButtonMapsToDeterministicPadButton)
{
    for (const Mapping& mapping : kMappings) {
        EXPECT_TRUE(n64::input_map_button(mapping.raw_bit, mapping.button))
            << "bit " << mapping.raw_bit << " button " << mapping.button;
        for (int button = 0; button < (int)INPUT_PAD_COUNT; ++button) {
            if ((PadButton)button == mapping.button) {
                continue;
            }
            EXPECT_FALSE(n64::input_map_button(mapping.raw_bit, (PadButton)button))
                << "bit " << mapping.raw_bit << " unexpectedly maps to " << button;
        }
    }
}

TEST_F(N64InputTest, PollHappensExactlyOncePerPumpCall)
{
    EXPECT_EQ(rpe_n64_fake_input_poll_calls, 0);
    EXPECT_EQ(n64::input_poll_count(), 0u);

    EXPECT_TRUE(Platform_PumpEvents());
    EXPECT_EQ(rpe_n64_fake_input_poll_calls, 1);
    EXPECT_EQ(n64::input_poll_count(), 1u);

    EXPECT_TRUE(Platform_PumpEvents());
    EXPECT_EQ(rpe_n64_fake_input_poll_calls, 2);
    EXPECT_EQ(n64::input_poll_count(), 2u);
}

TEST_F(N64InputTest, SnapshotIsLatchedBetweenPolls)
{
    rpe_n64_fake_pads[0].buttons = RPE_N64_BTN_A;
    Platform_PumpEvents();
    EXPECT_TRUE(Input_IsPadButtonDown(0u, INPUT_PAD_CROSS));

    // Изменение сырого состояния без poll не видно HAL.
    rpe_n64_fake_pads[0].buttons = RPE_N64_BTN_B;
    EXPECT_TRUE(Input_IsPadButtonDown(0u, INPUT_PAD_CROSS));
    EXPECT_FALSE(Input_IsPadButtonDown(0u, INPUT_PAD_CIRCLE));

    Platform_PumpEvents();
    EXPECT_FALSE(Input_IsPadButtonDown(0u, INPUT_PAD_CROSS));
    EXPECT_TRUE(Input_IsPadButtonDown(0u, INPUT_PAD_CIRCLE));
}

TEST_F(N64InputTest, DisconnectedControllerIsSafe)
{
    rpe_n64_fake_pads[0].connected = false;
    rpe_n64_fake_pads[0].buttons = 0xFFFFFFFFu;  // «мусор» не должен протечь
    Platform_PumpEvents();

    EXPECT_FALSE(n64::input_connected(0u));
    EXPECT_EQ(n64::input_buttons(0u), 0u);
    for (int button = 0; button < (int)INPUT_PAD_COUNT; ++button) {
        EXPECT_FALSE(Input_IsPadButtonDown(0u, (PadButton)button));
    }
    for (uint32_t axis = 0u; axis < INPUT_AXIS_COUNT; ++axis) {
        EXPECT_EQ(Input_GetPadAxis(0u, axis), 0);
    }
}

TEST_F(N64InputTest, InvalidPortsAndButtonsAreSafe)
{
    rpe_n64_fake_pads[0].connected = true;
    rpe_n64_fake_pads[0].buttons = RPE_N64_BTN_A;
    Platform_PumpEvents();

    for (uint32_t port = n64::INPUT_PORT_COUNT; port < n64::INPUT_PORT_COUNT + 3u; ++port) {
        EXPECT_FALSE(Input_IsPadButtonDown(port, INPUT_PAD_CROSS));
        EXPECT_EQ(Input_GetPadAxis(port, INPUT_AXIS_LEFTX), 0);
        EXPECT_EQ(n64::input_buttons(port), 0u);
    }
    EXPECT_FALSE(Input_IsPadButtonDown(0u, INPUT_PAD_COUNT));
}

TEST_F(N64InputTest, StickDeadzoneAndYAxisConvention)
{
    rpe_n64_fake_pads[0].connected = true;
    rpe_n64_fake_pads[0].stick_x = 5;
    rpe_n64_fake_pads[0].stick_y = 5;
    rpe_n64_fake_pads[0].cstick_x = -5;
    rpe_n64_fake_pads[0].cstick_y = -5;
    Platform_PumpEvents();

    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_LEFTX), 0);
    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_LEFTY), 0);
    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_RIGHTX), 0);
    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_RIGHTY), 0);

    rpe_n64_fake_pads[0].stick_x = 64;
    rpe_n64_fake_pads[0].stick_y = 64;
    rpe_n64_fake_pads[0].cstick_x = -64;
    rpe_n64_fake_pads[0].cstick_y = -64;
    Platform_PumpEvents();

    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_LEFTX), 64);
    // libdragon +Y вверх → engine/SDL +Y вниз.
    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_LEFTY), -64);
    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_RIGHTX), -64);
    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_RIGHTY), 64);
}

TEST_F(N64InputTest, AnalogTriggersScaleToAxisRange)
{
    rpe_n64_fake_pads[0].connected = true;
    rpe_n64_fake_pads[0].analog_l = 0u;
    rpe_n64_fake_pads[0].analog_r = 255u;
    Platform_PumpEvents();

    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_L2), 0);
    EXPECT_EQ(Input_GetPadAxis(0u, INPUT_AXIS_R2), 127);

    rpe_n64_fake_pads[0].analog_l = 128u;
    Platform_PumpEvents();
    const int16_t mid = Input_GetPadAxis(0u, INPUT_AXIS_L2);
    EXPECT_GE(mid, 63);
    EXPECT_LE(mid, 64);
}

TEST_F(N64InputTest, DeadzonePureFunctionClamps)
{
    EXPECT_EQ(n64::input_apply_deadzone(0, false), 0);
    EXPECT_EQ(n64::input_apply_deadzone(n64::INPUT_DEADZONE, false), 0);
    EXPECT_EQ(n64::input_apply_deadzone(n64::INPUT_DEADZONE + 1, false), n64::INPUT_DEADZONE + 1);
    EXPECT_EQ(n64::input_apply_deadzone(100, true), -100);
    EXPECT_EQ(n64::input_apply_deadzone(-128, true), 127);
}
