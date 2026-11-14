// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_render_utils.h
//
// Детерминированные утилиты render-skeleton Saturn (Спринт 23–24, неделя 48):
// конвертация engine RGB888 → VDP2 RGB555 и golden-хэш кадра очистки.
// Модуль host-тестируемый; Jo Engine/SH-2 не включает.
#pragma once

#include <stdint.h>

namespace saturn {

/** Цвет очистки по умолчанию (RENDER_CLEAR_DEFAULT_* в ядре: 0,0,32). */
constexpr uint8_t CLEAR_DEFAULT_R = 0u;
constexpr uint8_t CLEAR_DEFAULT_G = 0u;
constexpr uint8_t CLEAR_DEFAULT_B = 32u;

/** RGB555 + MSB (формат jo_color): 0BBBBBGGGGGRRRRR | 0x8000. */
constexpr uint16_t CLEAR_GOLDEN_WORD = 0x9000u;

/** FNV-1a 32 по 320×224 LE-байтам golden-слова. */
constexpr uint32_t CLEAR_GOLDEN_HASH = 0xC4AE5DC5u;

constexpr uint32_t SCREEN_WIDTH = 320u;
constexpr uint32_t SCREEN_HEIGHT = 224u;

/**
 * Конвертация 8-битных компонент в Saturn RGB555 с установленным MSB.
 *
 * @param r,g,b Компоненты 0..255.
 * @return 16-битное слово VDP2 (R[4:0], G[9:5], B[14:10], MSB=1).
 */
uint16_t rgb888_to_saturn(uint8_t r, uint8_t g, uint8_t b);

/**
 * FNV-1a 32 по слову @p word, повторённому width×height раз в LE-порядке.
 *
 * @param word Цвет пикселя RGB555.
 * @param width Ширина кадра.
 * @param height Высота кадра.
 * @return 32-битный хэш.
 */
uint32_t frame_word_hash(uint16_t word, uint32_t width, uint32_t height);

/** @return Хэш golden-кадра очистки (0,0,32) 320×224. */
uint32_t clear_golden_hash(void);

}  // namespace saturn
