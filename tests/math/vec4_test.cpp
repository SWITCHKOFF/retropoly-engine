// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/math/vec4_test.cpp
//
// Минимальный набор Vec4 (16.16) — docs/plans/sprint_01_02.md, Неделя 3 [H].
#include "core/math/vec4.h"

#include <gtest/gtest.h>

TEST(Vec4, MakeAddSubValues)
{
    const Vec4 a = vec4_make(fixed_from_int(1), fixed_from_int(2), fixed_from_int(3), fixed_from_int(4));
    const Vec4 b = vec4_make(fixed_from_int(5), fixed_from_int(6), fixed_from_int(7), fixed_from_int(8));

    const Vec4 sum = vec4_add(a, b);
    EXPECT_EQ(sum.x, fixed_from_int(6));
    EXPECT_EQ(sum.y, fixed_from_int(8));
    EXPECT_EQ(sum.z, fixed_from_int(10));
    EXPECT_EQ(sum.w, fixed_from_int(12));

    const Vec4 diff = vec4_sub(b, a);
    EXPECT_EQ(diff.x, fixed_from_int(4));
    EXPECT_EQ(diff.y, fixed_from_int(4));
    EXPECT_EQ(diff.z, fixed_from_int(4));
    EXPECT_EQ(diff.w, fixed_from_int(4));
}

TEST(Vec4, ScalePreciseValues)
{
    const Vec4 v = vec4_make(fixed_from_int(2), fixed_from_int(-4), 0, fixed_from_float(0.5f));
    const Vec4 s = vec4_scale(v, fixed_from_float(2.0f));
    EXPECT_EQ(s.x, fixed_from_int(4));
    EXPECT_EQ(s.y, fixed_from_int(-8));
    EXPECT_EQ(s.z, 0);
    EXPECT_EQ(s.w, FIXED_ONE);
}

TEST(Vec4, DotSingleFinalClamp)
{
    // Независимое ожидание: (1,2,3,4)·(5,6,7,8) = 5+12+21+32 = 70.
    const Vec4 a = vec4_make(fixed_from_int(1), fixed_from_int(2), fixed_from_int(3), fixed_from_int(4));
    const Vec4 b = vec4_make(fixed_from_int(5), fixed_from_int(6), fixed_from_int(7), fixed_from_int(8));
    EXPECT_EQ(vec4_dot(a, b), fixed_from_int(70));

    // Полный int64-аккумулятор (4 компоненты): 4·INT32_MAX·2^16 / 2^16 → INT32_MAX.
    const Vec4 big = vec4_make(INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX);
    const Vec4 one = vec4_make(FIXED_ONE, FIXED_ONE, FIXED_ONE, FIXED_ONE);
    EXPECT_EQ(vec4_dot(big, one), INT32_MAX);
    EXPECT_EQ(vec4_dot(vec4_make(INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN), one), INT32_MIN);
}

TEST(Vec4, Determinism)
{
    const Vec4 a = vec4_make(fixed_from_float(1.125f), fixed_from_int(-7), 0, fixed_from_float(0.25f));
    const Vec4 b = vec4_make(fixed_from_int(2), fixed_from_float(0.5f), fixed_from_int(9), FIXED_ONE);
    const Vec4 once = vec4_sub(vec4_scale(a, FIXED_ONE / 4), b);
    for (int i = 0; i < 16; ++i)
    {
        const Vec4 again = vec4_sub(vec4_scale(a, FIXED_ONE / 4), b);
        EXPECT_EQ(once.x, again.x);
        EXPECT_EQ(once.y, again.y);
        EXPECT_EQ(once.z, again.z);
        EXPECT_EQ(once.w, again.w);
        EXPECT_EQ(vec4_dot(a, b), vec4_dot(a, b));
    }
}
