// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_input.cpp
//
// Маппинг 6-button Saturn pad (контракты — saturn_input.h).
// SMPC отдаёт active-low данные: бит 0 — кнопка нажата.
#include "hal/saturn/saturn_input.h"

#include "hal/saturn/jo_saturn_bridge.h"

namespace saturn {

namespace {

// Активные-низкие биты SMPC (совпадают с JO_KEY_* Jo Engine).
constexpr uint16_t kRawRight = 1u << 15;
constexpr uint16_t kRawLeft = 1u << 14;
constexpr uint16_t kRawDown = 1u << 13;
constexpr uint16_t kRawUp = 1u << 12;
constexpr uint16_t kRawStart = 1u << 11;
constexpr uint16_t kRawA = 1u << 10;
constexpr uint16_t kRawC = 1u << 9;
constexpr uint16_t kRawB = 1u << 8;
constexpr uint16_t kRawR = 1u << 7;
constexpr uint16_t kRawX = 1u << 6;
constexpr uint16_t kRawY = 1u << 5;
constexpr uint16_t kRawZ = 1u << 4;
constexpr uint16_t kRawL = 1u << 3;

RawPadSource s_source = nullptr;
RawPadState s_raw = {0xFFFFu, 0u, 0u};
uint16_t s_buttons = 0u;
bool s_connected = false;
bool s_regular = false;
uint32_t s_poll_count = 0u;

}  // namespace

uint16_t input_map_raw(uint16_t raw, bool connected, bool regular)
{
    if (!connected || !regular) {
        return 0u;
    }

    const uint16_t pressed = static_cast<uint16_t>(~raw);
    uint16_t buttons = 0u;
    if ((pressed & kRawUp) != 0u) buttons |= RPE_SATURN_PAD_UP;
    if ((pressed & kRawDown) != 0u) buttons |= RPE_SATURN_PAD_DOWN;
    if ((pressed & kRawLeft) != 0u) buttons |= RPE_SATURN_PAD_LEFT;
    if ((pressed & kRawRight) != 0u) buttons |= RPE_SATURN_PAD_RIGHT;
    if ((pressed & kRawA) != 0u) buttons |= RPE_SATURN_PAD_A;
    if ((pressed & kRawB) != 0u) buttons |= RPE_SATURN_PAD_B;
    if ((pressed & kRawC) != 0u) buttons |= RPE_SATURN_PAD_C;
    if ((pressed & kRawX) != 0u) buttons |= RPE_SATURN_PAD_X;
    if ((pressed & kRawY) != 0u) buttons |= RPE_SATURN_PAD_Y;
    if ((pressed & kRawZ) != 0u) buttons |= RPE_SATURN_PAD_Z;
    if ((pressed & kRawL) != 0u) buttons |= RPE_SATURN_PAD_L;
    if ((pressed & kRawR) != 0u) buttons |= RPE_SATURN_PAD_R;
    if ((pressed & kRawStart) != 0u) buttons |= RPE_SATURN_PAD_START;
    return buttons;
}

void input_init(RawPadSource source)
{
    s_source = source;
    s_raw.data = 0xFFFFu;
    s_raw.connected = 0u;
    s_raw.regular = 0u;
    s_buttons = 0u;
    s_connected = false;
    s_regular = false;
    s_poll_count = 0u;
}

void input_shutdown(void)
{
    s_source = nullptr;
    s_raw.data = 0xFFFFu;
    s_raw.connected = 0u;
    s_raw.regular = 0u;
    s_buttons = 0u;
    s_connected = false;
    s_regular = false;
}

void input_poll(void)
{
    ++s_poll_count;

    RawPadState raw = {0xFFFFu, 0u, 0u};
    if (s_source != nullptr) {
        raw = s_source();
    }

    s_raw = raw;
    s_connected = raw.connected != 0u;
    s_regular = raw.regular != 0u;
    s_buttons = input_map_raw(raw.data, s_connected, s_regular);
}

uint16_t input_buttons(void)
{
    return s_buttons;
}

bool input_is_connected(void)
{
    return s_connected;
}

bool input_is_regular(void)
{
    return s_regular;
}

uint32_t input_poll_count(void)
{
    return s_poll_count;
}

bool input_pad_button_down(uint32_t pad_index, PadButton button)
{
    if (pad_index != 0u) {
        return false;
    }

    uint16_t bit = 0u;
    switch (button) {
        case INPUT_PAD_UP:
            bit = RPE_SATURN_PAD_UP;
            break;
        case INPUT_PAD_DOWN:
            bit = RPE_SATURN_PAD_DOWN;
            break;
        case INPUT_PAD_LEFT:
            bit = RPE_SATURN_PAD_LEFT;
            break;
        case INPUT_PAD_RIGHT:
            bit = RPE_SATURN_PAD_RIGHT;
            break;
        case INPUT_PAD_CROSS:
            bit = RPE_SATURN_PAD_A;
            break;
        case INPUT_PAD_CIRCLE:
            bit = RPE_SATURN_PAD_B;
            break;
        case INPUT_PAD_SQUARE:
            bit = RPE_SATURN_PAD_C;
            break;
        case INPUT_PAD_TRIANGLE:
            bit = RPE_SATURN_PAD_X;
            break;
        case INPUT_PAD_L1:
            bit = RPE_SATURN_PAD_Y;
            break;
        case INPUT_PAD_R1:
            bit = RPE_SATURN_PAD_Z;
            break;
        case INPUT_PAD_L2:
            bit = RPE_SATURN_PAD_L;
            break;
        case INPUT_PAD_R2:
            bit = RPE_SATURN_PAD_R;
            break;
        case INPUT_PAD_START:
            bit = RPE_SATURN_PAD_START;
            break;
        case INPUT_PAD_SELECT:  // 6-button Saturn pad не имеет Select.
        case INPUT_PAD_COUNT:
        default:
            return false;
    }

    return bit != 0u && (s_buttons & bit) != 0u;
}

}  // namespace saturn
