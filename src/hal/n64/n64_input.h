// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_input.h
//
// Ввод N64 (Спринт 41–42): libdragon joypad → engine INPUT_PAD_*.
//
// Контракт опроса: Platform_PumpEvents вызывает input_poll() РОВНО ОДИН раз
// за кадр; HAL-запросы читают защёлкнутые снимки портов. Нет контроллера /
// неподдерживаемое устройство — все кнопки отпущены, оси 0 (без краша).
//
// Маппинг (engine policy, docs/N64.md § input):
//   D-pad  → INPUT_PAD_UP/DOWN/LEFT/RIGHT
//   A      → INPUT_PAD_CROSS
//   B      → INPUT_PAD_CIRCLE
//   Z      → INPUT_PAD_SQUARE
//   C-Up   → INPUT_PAD_TRIANGLE
//   C-Down → INPUT_PAD_SELECT
//   C-Left → INPUT_PAD_L2
//   C-Right→ INPUT_PAD_R2
//   L/R    → INPUT_PAD_L1/R1
//   Start  → INPUT_PAD_START
//   stick  → INPUT_AXIS_LEFTX/LEFTY; C-stick → RIGHTX/RIGHTY;
//            analog L/R → L2/R2 (0..127). Y-оси инвертируются в SDL-
//            конвенцию (вверх = отрицательное значение).
//
// Модуль не включает libdragon: источник — C-мост, host-тесты подменяют
// его fake_n64_bridge.cpp.
#pragma once

#include <stdint.h>

#include "hal/platform.h"

namespace n64 {

/** Мёртвая зона стика (единицы −127..127). */
constexpr int16_t INPUT_DEADZONE = 8;

/** Число портов N64. */
constexpr uint32_t INPUT_PORT_COUNT = 4u;

/** Сброс защёлки (идемпотентно; не вызывает bridge). */
void input_init();

/** Сброс защёлки (идемпотентно). */
void input_shutdown();

/** true — input_init выполнен. */
bool input_ready();

/**
 * Ровно один опрос контроллеров: bridge joypad_poll + снимок 4 портов.
 * Вызывается из Platform_PumpEvents один раз за кадр.
 */
void input_poll();

/** @return Число опросов с момента input_init (диагностика/тесты). */
uint32_t input_poll_count();

/** @return true — на момент последнего poll() порт содержал контроллер. */
bool input_connected(uint32_t port);

/** @return Защёлкнутая маска RPE_N64_BTN_* порта @p port (0 вне диапазона). */
uint32_t input_buttons(uint32_t port);

/**
 * Состояние кнопки engine-раскладки на порту @p port.
 *
 * @return true при нажатии; false для пустого/вне диапазона порта.
 */
bool input_pad_button_down(uint32_t port, PadButton button);

/**
 * Значение оси engine-раскладки на порту @p port.
 *
 * @return −128..127 (триггеры 0..127); 0 для пустого/вне диапазона порта.
 */
int16_t input_pad_axis(uint32_t port, uint32_t axis);

/**
 * Чистая функция маппинга битов N64 → engine PadButton.
 *
 * @param buttons Маска RPE_N64_BTN_*.
 * @param button engine-кнопка.
 * @return true при нажатии.
 */
bool input_map_button(uint32_t buttons, PadButton button);

/**
 * Чистая функция мёртвой зоны/инверсии стика.
 *
 * @param raw Сырое значение (−127..127).
 * @param invert_y Инверсия Y в SDL-конвенцию.
 * @return Значение после deadzone (−127..127).
 */
int16_t input_apply_deadzone(int16_t raw, bool invert_y);

}  // namespace n64
