// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_input.h
//
// 6-button Saturn pad: active-low raw state → INPUT_PAD_* (Спринт 23–24,
// неделя 46). Модуль host-тестируемый: источник пада — инъектируемая
// функция, поэтому тесты не линкуют Jo Engine.
//
// Контракт опроса: Platform_PumpEvents вызывает input_poll() РОВНО ОДИН раз
// за кадр; HAL-запросы читают защёлкнутое состояние. Нет пада / пад не
// RegularGamepad — все кнопки отпущены (безопасные значения).
#pragma once

#include <stdint.h>

#include "hal/platform.h"

namespace saturn {

/** Сырое состояние порта 1, возвращаемое bridge'ом. */
struct RawPadState
{
    uint16_t data;     /**< Активные-низкие биты SMPC (0 — нажато). */
    uint8_t connected; /**< Пад/периферия подключена. */
    uint8_t regular;   /**< Именно RegularGamepad (6-button pad). */
};

/** Источник сырого состояния пада (bridge на Saturn, fake в тестах). */
typedef RawPadState (*RawPadSource)(void);

/**
 * Инициализация модуля и установка источника.
 *
 * @param source Источник; nullptr — модуль всегда возвращает «нет пада».
 */
void input_init(RawPadSource source);

/** Сброс защёлки и источника (идемпотентно). */
void input_shutdown(void);

/** Ровно один опрос источника за кадр. */
void input_poll(void);

/** @return Защёлкнутые активные-высокие биты RPE_SATURN_PAD_*. */
uint16_t input_buttons(void);

/** @return true, если на момент последнего poll() был RegularGamepad. */
bool input_is_connected(void);

/** @return true, если на момент последнего poll() был именно regular pad. */
bool input_is_regular(void);

/** @return Число вызовов input_poll() с момента init (диагностика/тесты). */
uint32_t input_poll_count(void);

/**
 * Состояние кнопки engine-раскладки на паде @p pad_index.
 *
 * @return true при нажатии; false для pad_index != 0, Select (нет на
 *         6-button pad) и невалидных значений.
 */
bool input_pad_button_down(uint32_t pad_index, PadButton button);

/**
 * Маппинг active-low raw → active-high RPE-биты (чистая функция).
 *
 * @param raw Сырые биты SMPC.
 * @param connected Флаг подключения.
 * @param regular Флаг RegularGamepad.
 * @return Биты RPE_SATURN_PAD_* (0 при disconnected/non-regular).
 */
uint16_t input_map_raw(uint16_t raw, bool connected, bool regular);

}  // namespace saturn
