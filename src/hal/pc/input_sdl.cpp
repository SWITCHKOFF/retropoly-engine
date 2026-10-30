// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/pc/input_sdl.cpp
//
// HAL PC: ввод. Нормативы: docs/plans/sprint_05_06.md (недели 9 и 11),
// engineering doc v1.43 §3 — ТОЛЬКО SDL_Gamepad API (SDL_GetGamepads,
// SDL_OpenGamepad, SDL_GetGamepadButton, SDL_GetGamepadAxis);
// SDL_GameController запрещён (не существует в SDL3).
//
// Поведение:
//   - геймпады открываются в hal_input_init (вызов из Platform_Init),
//     максимум HAL_PC_MAX_PADS (PS1-паритет: 2 контроллера);
//   - L2/R2 — аналоговые оси SDL: цифровое чтение по порогу
//     INPUT_TRIGGER_THRESHOLD (≥50% хода);
//   - pad 0 без реального геймпада эмулирует клавиатурой (цифровой PS1-пад,
//     week 11: WASD/стрелки → D-pad, SPACE → Крестик, J/K/L → Круг/Квадрат/
//     Треугольник, U/I → L1/R1, Enter → Start, Tab → Select; L2/R2 на
//     клавиатуре не эмулируются);
//   - все запросы вне диапазона / без устройств — безопасные значения
//     (0/false) без краша.
#include "hal/pc/hal_pc_internal.h"

#include <SDL3/SDL.h>

#include <stddef.h>
#include <stdint.h>

/// Открытые геймпады (индекс = pad_index; NULL — слот пуст).
static SDL_Gamepad* s_gamepads[HAL_PC_MAX_PADS];

/// Порог аналогового триггера в сжатом масштабе (-128..127): 64 = половина.
static constexpr int16_t INPUT_TRIGGER_THRESHOLD = 64;

/// Сканкоды INPUT_KEY_* — порядок зеркалит enum в platform.h (контракт ABI).
static const SDL_Scancode KEY_SCANCODES[INPUT_KEY_COUNT] = {
    SDL_SCANCODE_W, SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, // W A S D
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_SPACE,
    SDL_SCANCODE_RETURN,
    SDL_SCANCODE_ESCAPE,
    SDL_SCANCODE_TAB,
    SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT, // шифты PC именно они
    SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL,
    SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, // pad-эмуляция face
    SDL_SCANCODE_U, SDL_SCANCODE_I,                 // pad-эмуляция шоулдеров
};

static_assert((int)(sizeof(KEY_SCANCODES) / sizeof(KEY_SCANCODES[0])) ==
                  (int)INPUT_KEY_COUNT,
              "input_sdl.cpp: таблица сканкодов не совпадает с KeyCode");

/// INPUT_PAD_* → SDL_GamepadButton; L2/R2 отдельный путь — через оси.
static const SDL_GamepadButton PAD_BUTTONS[INPUT_PAD_COUNT] = {
    SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_SOUTH,   // PS: Крестик (низ)
    SDL_GAMEPAD_BUTTON_EAST,    // PS: Круг (право)
    SDL_GAMEPAD_BUTTON_WEST,    // PS: Квадрат (лево)
    SDL_GAMEPAD_BUTTON_NORTH,   // PS: Треугольник (верх)
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_INVALID, // L2: аналоговая ось
    SDL_GAMEPAD_BUTTON_INVALID, // R2: аналоговая ось
    SDL_GAMEPAD_BUTTON_START,
    SDL_GAMEPAD_BUTTON_BACK,    // PS: Select/Share
};

static_assert((int)(sizeof(PAD_BUTTONS) / sizeof(PAD_BUTTONS[0])) ==
                  (int)INPUT_PAD_COUNT,
              "input_sdl.cpp: таблица PAD_BUTTONS не совпадает с PadButton");

/// INPUT_AXIS_* → SDL_GamepadAxis; порядок зеркалит SDL (контракт).
static const SDL_GamepadAxis PAD_AXES[INPUT_AXIS_COUNT] = {
    SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
    SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY,
    SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
};

static_assert((int)(sizeof(PAD_AXES) / sizeof(PAD_AXES[0])) ==
                  (int)INPUT_AXIS_COUNT,
              "input_sdl.cpp: таблица PAD_AXES не совпадает с InputAxis");

/// Строка клавиатурной эмуляции цифрового pad 0 (см. комментарий в шапке).
struct KeyboardPadEntry
{
    SDL_Scancode scancode;
    PadButton button;
};

