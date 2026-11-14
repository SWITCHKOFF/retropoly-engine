// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_input.cpp
//
// Реализация ввода N64 (контракт — n64_input.h).
#include "hal/n64/n64_input.h"

#include "hal/n64/n64_bridge.h"

namespace n64 {

namespace {

RpeN64PadState s_pads[INPUT_PORT_COUNT];
bool s_input_ready = false;
uint32_t s_poll_count = 0u;

bool port_valid(uint32_t port)
{
    return port < INPUT_PORT_COUNT;
}

}  // namespace

void input_init()
{
    for (uint32_t i = 0u; i < INPUT_PORT_COUNT; ++i) {
        s_pads[i].buttons = 0u;
        s_pads[i].stick_x = 0;
        s_pads[i].stick_y = 0;
        s_pads[i].cstick_x = 0;
        s_pads[i].cstick_y = 0;
        s_pads[i].analog_l = 0u;
        s_pads[i].analog_r = 0u;
        s_pads[i].connected = false;
    }
    s_poll_count = 0u;
    s_input_ready = true;
}

void input_shutdown()
{
    for (uint32_t i = 0u; i < INPUT_PORT_COUNT; ++i) {
        s_pads[i] = RpeN64PadState{};
    }
    s_poll_count = 0u;
    s_input_ready = false;
}

bool input_ready()
{
    return s_input_ready;
}

void input_poll()
{
    if (!s_input_ready) {
        return;
    }

    // Ровно один joypad_poll на кадр — требование S41–42.
    rpe_n64_input_poll();
    ++s_poll_count;

    for (uint32_t i = 0u; i < INPUT_PORT_COUNT; ++i) {
        RpeN64PadState state = RpeN64PadState{};
        if (!rpe_n64_input_read(i, &state)) {
            state.connected = false;
            state.buttons = 0u;
        }
        s_pads[i] = state;
    }
}

uint32_t input_poll_count()
{
    return s_poll_count;
}

bool input_connected(uint32_t port)
{
    return port_valid(port) && s_pads[port].connected;
}

uint32_t input_buttons(uint32_t port)
{
    return port_valid(port) ? s_pads[port].buttons : 0u;
}

bool input_map_button(uint32_t buttons, PadButton button)
{
    uint32_t mask = 0u;
    switch (button) {
        case INPUT_PAD_UP:
            mask = RPE_N64_BTN_D_UP;
            break;
        case INPUT_PAD_DOWN:
            mask = RPE_N64_BTN_D_DOWN;
            break;
        case INPUT_PAD_LEFT:
            mask = RPE_N64_BTN_D_LEFT;
            break;
        case INPUT_PAD_RIGHT:
            mask = RPE_N64_BTN_D_RIGHT;
            break;
        case INPUT_PAD_CROSS:
            mask = RPE_N64_BTN_A;
            break;
        case INPUT_PAD_CIRCLE:
            mask = RPE_N64_BTN_B;
            break;
        case INPUT_PAD_SQUARE:
            mask = RPE_N64_BTN_Z;
            break;
        case INPUT_PAD_TRIANGLE:
            mask = RPE_N64_BTN_C_UP;
            break;
        case INPUT_PAD_L1:
            mask = RPE_N64_BTN_L;
            break;
        case INPUT_PAD_R1:
            mask = RPE_N64_BTN_R;
            break;
        case INPUT_PAD_L2:
            mask = RPE_N64_BTN_C_LEFT;
            break;
        case INPUT_PAD_R2:
            mask = RPE_N64_BTN_C_RIGHT;
            break;
        case INPUT_PAD_START:
            mask = RPE_N64_BTN_START;
            break;
        case INPUT_PAD_SELECT:
            mask = RPE_N64_BTN_C_DOWN;
            break;
        case INPUT_PAD_COUNT:
        default:
            return false;
    }
    return (buttons & mask) != 0u;
}

bool input_pad_button_down(uint32_t port, PadButton button)
{
    if (!port_valid(port) || !s_pads[port].connected) {
        return false;
    }
    return input_map_button(s_pads[port].buttons, button);
}

int16_t input_apply_deadzone(int16_t raw, bool invert_y)
{
    int32_t value = invert_y ? -(int32_t)raw : (int32_t)raw;
    if (value < -127) {
        value = -127;
    }
    if (value > 127) {
        value = 127;
    }
    if (value >= -INPUT_DEADZONE && value <= INPUT_DEADZONE) {
        return 0;
    }
    return (int16_t)value;
}

namespace {

uint8_t analog_to_axis(uint8_t value)
{
    // 0..255 → 0..127 (детерминированное целочисленное масштабирование).
    return (uint8_t)(((uint32_t)value * 127u + 127u) / 255u);
}

}  // namespace

int16_t input_pad_axis(uint32_t port, uint32_t axis)
{
    if (!port_valid(port) || !s_pads[port].connected) {
        return 0;
    }
    const RpeN64PadState& pad = s_pads[port];
    switch (axis) {
        case INPUT_AXIS_LEFTX:
            return input_apply_deadzone(pad.stick_x, false);
        case INPUT_AXIS_LEFTY:
            // libdragon: +Y вверх; engine/SDL: +Y вниз.
            return input_apply_deadzone(pad.stick_y, true);
        case INPUT_AXIS_RIGHTX:
            return input_apply_deadzone(pad.cstick_x, false);
        case INPUT_AXIS_RIGHTY:
            return input_apply_deadzone(pad.cstick_y, true);
        case INPUT_AXIS_L2:
            return (int16_t)analog_to_axis(pad.analog_l);
        case INPUT_AXIS_R2:
            return (int16_t)analog_to_axis(pad.analog_r);
        case INPUT_AXIS_COUNT:
        default:
            return 0;
    }
}

}  // namespace n64
