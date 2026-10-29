// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/math/vec2_test.cpp
//
// Минимальный набор Vec2 (16.16) — docs/plans/sprint_01_02.md, Неделя 3 [H].
#include "core/math/vec2.h"

#include <gtest/gtest.h>

/** Тестовый компаратор (инициализация попарным равенством raw-компонент). */
static bool operator==(Vec2 a, Vec2 b)
{
    return a.x == b.x && a.y == b.y;
}

TEST(Vec2, MakeAddSubValues)
{
    const Vec2 a = vec2_make(fixed_from_int(1), fixed_from_int(2));
    const Vec2 b = vec2_make(fixed_from_int(3), fixed_from_int(5));

    const Vec2 sum = vec2_add(a, b);
    EXPECT_EQ(sum.x, fixed_from_int(4));
    EXPECT_EQ(sum.y, fixed_from_int(7));

    const Vec2 diff = vec2_sub(b, a);
    EXPECT_EQ(diff.x, fixed_from_int(2));
    EXPECT_EQ(diff.y, fixed_from_int(3));
}

TEST(Vec2, ScalePreciseValues)
{
    const Vec2 v = vec2_make(fixed_from_float(1.5f), fixed_from_int(-4));
    const Vec2 s = vec2_scale(v, fixed_from_int(2));
    EXPECT_EQ(s.x, fixed_from_int(3));
    EXPECT_EQ(s.y, fixed_from_int(-8));
}

TEST(Vec2, DotSingleFinalClamp)
{
    // Независимое ожидание: (1,2)·(3,4) = 3 + 8 = 11.
    const Vec2 a = vec2_make(fixed_from_int(1), fixed_from_int(2));
    const Vec2 b = vec2_make(fixed_from_int(3), fixed_from_int(4));
    EXPECT_EQ(vec2_dot(a, b), fixed_from_int(11));

    // Полный int64-аккумулятор: 3·INT32_MAX·2^16 / 2^16 → INT32_MAX.
    const Vec2 big = vec2_make(INT32_MAX, INT32_MAX);
    const Vec2 one = vec2_make(FIXED_ONE, FIXED_ONE);
    EXPECT_EQ(vec2_dot(big, one), INT32_MAX);
    EXPECT_EQ(vec2_dot(vec2_make(INT32_MIN, INT32_MIN), one), INT32_MIN);
}

TEST(Vec2, Determinism)
{
    const Vec2 a = vec2_make(fixed_from_float(0.375f), fixed_from_float(-2.125f));
    const Vec2 b = vec2_make(fixed_from_int(5), fixed_from_int(2));
    EXPECT_EQ(vec2_dot(a, b), vec2_dot(a, b));
    EXPECT_EQ(vec2_add(vec2_scale(a, FIXED_ONE), b), vec2_add(vec2_scale(a, FIXED_ONE), b));
}
