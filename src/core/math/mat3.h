// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/math/mat3.h
//
// Матрица 3×3 (rotation-подмножество Mat4) — для будущей конверсии в
// GTE-представление PS1 (Спринты 15–16; docs/plans/sprint_15_16.md).
// Нормативно: docs/RetroEngine_Engineering_Documentation_v1.43.md §4.3/§4.4
// (engine Mat3 → GTE через backend-адаптер, сырой кастом запрещён);
// docs/plans/sprint_01_02.md Неделя 4.
// Редукция — полный int64-аккумулятор (fixed_sat_add_i64), один fixed-clamp.
#pragma once

#include "core/math/fixed.h"
#include "core/math/mat4.h" // Mat4 для mat3_from_mat4 (односторонняя зависимость)
#include "core/math/vec3.h"

/** Матрица 3×3 в column-major порядке: element (row r, col c) → m[c*3 + r]. */
typedef struct
{
    fixed_t m[9];
} Mat3;

/**
 * Заполняет out единичной матрицей 3×3.
 *
 * @param out Выходная матрица.
 */
void mat3_identity(Mat3* out);

/**
 * Извлекает верхнюю 3×3 подматрицу Mat4 (rotation-часть без переноса/проекции).
 * ВНИМАНИЕ: осмысленно для Mat4 без масштаба/проекции (GTE-rotation конверсия).
 *
 * @param out Результат (3×3).
 * @param src Исходная матрица 4×4 (column-major).
 */
void mat3_from_mat4(Mat3* out, const Mat4* src);

/**
 * out = a · b (элементная редукция: 3 произведения в полном int64-аккумуляторе,
 * насыщение к int64, единственный fixed-клап; безопасно при алиасинге).
 *
 * @param out Результат (a·b).
 * @param a Левая матрица.
 * @param b Правая матрица.
 */
void mat3_multiply(Mat3* out, const Mat3* a, const Mat3* b);

/**
 * Преобразование вектора 3D: out = M · v (редукция 3 произведения + один clamp).
 *
 * @param m Матрица 3×3.
 * @param v Вектор (16.16).
 * @return Преобразованный вектор.
 */
Vec3 mat3_transform(const Mat3* m, Vec3 v);
