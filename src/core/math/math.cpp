// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/math/math.cpp
//
// Non-inline реализации математического ядра (vec/trig/mat).
// Нормативно: docs/RetroEngine_Engineering_Documentation_v1.43.md §4.2–§4.4;
// docs/plans/sprint_01_02.md (Неделя 3 — vec length/normalize; Неделя 4 —
// детерминированная тригонометрия, Mat3/Mat4).
#include "core/math/vec3.h"

#include "core/math/mat3.h"
#include "core/math/mat4.h"

// ============================================================================
// Векторы (Спринт 1–2 Неделя 3)
// ============================================================================

fixed_t vec3_length(Vec3 v)
{
    // Детерминированный integer sqrt от редукции с одним clamp.
    return fixed_sqrt(vec3_length_sq(v));
}

Vec3 vec3_normalize(Vec3 v)
{
    const fixed_t len = vec3_length(v);
    // Деление нулевой длины — контракт fixed_div (debug assert / release
    // насыщение); без собственных веток — единое поведение с ядром.
    return Vec3{fixed_div(v.x, len), fixed_div(v.y, len), fixed_div(v.z, len)};
}

// ============================================================================
// Тригонометрия (Спринт 1–2 Неделя 4): sin_fixed / cos_fixed
// ============================================================================

namespace
{
/**
 * Целочисленная редукция угла к [0, π/2] + симметричное свёртывание знака,
 * затем полином Тейлора (4 члена): sin(x) ≈ x − x³/3! + x⁵/5! − x⁷/7! + x⁹/9!.
 * Все операции — fixed_mul/fixed_div (насыщающие, детерминированные); никакой
 * плавающей точки. Погрешность: обрезка ряда ≤ x¹¹/11! < 2.7e-6 на [0, π/2],
 * округление fixed ≤ ~1e-4 abs во всём периоде.
 */
fixed_t sin_i64(int64_t a)
{
    // Периодичность: [0, 2π) (остаток с коррекцией знака — floor-мод).
    a %= FIXED_TWO_PI;
    if (a < 0)
        a += FIXED_TWO_PI;

    int neg = 0;
    if (a >= FIXED_PI)
    {
        a -= FIXED_PI;
        neg = 1;
    }
    // Свёртывание к [0, π/2]: sin(π − x) = sin(x); a ≤ FIXED_PI здесь.
    if (a > FIXED_HALF_PI)
        a = FIXED_PI - a;

    const fixed_t x = (fixed_t)a;
    const fixed_t x2 = fixed_mul(x, x);
    const fixed_t x3 = fixed_mul(x2, x);
    const fixed_t x5 = fixed_mul(x3, x2);
    const fixed_t x7 = fixed_mul(x5, x2);
    const fixed_t x9 = fixed_mul(x7, x2);
    // Деление на ЦЕЛОЧИСЛЕННУЮ константу n (не fixed-число!): v_value = x_value / n
    // соответствует raw v = x_raw / n (без домножения на FIXED_ONE).
    // x_raw ≤ (~π/2)⁹·2^45 < 2^45 → без переполнения int64.
    const fixed_t t3 = (fixed_t)(x3 / 6);
    const fixed_t t5 = (fixed_t)(x5 / 120);
    const fixed_t t7 = (fixed_t)(x7 / 5040);
    const fixed_t t9 = (fixed_t)(x9 / 362880);

    fixed_t result = fixed_sub(x, t3);
    result = fixed_add(result, t5);
    result = fixed_sub(result, t7);
    result = fixed_add(result, t9);
    // Знак четверти; |result| ≤ FIXED_ONE → насыщение не задействовано.
    return neg ? fixed_sub(0, result) : result;
}
} // namespace

fixed_t sin_fixed(fixed_t angle) { return sin_i64((int64_t)angle); }

fixed_t cos_fixed(fixed_t angle)
{
    // cos(a) = sin(a + π/2); смещение считается в int64 — до фиксации fixed-аргумента.
    return sin_i64((int64_t)angle + FIXED_HALF_PI);
}

// ============================================================================
// Mat4 (Спринт 1–2 Неделя 4)
// ============================================================================

void mat4_identity(Mat4* out)
{
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            out->m[c * 4 + r] = (r == c) ? FIXED_ONE : 0;
        }
    }
}