static const KeyboardPadEntry KEYBOARD_PAD_0[] = {
    { SDL_SCANCODE_W, INPUT_PAD_UP },
    { SDL_SCANCODE_S, INPUT_PAD_DOWN },
    { SDL_SCANCODE_A, INPUT_PAD_LEFT },
    { SDL_SCANCODE_D, INPUT_PAD_RIGHT },
    { SDL_SCANCODE_UP, INPUT_PAD_UP },
    { SDL_SCANCODE_DOWN, INPUT_PAD_DOWN },
    { SDL_SCANCODE_LEFT, INPUT_PAD_LEFT },
    { SDL_SCANCODE_RIGHT, INPUT_PAD_RIGHT },
    { SDL_SCANCODE_SPACE, INPUT_PAD_CROSS },
    { SDL_SCANCODE_J, INPUT_PAD_CIRCLE },
    { SDL_SCANCODE_K, INPUT_PAD_SQUARE },
    { SDL_SCANCODE_L, INPUT_PAD_TRIANGLE },
    { SDL_SCANCODE_U, INPUT_PAD_L1 },
    { SDL_SCANCODE_I, INPUT_PAD_R1 },
    { SDL_SCANCODE_RETURN, INPUT_PAD_START },
    { SDL_SCANCODE_TAB, INPUT_PAD_SELECT },
};

/// Сжатие raw SDL [-32768..32767] в [-128..127]: детерминированное целочисленное
/// raw/256 (без float; триггеры SDL [0..32767] → [0..127]).
static int16_t hal_input_scale_axis(Sint16 raw)
{
    return (int16_t)((int32_t)raw / 256);
}

bool hal_input_init(void)
{
    for (uint32_t i = 0u; i < HAL_PC_MAX_PADS; ++i) {
        s_gamepads[i] = nullptr;
    }

    int count = 0;
    SDL_JoystickID* devices = SDL_GetGamepads(&count);
    if (devices == nullptr) {
        // Устройств нет / сервис недоступен — это не сбой Platform_Init.
        return true;
    }

    int opened = count;
    if (opened < 0) {
        opened = 0;
    } else if (opened > (int)HAL_PC_MAX_PADS) {
        opened = (int)HAL_PC_MAX_PADS;
    }
    for (int i = 0; i < opened; ++i) {
        // Устройство могло исчезнуть между перечислением и открытием —
        // слот просто остаётся пустым.
        s_gamepads[i] = SDL_OpenGamepad(devices[i]);
    }
    SDL_free(devices);
    return true;
}

void hal_input_shutdown(void)
{
    for (uint32_t i = 0u; i < HAL_PC_MAX_PADS; ++i) {
        if (s_gamepads[i] != nullptr) {
            SDL_CloseGamepad(s_gamepads[i]);
            s_gamepads[i] = nullptr;
        }
    }
}

bool Input_IsKeyDown(KeyCode key)
{
    if (key < 0 || key >= INPUT_KEY_COUNT) {
        return false;
    }
    int numkeys = 0;
    const bool* state = SDL_GetKeyboardState(&numkeys);
    if (state == nullptr || numkeys <= 0) {
        return false;
    }
    const int scancode = (int)KEY_SCANCODES[key];
    if (scancode < 0 || scancode >= numkeys) {
        return false;
    }
    return state[scancode];
}

bool Input_IsPadButtonDown(uint32_t pad_index, PadButton button)
{
    if (button < 0 || button >= INPUT_PAD_COUNT || pad_index >= HAL_PC_MAX_PADS) {
        return false;
    }

    SDL_Gamepad* gamepad = s_gamepads[pad_index];
    const SDL_GamepadButton sdl_button = PAD_BUTTONS[button];

    if (gamepad == nullptr) {
        // Нет физического пада в слоте: pad 0 — клавиатурная эмуляция.
        if (pad_index != 0u) {
            return false;
        }
        if (sdl_button == SDL_GAMEPAD_BUTTON_INVALID) {
            return false; // L2/R2 на клавиатуре не эмулируются
        }
        int numkeys = 0;
        const bool* state = SDL_GetKeyboardState(&numkeys);
        if (state == nullptr || numkeys <= 0) {
            return false;
        }
        for (size_t i = 0; i < sizeof(KEYBOARD_PAD_0) / sizeof(KEYBOARD_PAD_0[0]);
             ++i) {
            if (KEYBOARD_PAD_0[i].button != button) {
                continue;
            }
            const int scancode = (int)KEYBOARD_PAD_0[i].scancode;
            if (scancode < 0 || scancode >= numkeys) {
                continue;
            }
            if (state[scancode]) {
                return true;
            }
        }
        return false;
    }

    if (sdl_button == SDL_GAMEPAD_BUTTON_INVALID) {
        // L2/R2: аналоговые оси, цифровая трактовка по порогу.
        const int16_t axis = (button == INPUT_PAD_L2)
                                 ? Input_GetPadAxis(pad_index, INPUT_AXIS_L2)
                                 : Input_GetPadAxis(pad_index, INPUT_AXIS_R2);
        return axis >= INPUT_TRIGGER_THRESHOLD;
    }
    return SDL_GetGamepadButton(gamepad, sdl_button);
}

int16_t Input_GetPadAxis(uint32_t pad_index, uint32_t axis)
{
    if (pad_index >= HAL_PC_MAX_PADS || axis >= INPUT_AXIS_COUNT) {
        return 0;
    }
    SDL_Gamepad* gamepad = s_gamepads[pad_index];
    if (gamepad == nullptr) {
        // Клавиатурная эмуляция аналоговых осей не поддерживается.
        return 0;
    }
    const Sint16 raw = SDL_GetGamepadAxis(gamepad, PAD_AXES[axis]);
    return hal_input_scale_axis(raw);
}
