// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/math/fixed_test.cpp
//
// Юнит-тесты fixed_t (16.16) — docs/plans/sprint_01_02.md, Неделя 2.
// Эталонные значения вычисляются независимо (ручная математика в double),
// а не функциями движка. Детерминизм проверяется на raw-значениях fixed_t.
//
// Нормативный контракт: src/core/math/fixed.h;
// engineering doc v1.43 §4.1, §12.1 (пп. 1–2).
#include "core/math/fixed.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <climits>
#include <random>

namespace
{

/** Абсолютная погрешность в представлениях 16.16 (2.0 raw ≈ 3.05e-5). */
inline double abs_error_raw(fixed_t actual, double expected_raw)
{
    return std::fabs(static_cast<double>(actual) - expected_raw);
}

} // namespace

// ----------------------------------------------------------------------------
// Конверсии
// ----------------------------------------------------------------------------

TEST(Fixed, FromIntExact)
{
    EXPECT_EQ(fixed_from_int(0), 0);
    EXPECT_EQ(fixed_from_int(1), FIXED_ONE);
    EXPECT_EQ(fixed_from_int(-1), -FIXED_ONE);
    EXPECT_EQ(fixed_from_int(100), INT64_C(100) * FIXED_ONE);
}

TEST(Fixed, FromIntSaturates)
{
    // 40000 > 32767.999: диапазон int32_t «не вмещает» целую часть — насыщение.
    EXPECT_EQ(fixed_from_int(40000), INT32_MAX);
    EXPECT_EQ(fixed_from_int(-40000), INT32_MIN);
    EXPECT_EQ(fixed_from_int(INT32_MAX), INT32_MAX);
    EXPECT_EQ(fixed_from_int(INT32_MIN), INT32_MIN);
}

TEST(Fixed, RoundTripToInt)
{
    const int32_t values[] = {0, 1, -1, 5, -5, 100, -100, 32767, -32768};
    for (int32_t v : values)
    {
        EXPECT_EQ(fixed_to_int(fixed_from_int(v)), v) << "v = " << v;
    }
}

TEST(Fixed, FromFloatBasics)
{
    // Двоично-точные значения — проверяем raw побитно.
    EXPECT_EQ(fixed_from_float(2.5f), fixed_from_int(2) + FIXED_ONE / 2);
    EXPECT_EQ(fixed_from_float(-1.5f), -3 * (FIXED_ONE / 2));
    EXPECT_EQ(fixed_from_float(0.0f), 0);
    EXPECT_EQ(fixed_from_float(-0.0f), 0);
    // Плавающая конверсия: погрешность < 0.001.
    EXPECT_NEAR(fixed_to_float(fixed_from_float(0.3f)), 0.3, 0.001);
    EXPECT_NEAR(fixed_to_float(fixed_from_float(123.456f)), 123.456, 0.001);
    // Усечение к нулю (truncation toward zero).
    EXPECT_LT(fixed_to_float(fixed_from_float(-2.7f)), -2.6998);
    EXPECT_GT(fixed_to_float(fixed_from_float(-2.7f)), -2.7 - 0.001);
}

TEST(Fixed, FromFloatNonFiniteIsZero)
{
    EXPECT_EQ(fixed_from_float(std::numeric_limits<float>::quiet_NaN()), 0);
    EXPECT_EQ(fixed_from_float(std::numeric_limits<float>::infinity()), 0);
    EXPECT_EQ(fixed_from_float(-std::numeric_limits<float>::infinity()), 0);
}

TEST(Fixed, FromFloatSaturates)
{
    EXPECT_EQ(fixed_from_float(1e9f), INT32_MAX);
    EXPECT_EQ(fixed_from_float(-1e9f), INT32_MIN);
    // 32768.0 нельзя вместить в signed 16.16 (там 32767.9999) — насыщение.
    EXPECT_EQ(fixed_from_float(40000.0f), INT32_MAX);
}

TEST(Fixed, ToFloatBasics)
{
    EXPECT_NEAR(fixed_to_float(FIXED_ONE), 1.0, 0.001);
    EXPECT_NEAR(fixed_to_float(-FIXED_ONE), -1.0, 0.001);
    EXPECT_NEAR(fixed_to_float(0), 0.0, 0.001);
    // 2.5 → 0.5 LSB точности: raw 163840 двоично-точно.
    EXPECT_NEAR(fixed_to_float(fixed_from_int(2) + FIXED_ONE / 2), 2.5, 0.001);
}

// ----------------------------------------------------------------------------
// Add / Sub: точные значения и насыщение
// ----------------------------------------------------------------------------

