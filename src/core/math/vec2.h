// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/math/vec2.h
//
// Вектор 2D в fixed-point (16.16). Нормативный источник:
// docs/RetroEngine_Engineering_Documentation_v1.43.md §4.2;
// docs/plans/sprint_01_02.md Неделя 3 ([H] — минимальный набор).
// Редукция (dot) — один int64-аккумулятор, clamp ровно один раз.
#pragma once

#include "core/math/fixed.h"

/** Вектор 2D в 16.16 (POD-структура; layout — нормативный контракт). */
typedef struct
{
    fixed_t x;
    fixed_t y;
} Vec2;

/**
 * Формирование Vec2 из компонент (значения уже в 16.16).
 *
 * @param x Компонента X (16.16).
 * @param y Компонента Y (16.16).
 * @return Сформированный вектор.
 */
static inline Vec2 vec2_make(fixed_t x, fixed_t y) { return Vec2{x, y}; }

/**
 * Компонентное сложение (насыщающее, через fixed_add).
 *
 * @param a Слагаемое.
 * @param b Слагаемое.
 * @return a + b.
 */
static inline Vec2 vec2_add(Vec2 a, Vec2 b) { return Vec2{fixed_add(a.x, b.x), fixed_add(a.y, b.y)}; }

/**
 * Компонентное вычитание (насыщающее, через fixed_sub).
 *
 * @param a Уменьшаемое.
 * @param b Вычитаемое.
 * @return a − b.
 */
static inline Vec2 vec2_sub(Vec2 a, Vec2 b) { return Vec2{fixed_sub(a.x, b.x), fixed_sub(a.y, b.y)}; }

/**
 * Компонентное умножение на скаляр (насыщающий fixed_mul, масштаб Q16.16).
 *
 * @param v Вектор.
 * @param s Скаляр (16.16).
 * @return v·s.
 */
static inline Vec2 vec2_scale(Vec2 v, fixed_t s) { return Vec2{fixed_mul(v.x, s), fixed_mul(v.y, s)}; }

/**
 * Скалярное произведение: полный int64-аккумулятор, clamp ровно один раз.
 *
 * @param a Вектор A.
 * @param b Вектор B.
 * @return a·b в 16.16 (однократное насыщение в конце редукции).
 */
static inline fixed_t vec2_dot(Vec2 a, Vec2 b)
{
    // Полные Q16.16-произведения до конца редукции: пара int32-произведений
    // не выходит за диапазон int64 (2·MAX² ≤ INT64_MAX), накопление
    // детерминированно; насыщающий аккумулятор — единый стиль редукций.
    const int64_t acc = fixed_sat_add_i64((int64_t)a.x * (int64_t)b.x, (int64_t)a.y * (int64_t)b.y);
    return fixed_clamp_i64(acc / FIXED_ONE);
}
