// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/benchmark/memory_benchmark.cpp
//
// Baseline allocation latency (docs/plans/sprint_03_04.md, Недели 5–7 [H]).
// Это НЕ acceptance-порог: фиксируется базовая стоимость Linear/Pool/Stack на
// reference-машине. Host-only (пресет pc-bench), без Google Benchmark —
// цель — измерить сами аллокаторы без вендоринга дополнительной зависимости.
#include "core/memory/linear_allocator.h"
#include "core/memory/pool_allocator.h"
#include "core/memory/stack_allocator.h"

#include <stdint.h>

#include <chrono>
#include <cstdio>

namespace
{

alignas(64) uint8_t g_linear_storage[1024 * 1024];
alignas(64) uint8_t g_stack_storage[1024 * 1024];

struct BenchObject
{
    uint32_t id;
    uint32_t payload;
};

/** Барьер против dead-code elimination результата аллокации. */
inline void consume(const void* pointer)
{
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" : : "r"(pointer) : "memory");
#else
    static volatile const void* sink = nullptr;
    sink = pointer;
#endif
}

template<typename Func>
double measure_ns_per_op(uint32_t iterations, Func&& func)
{
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (uint32_t i = 0; i < iterations; ++i)
        func();
    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    const double total_ns =
        std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(end - start).count();
    return total_ns / static_cast<double>(iterations);
}

} // namespace

int main()
{
    const uint32_t iterations = 5000000;

    LinearAllocator linear(g_linear_storage, sizeof(g_linear_storage));
    StackAllocator stack(g_stack_storage, sizeof(g_stack_storage));
    Pool<BenchObject, 256> pool;

    const double linear_ns = measure_ns_per_op(iterations, [&linear]() {
        if (linear.remaining() < 16)
            linear.reset();
        consume(linear.allocate(16, 8));
    });

    const double stack_ns = measure_ns_per_op(iterations, [&stack]() {
        if (stack.remaining() < 16)
            stack.reset_frame();
        consume(stack.push(16, 8));
    });

    const double pool_ns = measure_ns_per_op(iterations, [&pool]() {
        BenchObject* object = pool.allocate();
        consume(object);
        if (object != nullptr && !pool.deallocate(object))
            std::fprintf(stderr, "pool deallocate failed\n");
    });

    std::printf("allocator baseline (ns/op, %u iterations)\n", iterations);
    std::printf("  LinearAllocator::allocate(16, 8)          %8.3f\n", linear_ns);
    std::printf("  StackAllocator::push(16, 8)               %8.3f\n", stack_ns);
    std::printf("  Pool::allocate + Pool::deallocate         %8.3f\n", pool_ns);
    return 0;
}