TEST(Fixed, AddSubPreciseValues)
{
    EXPECT_EQ(fixed_add(fixed_from_int(10), fixed_from_int(3)), fixed_from_int(13));
    EXPECT_EQ(fixed_sub(fixed_from_int(30), fixed_from_int(10)), fixed_from_int(20));
    // Дробные: 0.25 + 0.5 = 0.75 (двоично-точно).
    EXPECT_EQ(fixed_add(FIXED_ONE / 4, FIXED_ONE / 2), 3 * (FIXED_ONE / 4));
    EXPECT_EQ(fixed_add(fixed_from_int(2), fixed_from_int(-5)), fixed_from_int(-3));
}

TEST(Fixed, SaturatingArithmetic)
{
    // Нормативный вектор: add(INT32_MAX, 1) == INT32_MAX; sub(INT32_MIN, 1) == INT32_MIN.
    EXPECT_EQ(fixed_add(INT32_MAX, 1), INT32_MAX);
    EXPECT_EQ(fixed_add(INT32_MAX, INT32_MAX), INT32_MAX);
    EXPECT_EQ(fixed_add(INT32_MIN, -1), INT32_MIN);
    EXPECT_EQ(fixed_sub(INT32_MIN, 1), INT32_MIN);
    EXPECT_EQ(fixed_sub(INT32_MIN, INT32_MAX), INT32_MIN);
    EXPECT_EQ(fixed_sub(INT32_MAX, INT32_MIN), INT32_MAX);
    // Насыщающий результат идентичен clamped-эталону в int64.
    EXPECT_EQ(fixed_add(fixed_from_int(20000), fixed_from_int(20000)), INT32_MAX);
    EXPECT_EQ(fixed_sub(fixed_from_int(-20000), fixed_from_int(20000)), INT32_MIN);
}

// ----------------------------------------------------------------------------
// Mul: масштаб Q16.16
// ----------------------------------------------------------------------------

TEST(Fixed, MultiplyUsesQ16_16Scale)
{
    // Нормативный вектор: fixed_mul(from_int(5), from_int(7)) == from_int(35).
    EXPECT_EQ(fixed_mul(fixed_from_int(5), fixed_from_int(7)), fixed_from_int(35));
    // Дробные: 2.5 × 4.0 = 10.0; 0.25 × 4.0 = 1.0 (двоично-точные).
    EXPECT_EQ(fixed_mul(fixed_from_float(2.5f), fixed_from_float(4.0f)), fixed_from_int(10));
    EXPECT_EQ(fixed_mul(FIXED_ONE / 4, fixed_from_int(4)), FIXED_ONE);
    // Знаки.
    EXPECT_EQ(fixed_mul(fixed_from_int(5), fixed_from_int(-7)), fixed_from_int(-35));
    EXPECT_EQ(fixed_mul(fixed_from_int(-5), fixed_from_int(-7)), fixed_from_int(35));
    // Усечение к нулю в масштабе Q16.16.
    EXPECT_NEAR(fixed_to_float(fixed_mul(fixed_from_float(1.3f), fixed_from_float(2.7f))),
        1.3 * 2.7, 0.001);
}

TEST(Fixed, MultiplySaturates)
{
    EXPECT_EQ(fixed_mul(INT32_MAX, INT32_MAX), INT32_MAX);
    EXPECT_EQ(fixed_mul(INT32_MIN, INT32_MIN), INT32_MAX); // положительное насыщение
    EXPECT_EQ(fixed_mul(INT32_MIN, INT32_MAX), INT32_MIN);
    EXPECT_EQ(fixed_mul(INT32_MIN, FIXED_ONE), INT32_MIN); // 1.0×: −32768 без потери
}

// ----------------------------------------------------------------------------
// Div
// ----------------------------------------------------------------------------

TEST(Fixed, DivPreciseValues)
{
    EXPECT_EQ(fixed_div(fixed_from_int(100), fixed_from_int(10)), fixed_from_int(10));
    // 10.0 / 3.0 → 3.3333… (усечение; погрешность < 0.001 против double-эталона).
    const fixed_t q = fixed_div(fixed_from_int(10), fixed_from_int(3));
    EXPECT_NEAR(fixed_to_float(q), 10.0 / 3.0, 0.001);
    // Знаковое деление.
    EXPECT_EQ(fixed_div(fixed_from_int(-100), fixed_from_int(10)), fixed_from_int(-10));
    EXPECT_EQ(fixed_div(fixed_from_int(-100), fixed_from_int(-10)), fixed_from_int(10));
}

#if defined(NDEBUG)
TEST(Fixed, DivisionByZeroReleaseContract)
{
    // Release: определённое насыщение, без UB и без падения.
    EXPECT_EQ(fixed_div(FIXED_ONE, 0), INT32_MAX);
    EXPECT_EQ(fixed_div(0, 0), INT32_MAX);       // a ≥ 0
    EXPECT_EQ(fixed_div(-FIXED_ONE, 0), INT32_MIN); // a < 0
    EXPECT_EQ(fixed_div(fixed_from_int(10), fixed_from_int(0)), INT32_MAX);
}
#else
TEST(Fixed, DivisionByZeroDebugAsserts)
{
    // Debug: контракт — assert (abort). Дополнительно оба знака знаменателя.
    EXPECT_DEATH(fixed_div(FIXED_ONE, 0), "fixed_div");
    EXPECT_DEATH(fixed_div(INT32_MIN, 0), "fixed_div");
}
#endif

