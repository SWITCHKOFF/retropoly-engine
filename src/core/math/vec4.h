// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/math/vec4.h
//
// Однородный вектор 4D в fixed-point (16.16). Нормативные источники:
// docs/RetroEngine_Engineering_Documentation_v1.43.md §4.2/§4.3 (Vec4 —
// аргумент mat4_transform); docs/plans/sprint_01_02.md Неделя 3 ([H] —
// минимальный набор). Редукция dot — один int64-аккумулятор, clamp ровно
// один раз в конце.
#pragma once

#include "core/math/fixed.h"

/** Однородный вектор 4D в 16.16 (POD-структура; layout — контракт §4.3). */
typedef struct
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
    fixed_t w;
} Vec4;

/**
 * Формирование Vec4 из компонент (значения уже в 16.16).
 *
 * @param x Компонента X (16.16).
 * @param y Компонента Y (16.16).
 * @param z Компонента Z (16.16).
 * @param w Однородная компонента W (16.16).
 * @return Сформированный вектор.
 */
static inline Vec4 vec4_make(fixed_t x, fixed_t y, fixed_t z, fixed_t w)
{
    return Vec4{x, y, z, w};
}

/**
 * Компонентное сложение (насыщающее, через fixed_add).
 *
 * @param a Слагаемое.
 * @param b Слагаемое.
 * @return a + b.
 */
static inline Vec4 vec4_add(Vec4 a, Vec4 b)
{
    return Vec4{fixed_add(a.x, b.x), fixed_add(a.y, b.y), fixed_add(a.z, b.z), fixed_add(a.w, b.w)};
}

/**
 * Компонентное вычитание (насыщающее, через fixed_sub).
 *
 * @param a Уменьшаемое.
 * @param b Вычитаемое.
 * @return a − b.
 */
static inline Vec4 vec4_sub(Vec4 a, Vec4 b)
{
    return Vec4{fixed_sub(a.x, b.x), fixed_sub(a.y, b.y), fixed_sub(a.z, b.z), fixed_sub(a.w, b.w)};
}

/**
 * Компонентное умножение на скаляр (насыщающий fixed_mul, Q16.16).
 *
 * @param v Вектор.
 * @param s Скаляр (16.16).
 * @return v·s.
 */
static inline Vec4 vec4_scale(Vec4 v, fixed_t s)
{
    return Vec4{fixed_mul(v.x, s), fixed_mul(v.y, s), fixed_mul(v.z, s), fixed_mul(v.w, s)};
}

/**
 * Скалярное произведение: полный int64-аккумулятор для всей редукции,
 * clamp ровно один раз в конце (контракт single-final-clamp).
 *
 * @param a Вектор A.
 * @param b Вектор B.
 * @return a·b в 16.16 (однократное насыщение).
 */
static inline fixed_t vec4_dot(Vec4 a, Vec4 b)
{
    // Полные Q16.16-произведения держим до конца редукции; накопление —
    // насыщающее к диапазону int64_t (fixed_sat_add_i64, без UB), затем
    // единственный Q16.16-clamp.
    const int64_t acc = fixed_sat_add_i64(fixed_sat_add_i64(fixed_sat_add_i64((int64_t)a.x * (int64_t)b.x,
                                                                    (int64_t)a.y * (int64_t)b.y),
                                              (int64_t)a.z * (int64_t)b.z),
        (int64_t)a.w * (int64_t)b.w);
    return fixed_clamp_i64(acc / FIXED_ONE);
}
