// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/math/math.cpp
//
// Non-inline реализации математического ядра (vec/trig/mat).
// Нормативно: docs/RetroEngine_Engineering_Documentation_v1.43.md §4.2–4.4,
// docs/plans/sprint_01_02.md (Неделя 3 — length/normalize; Неделя 4 —
// тригонометрия sin_fixed/cos_fixed и матрицы).
#include "core/math/vec3.h"

fixed_t vec3_length(Vec3 v)
{
    // Детерминированный integer sqrt от редукции с одним clamp.
    return fixed_sqrt(vec3_length_sq(v));
}

Vec3 vec3_normalize(Vec3 v)
{
    const fixed_t len = vec3_length(v);
    // Деление нулевой длины — контракт fixed_div (debug assert / release
    // насыщение); без собственной ветки — единое поведение с ядром.
    return Vec3{fixed_div(v.x, len), fixed_div(v.y, len), fixed_div(v.z, len)};
}