void mat4_multiply(Mat4* out, const Mat4* a, const Mat4* b)
{
    // Aliasing-safe: все 16 аккумуляторов формируются до операций записи.
    int64_t acc[16];
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            // out(r,c) = Σ_k a(r,k)·b(k,c) в column-major: a.m[k*4+r]·b.m[c*4+k].
            int64_t sum = (int64_t)a->m[0 * 4 + r] * (int64_t)b->m[c * 4 + 0];
            sum = fixed_sat_add_i64(sum, (int64_t)a->m[1 * 4 + r] * (int64_t)b->m[c * 4 + 1]);
            sum = fixed_sat_add_i64(sum, (int64_t)a->m[2 * 4 + r] * (int64_t)b->m[c * 4 + 2]);
            sum = fixed_sat_add_i64(sum, (int64_t)a->m[3 * 4 + r] * (int64_t)b->m[c * 4 + 3]);
            acc[c * 4 + r] = sum;
        }
    }
    for (int i = 0; i < 16; ++i)
    {
        out->m[i] = fixed_clamp_i64(acc[i] / FIXED_ONE);
    }
}

void mat4_translate(Mat4* out, fixed_t x, fixed_t y, fixed_t z)
{
    mat4_identity(out);
    out->m[12] = x;
    out->m[13] = y;
    out->m[14] = z;
}

void mat4_rotate_x(Mat4* out, fixed_t angle)
{
    const fixed_t s = sin_fixed(angle);
    const fixed_t c = cos_fixed(angle);
    mat4_identity(out);
    out->m[5] = c;  // col1 row1
    out->m[6] = s;  // col1 row2
    out->m[9] = -s; // col2 row1 (int32: детерминированное точное отрицание)
    out->m[10] = c; // col2 row2
}

void mat4_rotate_y(Mat4* out, fixed_t angle)
{
    const fixed_t s = sin_fixed(angle);
    const fixed_t c = cos_fixed(angle);
    mat4_identity(out);
    out->m[0] = c;  // col0 row0
    out->m[2] = -s; // col0 row2
    out->m[8] = s;  // col2 row0
    out->m[10] = c; // col2 row2
}

void mat4_rotate_z(Mat4* out, fixed_t angle)
{
    const fixed_t s = sin_fixed(angle);
    const fixed_t c = cos_fixed(angle);
    mat4_identity(out);
    out->m[0] = c;  // col0 row0
    out->m[1] = s;  // col0 row1
    out->m[4] = -s; // col1 row0
    out->m[5] = c;  // col1 row1
}

void mat4_perspective(Mat4* out, fixed_t fov, fixed_t aspect, fixed_t near_z, fixed_t far_z)
{
    // f = cot(fov/2); все операнды в диапазоне, насыщение не задействовано.
    const fixed_t half_fov = fixed_div(fov, fixed_from_int(2));
    const fixed_t f = fixed_div(cos_fixed(half_fov), sin_fixed(half_fov));
    const fixed_t den = fixed_sub(near_z, far_z); // отрицательный: far > near
    const fixed_t num_z = fixed_add(far_z, near_z);
    const fixed_t proj = fixed_mul(fixed_mul(near_z, far_z), fixed_from_int(2));

    // Диагональ + Z-колонка (column-major, OpenGL gluPerspective):
    out->m[0] = fixed_div(f, aspect);
    out->m[5] = f;
    out->m[10] = fixed_div(num_z, den); // (far+near)/(near−far)
    // [3][2] = −1 (заведующий w-компонентой).
    out->m[11] = -FIXED_ONE;
    out->m[14] = fixed_div(proj, den); // 2·far·near/(near−far)
    // Нулевые элементы.
    out->m[1] = out->m[2] = out->m[3] = 0;
    out->m[4] = out->m[6] = out->m[7] = 0;
    out->m[8] = out->m[9] = 0;
    out->m[12] = out->m[13] = 0;
    out->m[15] = 0;
}

void mat4_look_at(Mat4* out, Vec3 eye, Vec3 target, Vec3 up)
{
    // gluLookAt: f = normalize(target − eye); s = normalize(f × up); u = s × f.
    const Vec3 f_dir = vec3_normalize(vec3_sub(target, eye));
    const Vec3 s_dir = vec3_normalize(vec3_cross(f_dir, up));
    const Vec3 u_dir = vec3_cross(s_dir, f_dir);

    // Столбцы [sᵀ | uᵀ | −fᵀ | ] по колонкам 0..2; перенос — минус скалярные
    // произведения базиса на eye (column-major: m[0]=s.x m[1]=u.x m[2]=−f.x …).
    out->m[0] = s_dir.x;
    out->m[1] = u_dir.x;
    out->m[2] = -f_dir.x;
    out->m[3] = 0;
    out->m[4] = s_dir.y;
    out->m[5] = u_dir.y;
    out->m[6] = -f_dir.y;
    out->m[7] = 0;
    out->m[8] = s_dir.z;
    out->m[9] = u_dir.z;
    out->m[10] = -f_dir.z;
    out->m[11] = 0;
    out->m[12] = fixed_sub(0, vec3_dot(s_dir, eye));
    out->m[13] = fixed_sub(0, vec3_dot(u_dir, eye));
    out->m[14] = vec3_dot(f_dir, eye);
    out->m[15] = FIXED_ONE;
}

