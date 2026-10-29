// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/math/vec3_test.cpp
//
// Юнит-тесты Vec3 (16.16) — docs/plans/sprint_01_02.md, Неделя 3.
// Эталонные значения — ручная математика; детерминизм — на raw-значениях.
#include "core/math/vec3.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <climits>

// ----------------------------------------------------------------------------
// Компонентные операции
// ----------------------------------------------------------------------------

TEST(Vec3, MakeAddSubScalePreciseValues)
{
    const Vec3 a = vec3_make(fixed_from_int(1), fixed_from_int(2), fixed_from_int(3));
    const Vec3 b = vec3_make(fixed_from_int(4), fixed_from_int(5), fixed_from_int(6));

    const Vec3 sum = vec3_add(a, b);
    EXPECT_EQ(sum.x, fixed_from_int(5));
    EXPECT_EQ(sum.y, fixed_from_int(7));
    EXPECT_EQ(sum.z, fixed_from_int(9));

    const Vec3 diff = vec3_sub(b, a);
    EXPECT_EQ(diff.x, fixed_from_int(3));
    EXPECT_EQ(diff.y, fixed_from_int(3));
    EXPECT_EQ(diff.z, fixed_from_int(3));

    const Vec3 scaled = vec3_scale(a, fixed_from_int(2));
    EXPECT_EQ(scaled.x, fixed_from_int(2));
    EXPECT_EQ(scaled.y, fixed_from_int(4));
    EXPECT_EQ(scaled.z, fixed_from_int(6));
}

TEST(Vec3, ComponentSaturates)
{
    const Vec3 big = vec3_make(INT32_MAX, INT32_MAX, INT32_MIN);
    const Vec3 one = vec3_make(FIXED_ONE, FIXED_ONE, -FIXED_ONE);
    const Vec3 sum = vec3_add(big, one);
    EXPECT_EQ(sum.x, INT32_MAX);
    EXPECT_EQ(sum.y, INT32_MAX);
    EXPECT_EQ(sum.z, INT32_MIN);
}

// ----------------------------------------------------------------------------
// Dot: редукция с одним clamp
// ----------------------------------------------------------------------------

TEST(Vec3, DotPreciseValues)
{
    // Нормативный вектор: (1,2,3)·(4,5,6) = 4+10+18 = 32.
    const Vec3 a = vec3_make(fixed_from_int(1), fixed_from_int(2), fixed_from_int(3));
    const Vec3 b = vec3_make(fixed_from_int(4), fixed_from_int(5), fixed_from_int(6));
    EXPECT_EQ(vec3_dot(a, b), fixed_from_int(32));

    // Независимое float-ожидание (ручная редукция).
    const Vec3 c = vec3_make(fixed_from_float(1.5f), fixed_from_float(-2.0f), fixed_from_int(4));
    const Vec3 d = vec3_make(fixed_from_float(2.0f), fixed_from_float(3.0f), fixed_from_float(0.25f));
    // 1.5·2 + (−2)·3 + 4·0.25 = 3 − 6 + 1 = −2.
    EXPECT_NEAR(fixed_to_float(vec3_dot(c, d)), -2.0, 0.001);
}

TEST(Vec3, ReductionUsesSingleFinalClamp)
{
    // Нормативный вектор: FULL int64-аккумулятор, clamp ровно один раз.
    // (INT32_MAX,INT32_MAX,INT32_MAX)·(ONE,ONE,ONE):
    // точная редукция 3·INT32_MAX·2^16/2^16 = 3·(INT32_MAX) → насыщение INT32_MAX.
    // Промежуточные per-component clamp'ы дали бы результат той же величины,
    // но
    // отдельные переполнения отсутствуют: результат строго INT32_MAX.
    const Vec3 a = vec3_make(INT32_MAX, INT32_MAX, INT32_MAX);
    const Vec3 b = vec3_make(FIXED_ONE, FIXED_ONE, FIXED_ONE);
    EXPECT_EQ(vec3_dot(a, b), INT32_MAX);

    // Отрицательный аналог: (INT32_MIN×3) → INT32_MIN.
    const Vec3 neg = vec3_make(INT32_MIN, INT32_MIN, INT32_MIN);
    EXPECT_EQ(vec3_dot(neg, b), INT32_MIN);

    // length_sq(экстремального вектора)
    // тоже идёт через ту же редукцию: |v|² = INT32_MAX.
    EXPECT_EQ(vec3_length_sq(a), INT32_MAX);
}

// ----------------------------------------------------------------------------
// Cross
// ----------------------------------------------------------------------------

TEST(Vec3, CrossOrthonormalBasis)
{
    const Vec3 e1 = vec3_make(FIXED_ONE, 0, 0);
    const Vec3 e2 = vec3_make(0, FIXED_ONE, 0);
    const Vec3 e3 = vec3_make(0, 0, FIXED_ONE);

    // Нормативный вектор: e1×e2 = e3 (точное raw-сравнение).
    const Vec3 c12 = vec3_cross(e1, e2);
    EXPECT_EQ(c12.x, 0);
    EXPECT_EQ(c12.y, 0);
    EXPECT_EQ(c12.z, FIXED_ONE);

    const Vec3 c23 = vec3_cross(e2, e3);
    EXPECT_EQ(c23.x, FIXED_ONE);
    EXPECT_EQ(c23.y, 0);
    EXPECT_EQ(c23.z, 0);

    const Vec3 c31 = vec3_cross(e3, e1);
    EXPECT_EQ(c31.x, 0);
    EXPECT_EQ(c31.y, FIXED_ONE);
    EXPECT_EQ(c31.z, 0);
}

