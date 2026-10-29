// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/math/fixed.cpp
//
// Non-inline реализации математического ядра (fixed-point).
// Нормативный контракт √ (§12.1 п. 1 engineering doc v1.43):
//   - отрицательный вход недопустим: debug — assert; release (NDEBUG) — 0;
//   - только целочисленная/фиксированная арифметика (не float):
//     полностью целочисленный «школьный» побитовый алгоритм (integer
//     longhand isqrt) — фиксированное число шагов, независимое от входа
//     (ограниченная итерация), побитно детерминирован на всех платформах.
#include "core/math/fixed.h"

fixed_t fixed_sqrt(fixed_t v)
{
    if (v < 0)
    {
        assert(!"fixed_sqrt: negative input");
        return 0;
    }
    if (v == 0)
        return 0;

    // √(V/2^16) = √(V·2^16)/2^16: подкоренное есть V, сдвинутое на число
    // дробных бит представления 16.16. V ≤ 2^31−1 → V·2^16 < 2^47.
    // Результат — floor(√(V·2^16)), максимум ~2^23.5 → без переполнения.
    uint64_t num = (uint64_t)v << FIXED_BITS;
    uint64_t result = 0;
    // Старший квадрат «четверки» для 48-битного числа: 4^23 = 2^46.
    uint64_t bit = (uint64_t)UINT64_C(1) << 46;

    // Классический побитовый алгоритм: ровно 24 шага при любом входе
    // (bounded iteration) — no division, no float, no data-dependent loops.
    while (bit != 0)
    {
        if (num >= result + bit)
        {
            num -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (fixed_t)result;
}
