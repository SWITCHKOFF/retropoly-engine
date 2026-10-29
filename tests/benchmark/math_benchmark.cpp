// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/benchmark/math_benchmark.cpp
//
// Бенчмарк-заготовка математического ядра (docs/plans/sprint_01_02.md,
// Неделя 4 [M]; пресет pc-bench). Собирается ТОЛЬКО когда google/benchmark
// вендорен (src/third_party/benchmark); vendoring — отдельная задача с pin'ом
// и записью в VERSION.txt (§3.6-правило для зависимостей).
// Базовая стоимость операций Q16.16: BM_FixedMul, BM_Vec3Normalize, BM_Mat4Multiply.
#include "core/math/mat4.h"
#include "core/math/vec3.h"

#include <benchmark/benchmark.h>

static void BM_FixedMul(benchmark::State& state)
{
    // Нормативный образец (engineering doc v1.43 §13): inputs фиксированы,
    // выход подавляется DoNotOptimize (детерминированная измеримая замкнутость).
    const fixed_t a = fixed_from_int(123);
    const fixed_t b = fixed_from_int(456);
    for (auto _ : state)
    {
        const fixed_t c = fixed_mul(a, b);
        benchmark::DoNotOptimize(c);
    }
}
BENCHMARK(BM_FixedMul);

static void BM_Vec3Normalize(benchmark::State& state)
{
    const Vec3 v = vec3_make(fixed_from_int(3), fixed_from_int(4), fixed_from_int(5));
    for (auto _ : state)
    {
        const Vec3 n = vec3_normalize(v);
        benchmark::DoNotOptimize(n);
    }
}
BENCHMARK(BM_Vec3Normalize);

static void BM_Mat4Multiply(benchmark::State& state)
{
    Mat4 rotate;
    Mat4 translate;
    Mat4 out;
    mat4_rotate_z(&rotate, FIXED_HALF_PI / 3);
    mat4_translate(&translate, fixed_from_int(1), fixed_from_int(2), fixed_from_int(3));
    for (auto _ : state)
    {
        mat4_multiply(&out, &translate, &rotate);
        benchmark::DoNotOptimize(out);
    }
}
BENCHMARK(BM_Mat4Multiply);

BENCHMARK_MAIN();