Vec4 mat4_transform(const Mat4* m, Vec4 v)
{
    // Покомпонентная редукция: 4 произведения на полный int64-аккумулятор,
    // один fixed-clamp на компоненту (column-major: row r = m[c*4+r] по c).
    const int64_t x_acc = fixed_sat_add_i64(fixed_sat_add_i64((int64_t)m->m[0] * (int64_t)v.x,
                                                (int64_t)m->m[4] * (int64_t)v.y),
        fixed_sat_add_i64((int64_t)m->m[8] * (int64_t)v.z, (int64_t)m->m[12] * (int64_t)v.w));
    const int64_t y_acc = fixed_sat_add_i64(fixed_sat_add_i64((int64_t)m->m[1] * (int64_t)v.x,
                                                (int64_t)m->m[5] * (int64_t)v.y),
        fixed_sat_add_i64((int64_t)m->m[9] * (int64_t)v.z, (int64_t)m->m[13] * (int64_t)v.w));
    const int64_t z_acc = fixed_sat_add_i64(fixed_sat_add_i64((int64_t)m->m[2] * (int64_t)v.x,
                                                (int64_t)m->m[6] * (int64_t)v.y),
        fixed_sat_add_i64((int64_t)m->m[10] * (int64_t)v.z, (int64_t)m->m[14] * (int64_t)v.w));
    const int64_t w_acc = fixed_sat_add_i64(fixed_sat_add_i64((int64_t)m->m[3] * (int64_t)v.x,
                                                (int64_t)m->m[7] * (int64_t)v.y),
        fixed_sat_add_i64((int64_t)m->m[11] * (int64_t)v.z, (int64_t)m->m[15] * (int64_t)v.w));

    return Vec4{
        fixed_clamp_i64(x_acc / FIXED_ONE),
        fixed_clamp_i64(y_acc / FIXED_ONE),
        fixed_clamp_i64(z_acc / FIXED_ONE),
        fixed_clamp_i64(w_acc / FIXED_ONE),
    };
}

// ============================================================================
// Mat3 (rotation-подмножество для GTE-конверсии, Спринты 15–16)
// ============================================================================

void mat3_identity(Mat3* out)
{
    for (int c = 0; c < 3; ++c)
    {
        for (int r = 0; r < 3; ++r)
        {
            out->m[c * 3 + r] = (r == c) ? FIXED_ONE : 0;
        }
    }
}

void mat3_from_mat4(Mat3* out, const Mat4* src)
{
    // Верхние 3×3 (rotation) без 4-й строки/колонки.
    for (int c = 0; c < 3; ++c)
    {
        for (int r = 0; r < 3; ++r)
        {
            out->m[c * 3 + r] = src->m[c * 4 + r];
        }
    }
}

void mat3_multiply(Mat3* out, const Mat3* a, const Mat3* b)
{
    int64_t acc[9];
    for (int c = 0; c < 3; ++c)
    {
        for (int r = 0; r < 3; ++r)
        {
            int64_t sum = (int64_t)a->m[0 * 3 + r] * (int64_t)b->m[c * 3 + 0];
            sum = fixed_sat_add_i64(sum, (int64_t)a->m[1 * 3 + r] * (int64_t)b->m[c * 3 + 1]);
            sum = fixed_sat_add_i64(sum, (int64_t)a->m[2 * 3 + r] * (int64_t)b->m[c * 3 + 2]);
            acc[c * 3 + r] = sum;
        }
    }
    for (int i = 0; i < 9; ++i)
    {
        out->m[i] = fixed_clamp_i64(acc[i] / FIXED_ONE);
    }
}

Vec3 mat3_transform(const Mat3* m, Vec3 v)
{
    const int64_t x_acc = fixed_sat_add_i64((int64_t)m->m[0] * (int64_t)v.x,
        fixed_sat_add_i64((int64_t)m->m[3] * (int64_t)v.y, (int64_t)m->m[6] * (int64_t)v.z));
    const int64_t y_acc = fixed_sat_add_i64((int64_t)m->m[1] * (int64_t)v.x,
        fixed_sat_add_i64((int64_t)m->m[4] * (int64_t)v.y, (int64_t)m->m[7] * (int64_t)v.z));
    const int64_t z_acc = fixed_sat_add_i64((int64_t)m->m[2] * (int64_t)v.x,
        fixed_sat_add_i64((int64_t)m->m[5] * (int64_t)v.y, (int64_t)m->m[8] * (int64_t)v.z));

    return Vec3{
        fixed_clamp_i64(x_acc / FIXED_ONE),
        fixed_clamp_i64(y_acc / FIXED_ONE),
        fixed_clamp_i64(z_acc / FIXED_ONE),
    };
}
