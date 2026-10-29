// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/math/mat4.h
//
// Матрица 4×4, column-major (как в OpenGL), fixed-point 16.16.
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md, §4.3 «МАТРИЦЫ»;
//   - docs/plans/sprint_01_02.md, Неделя 4.
//
// Контракты:
//   - element (row r, col c) хранится в m[c*4 + r];
//   - mat4_multiply/mat4_transform: редукция — полный int64-аккумулятор на
//     элемент с насыщением к диапазону int64 (fixed_sat_add_i64) и ОДНИМ
//     fixed-clamp в конце (единый стиль векторных редукций);
//   - mat4_multiply безопасна к алиасингу (out == a/b допустимо);
//   - тригонометрия — детерминированная fixed-реализация sin_fixed/cos_fixed
//     (целочисленный полином Тейлора после целочисленного приведения
//     аргумента к [0, π/2]; никаких float);
//   - Mat4 НИКОГДА не передаётся в бэкенд «сырым» кастомом — каждая platform
//     конвертирует в нативное представление (§4.4, Transforms contract).
#pragma once

#include "core/math/fixed.h"
#include "core/math/vec3.h"
#include "core/math/vec4.h"

/** π в представлении 16.16 (ближайшее: 3.14159274...). */
#define FIXED_PI INT32_C(205887)

/** π/2 в представлении 16.16 (ближайшее: 1.57079637...). */
#define FIXED_HALF_PI INT32_C(102944)

/** 2π в представлении 16.16 (ближайшее: 6.28318548...). */
#define FIXED_TWO_PI INT32_C(411775)

/**
 * Детерминированный синус в 16.16 (аргумент — радианы в 16.16).
 * Целочисленное приведение к [0, 2π) (остаток с коррекцией знака), симметричное
 * свёртывание к [0, π/2], оценка целочисленным полиномом Тейлора из 4 членов
 * (x − x³/3! + x⁵/5! − x⁷/7! + x⁹/9!, все операции fixed_mul/fixed_div с
 * насыщением). Реализация — src/core/math/math.cpp.
 *
 * @param angle Угол в радианах (16.16); любой знак и величина.
 * @return sin(angle) в 16.16 (погрешность ≤ ~1e-4 abs во всём периоде).
 */
fixed_t sin_fixed(fixed_t angle);

/**
 * Детерминированный косинус в 16.16 (радианы): cos(angle) = sin(angle + π/2)
 * с тем же свёртыванием (приведение выполняется в int64, до формирования
 * fixed-аргумента). Реализация — src/core/math/math.cpp.
 *
 * @param angle Угол в радианах (16.16); любой знак и величина.
 * @return cos(angle) в 16.16.
 */
fixed_t cos_fixed(fixed_t angle);

/** Матрица 4×4 в column-major порядке (как в OpenGL); layout — контракт §4.3. */
typedef struct
{
    fixed_t m[16];
} Mat4;

/**
 * Заполняет out единичной матрицей (нейтральный элемент умножения).
 *
 * @param out Выходная матрица (не ноль-указатель).
 */
void mat4_identity(Mat4* out);

/**
 * out = a · b (column-major, элементная редукция: 4 произведения в полном
 * int64-аккумуляторе с насыщением к int64 и единственным fixed-клапом).
 * Безопасно при алиасинге (out == a или out == b).
 *
 * @param out Результат (a·b).
 * @param a Левая матрица.
 * @param b Правая матрица.
 */
void mat4_multiply(Mat4* out, const Mat4* a, const Mat4* b);

/**
 * out = матрица переноса (x, y, z): единичная с заполненной 4-й колонкой.
 *
 * @param out Результат.
 * @param x Радиус-вектор по X (16.16).
 * @param y Радиус-вектор по Y (16.16).
 * @param z Радиус-вектор по Z (16.16).
 */
void mat4_translate(Mat4* out, fixed_t x, fixed_t y, fixed_t z);

/**
 * out = матрица поворота вокруг оси X на angle радиан (16.16; детерминированно,
 * через sin_fixed/cos_fixed).
 *
 * @param out Результат.
 * @param angle Угол в радианах (16.16).
 */
void mat4_rotate_x(Mat4* out, fixed_t angle);

/** @copydoc mat4_rotate_x — поворот вокруг оси Y. */
void mat4_rotate_y(Mat4* out, fixed_t angle);

/** @copydoc mat4_rotate_x — поворот вокруг оси Z. */
void mat4_rotate_z(Mat4* out, fixed_t angle);

/**
 * out = перспективная проекция (правосторонняя, OpenGL: камера смотрит в −Z,
 * клип по [near_z, far_z]). Параметры — как в gluPerspective: fov — вертикальный
 * угол (радианы, 16.16, 0 < fov < π), aspect = ширина/высота (> 0),
 * near_z/far_z — дистанции до ближней/дальней плоскостей (0 < near_z < far_z).
 * Имена near_z/far_z (а не near/far — наследие Win16-макросов MSVC).
 *
 * @param out Результат.
 * @param fov Вертикальный угол обзора в радианах (16.16).
 * @param aspect Отношение ширины к высоте (16.16).
 * @param near_z Дистанция до ближней плоскости (16.16, > 0).
 * @param far_z Дистанция до дальней плоскости (16.16, > near_z).
 */
void mat4_perspective(Mat4* out, fixed_t fov, fixed_t aspect, fixed_t near_z, fixed_t far_z);

/**
 * out = видовая матрица «камера в eye, смотрит в target» (правосторонняя,
 * как gluLookAt). Вырожденные входы (target == eye, up ∥ направление,
 * нулевой up) делят на ноль при нормировании — контракт fixed_div
 * (debug assert, release — насыщенные компоненты).
 *
 * @param out Результат (view-матрица).
 * @param eye Позиция камеры.
 * @param target Точка взгляда.
 * @param up Направление «вверх» (ненулевое, неколлинеарное направлению).
 */
void mat4_look_at(Mat4* out, Vec3 eye, Vec3 target, Vec3 up);

/**
 * Преобразование однородного вектора: out = M · v (покомпонентная редукция:
 * 4 произведения на полный int64-аккумулятор, один fixed-clamp на компоненту).
 *
 * @param m Матрица.
 * @param v Однородный вектор (16.16; обычная точка — w = FIXED_ONE).
 * @return Преобразованный вектор (16.16).
 */
Vec4 mat4_transform(const Mat4* m, Vec4 v);
