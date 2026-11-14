// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_input_test.cpp
//
// 6-button Saturn pad: active-low биты → INPUT_PAD_*, idle/disconnect,
// ровно один опрос на кадр и latch-семантика.
#include "hal/saturn/saturn_input.h"

#include <gtest/gtest.h>

#include <stdint.h>

namespace {

saturn::RawPadState s_raw = {0xFFFFu, 0u, 0u};
int s_source_calls = 0;

saturn::RawPadState test_source(void)
{
    ++s_source_calls;
    return s_raw;
}

struct Mapping
{
    uint16_t raw_bit;
    PadButton button;
};

const Mapping kMappings[] = {
    {1u << 12, INPUT_PAD_UP},    {1u << 13, INPUT_PAD_DOWN},    {1u << 14, INPUT_PAD_LEFT},
    {1u << 15, INPUT_PAD_RIGHT}, {1u << 10, INPUT_PAD_CROSS},   {1u << 8, INPUT_PAD_CIRCLE},
    {1u << 9, INPUT_PAD_SQUARE}, {1u << 6, INPUT_PAD_TRIANGLE}, {1u << 5, INPUT_PAD_L1},
    {1u << 4, INPUT_PAD_R1},     {1u << 3, INPUT_PAD_L2},       {1u << 7, INPUT_PAD_R2},
    {1u << 11, INPUT_PAD_START},
};

class SaturnInputTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        s_source_calls = 0;
        s_raw.data = 0xFFFFu;
        s_raw.connected = 1u;
        s_raw.regular = 1u;
        saturn::input_init(&test_source);
    }

    void TearDown() override { saturn::input_shutdown(); }
};

}  // namespace

TEST_F(SaturnInputTest, IdlePadHasNoButtons)
{
    saturn::input_poll();
    EXPECT_EQ(saturn::input_buttons(), 0u);
    for (int button = 0; button < (int)INPUT_PAD_COUNT; ++button) {
        EXPECT_FALSE(saturn::input_pad_button_down(0u, (PadButton)button));
    }
}

TEST_F(SaturnInputTest, ActiveLowMapsEverySixButtonControl)
{
    for (const Mapping& mapping : kMappings) {
        s_raw.data = (uint16_t)(~mapping.raw_bit);
        saturn::input_poll();

        for (int button = 0; button < (int)INPUT_PAD_COUNT; ++button) {
            const bool expected = ((PadButton)button == mapping.button);
            EXPECT_EQ(saturn::input_pad_button_down(0u, (PadButton)button), expected)
                << "raw bit " << mapping.raw_bit << ", button " << button;
        }
    }
}

TEST_F(SaturnInputTest, DisconnectedPadIsSafe)
{
    s_raw.data = 0x0000u;  // все биты «нажаты», но пада нет
    s_raw.connected = 0u;
    s_raw.regular = 1u;
    saturn::input_poll();

    EXPECT_FALSE(saturn::input_is_connected());
    EXPECT_EQ(saturn::input_buttons(), 0u);
    EXPECT_FALSE(saturn::input_pad_button_down(0u, INPUT_PAD_CROSS));
}

TEST_F(SaturnInputTest, NonRegularPeripheralIsSafe)
{
    s_raw.data = 0x0000u;
    s_raw.connected = 1u;
    s_raw.regular = 0u;
    saturn::input_poll();

    EXPECT_TRUE(saturn::input_is_connected());
    EXPECT_FALSE(saturn::input_is_regular());
    EXPECT_EQ(saturn::input_buttons(), 0u);
}

TEST_F(SaturnInputTest, StateIsLatchedBetweenPolls)
{
    s_raw.data = (uint16_t)(~(1u << 10));  // A
    saturn::input_poll();
    EXPECT_TRUE(saturn::input_pad_button_down(0u, INPUT_PAD_CROSS));

    s_raw.data = 0xFFFFu;
    // Без нового poll() состояние не меняется.
    EXPECT_TRUE(saturn::input_pad_button_down(0u, INPUT_PAD_CROSS));
    EXPECT_EQ(s_source_calls, 1);

    saturn::input_poll();
    EXPECT_FALSE(saturn::input_pad_button_down(0u, INPUT_PAD_CROSS));
    EXPECT_EQ(s_source_calls, 2);
    EXPECT_EQ(saturn::input_poll_count(), 2u);
}

TEST_F(SaturnInputTest, SelectAndOtherPadsAreUnmapped)
{
    s_raw.data = 0xFFFFu;
    saturn::input_poll();
    EXPECT_FALSE(saturn::input_pad_button_down(0u, INPUT_PAD_SELECT));
    EXPECT_FALSE(saturn::input_pad_button_down(1u, INPUT_PAD_CROSS));
    EXPECT_FALSE(saturn::input_pad_button_down(0u, INPUT_PAD_COUNT));
}

TEST_F(SaturnInputTest, NullSourceIsSafe)
{
    saturn::input_init(nullptr);
    saturn::input_poll();
    EXPECT_EQ(saturn::input_buttons(), 0u);
    EXPECT_FALSE(saturn::input_is_connected());
}