TEST(Vec3, CrossAntiCommutative)
{
    const Vec3 a = vec3_make(fixed_from_float(1.5f), fixed_from_int(-3), fixed_from_float(0.25f));
    const Vec3 b = vec3_make(fixed_from_float(-0.5f), fixed_from_float(2.0f), fixed_from_int(7));
    const Vec3 ab = vec3_cross(a, b);
    const Vec3 ba = vec3_cross(b, a);
    EXPECT_EQ(ab.x, -ba.x); // в int32 нет «−0»: нулевая компонента == нулевая
    EXPECT_EQ(ab.y, -ba.y);
    EXPECT_EQ(ab.z, -ba.z);
}

// ----------------------------------------------------------------------------
// Length / Normalize
// ----------------------------------------------------------------------------

TEST(Vec3, LengthPreciseValues)
{
    // 3-4-5 треугольник: точная целая редукция.
    const Vec3 v = vec3_make(fixed_from_int(3), fixed_from_int(4), 0);
    EXPECT_EQ(vec3_length(v), fixed_from_int(5));
    EXPECT_EQ(vec3_length_sq(v), fixed_from_int(25));

    EXPECT_EQ(vec3_length(vec3_make(0, 0, 0)), 0);
    EXPECT_EQ(vec3_length(vec3_make(fixed_from_int(0), fixed_from_int(5), fixed_from_int(0))),
        fixed_from_int(5));
}

TEST(Vec3, NormalizeUnitLength)
{
    const Vec3 v = vec3_make(fixed_from_int(3), fixed_from_int(4), 0);
    const Vec3 n = vec3_normalize(v);

    // 3/5, 4/5, 0; |n|² ≈ FIXED_ONE (допуск в float-единицах: нормирование
    // 16.16 даёт ошибку ≤ ~1e-4 — строго меньше 0.02).
    EXPECT_NEAR(fixed_to_float(n.x), 0.6, 0.001);
    EXPECT_NEAR(fixed_to_float(n.y), 0.8, 0.001);
    EXPECT_EQ(n.z, 0);
    EXPECT_NEAR(fixed_to_float(vec3_length_sq(n)), 1.0, 0.02);
}

TEST(Vec3, NormalizePreciseRawValues)
{
    // (2,0,0): length² = 4 (raw без переполнения), √4 = 2, 2/2 = 1 —
    // попарно точные raw-значения: normalize((2,0,0)) == (1,0,0).
    const Vec3 n1 = vec3_normalize(vec3_make(fixed_from_int(2), 0, 0));
    EXPECT_EQ(n1.x, FIXED_ONE);
    EXPECT_EQ(n1.y, 0);
    EXPECT_EQ(n1.z, 0);
}

#if defined(NDEBUG)
TEST(Vec3, NormalizeZeroReleaseContract)
{
    // Release: контракт fixed_div(b==0) без падения: a==0 ≥ 0 → INT32_MAX.
    const Vec3 n = vec3_normalize(vec3_make(0, 0, 0));
    EXPECT_EQ(n.x, INT32_MAX);
    EXPECT_EQ(n.y, INT32_MAX);
    EXPECT_EQ(n.z, INT32_MAX);
}
#else
TEST(Vec3, NormalizeZeroDebugAsserts)
{
    // Debug: normalize(0) проходит через div на ноль → assert (контракт).
    EXPECT_DEATH(vec3_normalize(vec3_make(0, 0, 0)), "fixed_div");
}
#endif

// ----------------------------------------------------------------------------
// Детерминизм
// ----------------------------------------------------------------------------

TEST(Vec3, DeterminismSameInputSameRawOutput)
{
    const Vec3 a = vec3_make(fixed_from_float(1.234f), fixed_from_float(-5.678f), fixed_from_int(9));
    const Vec3 b = vec3_make(fixed_from_float(2.111f), fixed_from_float(0.876f), fixed_from_int(3));
    const Vec3 once = vec3_cross(vec3_scale(a, fixed_from_float(0.25f)), b);
    for (int i = 0; i < 16; ++i)
    {
        const Vec3 again = vec3_cross(vec3_scale(a, fixed_from_float(0.25f)), b);
        EXPECT_EQ(once.x, again.x);
        EXPECT_EQ(once.y, again.y);
        EXPECT_EQ(once.z, again.z);
        EXPECT_EQ(vec3_dot(a, b), vec3_dot(a, b));
    }
    // Независимое ожидание (ручная редукция, 3·(ONE/2) = 1.5):
    const Vec3 h1 = vec3_make(3 * (FIXED_ONE / 2), 0, 0);
    const Vec3 h2 = vec3_make(FIXED_ONE, 0, 0);
    EXPECT_EQ(vec3_dot(h1, h2), 3 * (FIXED_ONE / 2));
}
