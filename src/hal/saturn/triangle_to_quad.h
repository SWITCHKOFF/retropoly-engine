// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/triangle_to_quad.h
//
// Детерминированная политика triangle→quad для VDP1 (Спринт 27–28, ADR-010).
//
// VDP1 не рисует треугольники с текстурой: только четырёхугольники, и
// текстура отображается целиком на quad. Для треугольника используется
// вырожденный quad: четвёртая вершина дублирует одну из трёх. Политика:
//
//   1. UV каждого угла обязан лежать в пределах 2 текселей от одного из
//      четырёх углов исходного прямоугольника текстуры. Иначе — явный
//      `TRI_QUAD_UNSUPPORTED_UV` (backend рисует flat-Gouraud с диагностикой,
//      silent drop запрещён).
//   2. Три угла обязаны быть различными. Совпадение углов — также
//      `TRI_QUAD_UNSUPPORTED_UV`.
//   3. Исходный прямоугольник — bbox UV, квантованный по X до 8 текселей
//      (требование VDP1), по Y — до 1 текселя.
//   4. Порядок вершин результата — канонический порядок углов текстуры
//      TL, TR, BR, BL. Недостающий угол дублирует следующий назначенный
//      угол (по кругу TL→TR→BR→BL): так VDP1 получает вырожденный quad,
//      а последовательность остаётся циклическим обходом трёх реальных
//      углов.
//
// Модуль host-тестируемый: fixed 16.16, без Jo Engine/SH-2.
#pragma once

#include <stdint.h>

#include "core/math/fixed.h"

namespace saturn {

/** Максимальная сторона текстуры VDP1 (engine-policy .tex v1). */
constexpr uint32_t VDP1_TEXTURE_MAX_DIM = 256u;

/** VDP1 pattern height — 8 бит: 1..255. */
constexpr uint32_t VDP1_PATTERN_MAX_HEIGHT = 255u;

/** Допуск «угол текстуры» в текселях. */
constexpr int32_t TRI_QUAD_CORNER_TOLERANCE_TEXELS = 2;

/** Диагностика конверсии. Порядок — контракт. */
enum TriQuadDiag
{
    TRI_QUAD_OK = 0,
    TRI_QUAD_NULL_ARGUMENT,
    TRI_QUAD_DEGENERATE,
    TRI_QUAD_OUT_OF_SCREEN,
    TRI_QUAD_UNSUPPORTED_UV,
    TRI_QUAD_INVALID_TEXTURE,
    TRI_QUAD_COUNT
};

/** Входная вершина: экран Saturn (y вниз, 0..319/0..223) + UV 0..1 + цвет. */
struct TriQuadVertex
{
    fixed_t x;
    fixed_t y;
    fixed_t u;
    fixed_t v;
    uint8_t r, g, b, a;
};

/** Вход конверсии. */
struct TriQuadInput
{
    TriQuadVertex v[3];
    uint32_t texture_width;   /**< 0 — нетекстурированный путь. */
    uint32_t texture_height;  /**< Для 256 — клампится до 255 (VDP1). */
};

/** Выход: вырожденный quad в порядке углов текстуры TL,TR,BR,BL. */
struct Vdp1Quad
{
    int16_t x[4];
    int16_t y[4];
    uint16_t colors[4];  /**< RGB555 без MSB (Gouraud table). */
    uint16_t u0;         /**< Исходный прямоугольник в текселях. */
    uint16_t v0;
    uint16_t width;      /**< Кратна 8 (VDP1 pattern width). */
    uint16_t height;     /**< 1..255. */
    bool textured;
};

/**
 * Экранные координаты Saturn: reference 320×240, y сдвинут на −8 (crop
 * ADR-008 §4).
 *
 * @return true, если точка внутри 320×224.
 */
static inline bool saturn_screen_contains(fixed_t x, fixed_t y)
{
    return x >= 0 && x < fixed_from_int(320) && y >= 0 && y < fixed_from_int(224);
}

/**
 * Детерминированное округление fixed 16.16 до целого (half away from zero).
 *
 * @param v Значение.
 * @return Округлённое целое (насыщение к int32_t).
 */
int32_t tri_quad_round_fixed(fixed_t v);

/**
 * Конверсия треугольника в VDP1-quad.
 *
 * @param in Вход (не NULL).
 * @param out Выход (не NULL; заполняется только при OK).
 * @param diag Выход: диагностический код (NULL допустим).
 * @return true при успехе; false при явном отказе (out не валиден).
 */
bool triangle_to_quad(const TriQuadInput* in, Vdp1Quad* out, TriQuadDiag* diag);

/** @return Диагностический код строкой. */
const char* tri_quad_diag_string(TriQuadDiag diag);

}  // namespace saturn