// ----------------------------------------------------------------------------
// Точность: стресс-тест против double-эталона
// ----------------------------------------------------------------------------

TEST(Fixed, AccuracyStressTestMul)
{
    std::mt19937 rng(42);
    const fixed_t k_range = 6 * FIXED_ONE;
    std::uniform_int_distribution<int32_t> dist(-k_range, k_range);

    double max_error = 0.0;
    for (int i = 0; i < 10000; ++i)
    {
        const fixed_t a = dist(rng);
        const fixed_t b = dist(rng);
        const double expected =
            (static_cast<double>(a) * static_cast<double>(b)) / 65536.0;
        const fixed_t actual = fixed_mul(a, b);
        max_error = std::max(max_error, abs_error_raw(actual, expected));
    }
    EXPECT_LT(max_error, 2.0); // < 2.0 / FIXED_ONE в raw-масштабе
}

TEST(Fixed, AccuracyStressTestDiv)
{
    std::mt19937 rng(42);
    const fixed_t k_range = 6 * FIXED_ONE;
    std::uniform_int_distribution<int32_t> dist(-k_range, k_range);

    double max_error = 0.0;
    for (int i = 0; i < 10000; ++i)
    {
        const fixed_t a = dist(rng);
        const fixed_t b = dist(rng);
        if (b == 0)
        {
            continue; // детерминированный пропуск нулевого делителя (контракт div)
        }
        const double expected = (static_cast<double>(a) * 65536.0) / static_cast<double>(b);
        const fixed_t actual = fixed_div(a, b);
        max_error = std::max(max_error, abs_error_raw(actual, expected));
    }
    EXPECT_LT(max_error, 2.0); // < 2.0 / FIXED_ONE в raw-масштабе
}

// ----------------------------------------------------------------------------
// Детерминизм
// ----------------------------------------------------------------------------

TEST(Fixed, DeterminismSameInputSameRawOutput)
{
    const fixed_t a = fixed_from_float(1.234567f);
    const fixed_t b = fixed_mul(a, INT32_MAX);
    // Один и тот же вход → побитно идентичный raw-выход (независимо от вызова).
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(fixed_mul(a, INT32_MAX), b);
        EXPECT_EQ(fixed_add(fixed_div(a, fixed_from_int(3)), fixed_from_int(7)),
            fixed_add(fixed_div(a, fixed_from_int(3)), fixed_from_int(7)));
    }
    // Независимое ожидание (ручная математика, не движок):
    // 123 × 456 = 56088 → 56088 × FIXED_ONE выходит за int32 → насыщение INT32_MAX.
    EXPECT_EQ(fixed_mul(fixed_from_int(123), fixed_from_int(456)), INT32_MAX);
    // Обычный путь: ручная редукция (247 × 2^16) / 65536 = 247.
    EXPECT_EQ(fixed_mul(fixed_from_int(247), FIXED_ONE), fixed_from_int(247));
}

// ----------------------------------------------------------------------------
// Sqrt (fixed.cpp)
// ----------------------------------------------------------------------------

TEST(Fixed, SqrtExactSquares)
{
    EXPECT_EQ(fixed_sqrt(fixed_from_int(25)), fixed_from_int(5));
    EXPECT_EQ(fixed_sqrt(fixed_from_int(49)), fixed_from_int(7));
    EXPECT_EQ(fixed_sqrt(0), 0);
    EXPECT_EQ(fixed_sqrt(FIXED_ONE), FIXED_ONE); // √1 = 1
    EXPECT_EQ(fixed_sqrt(fixed_from_int(16)), fixed_from_int(4));
}

TEST(Fixed, SqrtNonSquareWithinTolerance)
{
    EXPECT_NEAR(fixed_to_float(fixed_sqrt(fixed_from_int(2))), 1.4142135, 0.01);
    EXPECT_NEAR(fixed_to_float(fixed_sqrt(fixed_from_int(3))), 1.7320508, 0.01);
    EXPECT_NEAR(fixed_to_float(fixed_sqrt(fixed_from_float(2.25f))), 1.5, 0.01);
}

#if defined(NDEBUG)
TEST(Fixed, SqrtNegativeReleaseContract)
{
    // Release: отрицательный вход → 0 (§12.1 п. 1 engineering doc).
    EXPECT_EQ(fixed_sqrt(-FIXED_ONE), 0);
    EXPECT_EQ(fixed_sqrt(-1), 0);
}
#else
TEST(Fixed, SqrtNegativeDebugAsserts)
{
    EXPECT_DEATH(fixed_sqrt(-FIXED_ONE), "fixed_sqrt");
}
#endif
