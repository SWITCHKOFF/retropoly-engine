// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/math/fixed.h
//
// Движковый скаляр: знаковый 16.16 fixed-point (`fixed_t`, int32_t).
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md, §4.1 «FIXED-POINT»,
//     §12.1 «CANONICAL ENGINE CONTRACT» (пп. 1–2);
//   - docs/plans/sprint_01_02.md, Неделя 2.
//
// Контракт (нормативно):
//   - add/sub/mul/div — НАСЫЩАЮЩИЕ (saturating) к диапазону int32_t;
//   - результат НЕ зависит от поведения знакового переполнения (UB запрещён):
//     все промежуточные вычисления — в int64_t;
//   - умножение — масштаб Q16.16 с усечением к нулю (truncation toward zero);
//   - деление на ноль: debug — assert, release — INT32_MAX (a≥0) / INT32_MIN (a<0);
//   - одна и та же последовательность операций даёт побитно одинаковый
//     результат на всех поддерживаемых платформах (16.16 fixed-point).
#pragma once

#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include <cmath> // std::isfinite (движок собирается компилятором C++)

/** Знаковый 16.16 fixed-point скаляр движка (engine scalar). */
typedef int32_t fixed_t;

/** Число дробных бит представления 16.16. */
#define FIXED_BITS 16

/** Единица в представлении 16.16 (1.0 → 65536). */
#define FIXED_ONE (INT32_C(1) << FIXED_BITS)

/**
 * Насыщающее сложение 64-битных промежуточных значений (без UB).
 *
 * Применяется ВНУТРИ редукций (dot/mat-умножения), когда накопление полных
 * Q16.16-произведений может выйти за диапазон int64_t. Это аккумуляторная
 * защита от неопределённого поведения, а не Q16.16-clamp промежуточного
 * результата: единственный fixed-клап насыщения остаётся в конце редукции
 * (контракт ReductionUsesSingleFinalClamp). Детерминированно, без опоры
 * на поведение знакового переполнения.
 *
 * @param a Накопитель (int64_t).
 * @param b Добавляемое слагаемое (int64_t), |b| ≤ MAX·MAX (int32²).
 * @return a + b с насыщением к диапазону int64_t.
 */
static inline int64_t fixed_sat_add_i64(int64_t a, int64_t b)
{
    if (b > 0 && a > INT64_MAX - b)
        return INT64_MAX;
    if (b < 0 && a < INT64_MIN - b)
        return INT64_MIN;
    return a + b;
}

/**
 * Насыщающее приведение 64-битного промежуточного значения к fixed_t.
 *
 * Единственная точка обрезки диапазона внутри математического ядра; векторные
 * и матричные операции обязаны сводить редукцию к одному вызову в конце.
 *
 * @param v Значение в масштабе Q16.16 (int64_t).
 * @return Насыщенное значение: INT32_MAX при верхнем переполнении,
 *         INT32_MIN при нижнем, иначе — v.
 */
static inline fixed_t fixed_clamp_i64(int64_t v)
{
    if (v > INT32_MAX)
        return INT32_MAX;
    if (v < INT32_MIN)
        return INT32_MIN;
    return (fixed_t)v;
}

/**
 * Формирование fixed_t из целого: v масштабируется на FIXED_ONE.
 * Промежуточное значение — int64_t с насыщением (saturating).
 *
 * @param v Целое значение (int32_t).
 * @return Представление v в 16.16; при переполнении — INT32_MAX/INT32_MIN.
 */
static inline fixed_t fixed_from_int(int32_t v)
{
    return fixed_clamp_i64((int64_t)v * FIXED_ONE);
}

/**
 * Формирование fixed_t из числа с плавающей точкой (host-конверсия).
 *
 * Недетерминированные/не-числовые входы: NaN и ±Inf → 0.
 * Преобразование — усечением к нулю (truncation toward zero);
 * выход за диапазон насыщается к INT32_MIN/INT32_MAX.
 *
 * @param v Значение float.
 * @return Представление v в 16.16 (усечение к нулю), 0 для NaN/±Inf.
 */
static inline fixed_t fixed_from_float(float v)
{
    if (!std::isfinite(v))
        return 0;
    const double scaled = (double)v * (double)FIXED_ONE;
    if (scaled >= (double)INT32_MAX)
        return INT32_MAX;
    if (scaled <= (double)INT32_MIN)
        return INT32_MIN;
    return (fixed_t)scaled;
}

/**
 * Целочисленная часть fixed_t (усечение к нулю; электронное деление на 2^16).
 *
 * @param v Значение в представлении 16.16.
 * @return Целая часть v (truncation toward zero).
 */
static inline int32_t fixed_to_int(fixed_t v) { return v / FIXED_ONE; }

/**
 * Конверсия fixed_t в float (host-конверсия; для тестов и отладки).
 *
 * @param v Значение в представлении 16.16.
 * @return Аппроксимация v во float (погрешность ≤ 1/2 LSB).
 */
static inline float fixed_to_float(fixed_t v) { return (float)v / (float)FIXED_ONE; }

/**
 * Насыщающее сложение. Промежуточное значение — int64_t.
 *
 * @param a Слагаемое (16.16).
 * @param b Слагаемое (16.16).
 * @return a + b с насыщением к диапазону int32_t.
 */
static inline fixed_t fixed_add(fixed_t a, fixed_t b)
{
    return fixed_clamp_i64((int64_t)a + (int64_t)b);
}

/**
 * Насыщающее вычитание. Промежуточное значение — int64_t.
 *
 * @param a Уменьшаемое (16.16).
 * @param b Вычитаемое (16.16).
 * @return a − b с насыщением к диапазону int32_t.
 */
static inline fixed_t fixed_sub(fixed_t a, fixed_t b)
{
    return fixed_clamp_i64((int64_t)a - (int64_t)b);
}

/**
 * Насыщающее умножение в масштабе Q16.16 (truncation toward zero).
 * Промежуточное произведение — int64_t; масштаб 2^16 снимается перед
 * насыщением, результат побитно детерминирован.
 *
 * @param a Множитель (16.16).
 * @param b Множитель (16.16).
 * @return a · b в представлении 16.16 с насыщением к диапазону int32_t.
 */
static inline fixed_t fixed_mul(fixed_t a, fixed_t b)
{
    return fixed_clamp_i64(((int64_t)a * (int64_t)b) / FIXED_ONE);
}

/**
 * Насыщающее деление в масштабе Q16.16. Промежуточное значение — int64_t.
 * Деление на ноль: debug — assert; release — INT32_MAX при a ≥ 0, INT32_MIN
 * при a < 0 (определённый детерминированный контракт, §12.1 п. 2).
 *
 * @param a Делимое (16.16).
 * @param b Делитель (16.16), ноль недопустим (см. контракт).
 * @return a / b в представлении 16.16 с насыщением к диапазону int32_t.
 */
static inline fixed_t fixed_div(fixed_t a, fixed_t b)
{
    if (b == 0)
    {
        assert(!"fixed_div: division by zero");
        return a >= 0 ? INT32_MAX : INT32_MIN;
    }
    return fixed_clamp_i64(((int64_t)a * FIXED_ONE) / b);
}

/**
 * Целочисленный детерминированный квадратный корень (16.16).
 * Реализация — src/core/math/fixed.cpp (знаковый контракт и ограниченная
 * итерация описаны там).
 *
 * @param v Подкоренное значение (16.16); отрицательный вход недопустим.
 * @return √v в представлении 16.16; 0 при отрицательном входе в release.
 */
fixed_t fixed_sqrt(fixed_t v);
