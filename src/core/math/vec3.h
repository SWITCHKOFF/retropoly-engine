// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/math/vec3.h
//
// Вектор 3D в fixed-point (16.16). Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md, §4.2 «ВЕКТОРЫ»;
//   - docs/plans/sprint_01_02.md, Неделя 3.
//
// Контракты:
//   - все редукции (dot/length) накапливают ПОЛНОЕ int64_t-промежуточное и
//     насыщают К РОВНО ОДНОМУ РАЗУ в конце (ReductionUsesSingleFinalClamp);
//   - компонентные операции (add/sub/scale/cross) — int64-промежуточные,
//     насыщающие fixed-функции (детерминированно);
//   - length/normalize — non-inline (src/core/math/math.cpp);
//   - normalize(нулевой вектор) делит на ноль → контракт fixed_div.
#pragma once

#include "core/math/fixed.h"

/** Вектор 3D в 16.16 (POD-структура; layout — нормативный контракт §4.2). */
typedef struct
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
} Vec3;

/**
 * Формирование Vec3 из компонент (без конверсий: значения уже в 16.16).
 *
 * @param x Компонента X (16.16).
 * @param y Компонента Y (16.16).
 * @param z Компонента Z (16.16).
 * @return Сформированный вектор.
 */
static inline Vec3 vec3_make(fixed_t x, fixed_t y, fixed_t z) { return Vec3{x, y, z}; }

/**
 * Компонентное сложение (насыщающее, через fixed_add).
 *
 * @param a Слагаемое.
 * @param b Слагаемое.
 * @return a + b (покомпонентно, насыщение к диапазону int32_t).
 */
static inline Vec3 vec3_add(Vec3 a, Vec3 b)
{
    return Vec3{fixed_add(a.x, b.x), fixed_add(a.y, b.y), fixed_add(a.z, b.z)};
}

/**
 * Компонентное вычитание (насыщающее, через fixed_sub).
 *
 * @param a Уменьшаемое.
 * @param b Вычитаемое.
 * @return a − b (покомпонентно, насыщение к диапазону int32_t).
 */
static inline Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    return Vec3{fixed_sub(a.x, b.x), fixed_sub(a.y, b.y), fixed_sub(a.z, b.z)};
}

/**
 * Компонентное умножение на скаляр насыщающим fixed_mul.
 *
 * @param v Вектор.
 * @param s Скаляр (16.16).
 * @return v·s (покомпонентно, насыщающий масштаб Q16.16).
 */
static inline Vec3 vec3_scale(Vec3 v, fixed_t s)
{
    return Vec3{fixed_mul(v.x, s), fixed_mul(v.y, s), fixed_mul(v.z, s)};
}

/**
 * Редукция трёх попарных произведений с ПОЛНЫМ int64-аккумулятором:
 * clamp применяется ровно один раз в конце (нормативный контракт §4.2).
 * Аккумуляция — насыщающая к диапазону int64_t (fixed_sat_add_i64):
 * без неопределённого поведения знакового переполнения; это защита самого
 * накопителя, а не Q16.16-деление — reduce3 обязан делить на 2^16 и
 * фиксированно-насыщать ровно один раз.
 *
 * @param a0..a2 Компоненты A (16.16).
 * @param b0..b2 Компоненты B (16.16).
 * @return (a0·b0 + a1·b1 + a2·b2) / 2^16 с однократным насыщением.
 */
static inline fixed_t fixed_reduce3_products(fixed_t a0, fixed_t a1, fixed_t a2, fixed_t b0,
    fixed_t b1, fixed_t b2)
{
    // Полные Q16.16-произведения держим до окончания всей редукции.
    const int64_t acc = fixed_sat_add_i64(
        fixed_sat_add_i64((int64_t)a0 * (int64_t)b0, (int64_t)a1 * (int64_t)b1),
        (int64_t)a2 * (int64_t)b2);
    return fixed_clamp_i64(acc / FIXED_ONE);
}

/**
 * Скалярное произведение через single-final-clamp редукцию.
 *
 * @param a Вектор A.
 * @param b Вектор B.
 * @return a·b в 16.16 (строго один clamp в конце редукции).
 */
static inline fixed_t vec3_dot(Vec3 a, Vec3 b)
{
    return fixed_reduce3_products(a.x, a.y, a.z, b.x, b.y, b.z);
}

/**
 * Векторное произведение; редукция по компонентам — int64-промежуточные
 * с насыщающим fixed-функционалом (детерминированно, без UB-переполнений).
 *
 * @param a Вектор A.
 * @param b Вектор B.
 * @return a×b в 16.16.
 */
static inline Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    const int64_t ax = (int64_t)a.y * (int64_t)b.z - (int64_t)a.z * (int64_t)b.y;
    const int64_t ay = (int64_t)a.z * (int64_t)b.x - (int64_t)a.x * (int64_t)b.z;
    const int64_t az = (int64_t)a.x * (int64_t)b.y - (int64_t)a.y * (int64_t)b.x;
    return Vec3{
        fixed_clamp_i64(ax / FIXED_ONE),
        fixed_clamp_i64(ay / FIXED_ONE),
        fixed_clamp_i64(az / FIXED_ONE),
    };
}

/**
 * Квадрат длины через редукцию с одним clamp.
 *
 * @param v Вектор.
 * @return |v|² в 16.16.
 */
static inline fixed_t vec3_length_sq(Vec3 v) { return vec3_dot(v, v); }

/**
 * Длина вектора (детерминированный integer sqrt; fixed_sqrt).
 *
 * @param v Вектор.
 * @return |v| в 16.16.
 */
fixed_t vec3_length(Vec3 v);

/**
 * Нормирование вектора: v / |v|.
 * Нулевой вектор делит на ноль — контракт fixed_div (debug assert,
 * release — насыщенные компоненты).
 *
 * @param v Вектор (ненулевой — нормальный контракт).
 * @return Единичный вектор (длина² ≈ 1.0 в 16.16).
 */
Vec3 vec3_normalize(Vec3 v);
