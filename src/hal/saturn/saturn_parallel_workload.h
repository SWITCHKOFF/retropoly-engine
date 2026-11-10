// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_parallel_workload.h
//
// Параллельный workload S25–26: подготовка вершин/command list для будущего
// VDP1-пайплайна (S27–28). Master и Slave считают одинаковую fixed-point
// математику над детерминированными диапазонами треугольников.
//
// Контракты:
//   - вход — реальный MeshAsset с CD (CUBE.MSH) либо встроенный куб;
//   - split фиксирован: [0..N/2) Master, [N/2..N) Slave;
//   - single-CPU baseline и dual-CPU дают побайтно одинаковые записи/хэш;
//   - Slave пишет только в shared Work RAM, не трогает VDP1/VDP2;
//   - при отказе Slave Master пересчитывает вторую половину в отдельный
//     fallback-буфер (late write зависшего Slave не портит кадр).
#pragma once

#include "assets/assets.h" /* MeshAsset */
#include "core/math/mat4.h"

#include <stdint.h>

namespace saturn {

/** Всего треугольников в workload (делится на 2 без остатка). */
constexpr uint32_t PARALLEL_TRIANGLES_TOTAL = 256u;
/** Половина на CPU. */
constexpr uint32_t PARALLEL_TRIANGLES_PER_CPU = PARALLEL_TRIANGLES_TOTAL / 2u;
// Буферы workload живут в saturn::PARALLEL_WORK_BYTES (Work RAM-L):
// 3 × 512 × 16 B output + batch descriptor.

/**
 * Готовый треугольник command-list prep (фиксированный layout 8 байт).
 *
 * S25–26 готовит clip-space данные: hash трёх преобразованных вершин,
 * depth первого vertex и флаги near/far clip. Perspective divide и
 * screen-mapping остаются в VDP1-стадии S27–28 (на Master).
 */
struct PreparedTriangle
{
    uint32_t hash;  /**< FNV-1a трёх clip-space Vec4. */
    uint16_t depth; /**< Depth-ключ (по clip.z первого vertex). */
    uint16_t flags; /**< PT_* биты. */
};

enum PreparedTriangleFlags
{
    PT_VISIBLE = 1u << 0,
    PT_CLIPPED = 1u << 1,
    PT_BACKFACE = 1u << 2
};

static_assert(sizeof(PreparedTriangle) == 8u,
              "PreparedTriangle layout must stay 8 bytes (shared memory contract)");

/** Итог одного кадра параллельного workload. */
struct ParallelFrameResult
{
    bool ok;                /**< Кадр посчитан (single или dual). */
    bool used_slave;        /**< Вторая половина посчитана Slave. */
    bool fallback;          /**< Master пересчитал вторую половину. */
    uint32_t hash;          /**< FNV-1a всего output кадра. */
    uint32_t master_triangles;
    uint32_t slave_triangles;
    uint32_t wait_us;       /**< Время ожидания Slave. */
};

/** Итог boot-бенчмарка single vs dual (методика — ADR-009). */
struct ParallelBenchResult
{
    bool ok;                /**< Все прогоны завершились. */
    bool hashes_match;      /**< single и dual дали одинаковый hash. */
    bool slave_used;        /**< Slave реально исполнял job'ы. */
    uint32_t iterations;
    uint32_t single_us;     /**< Среднее время single-CPU, µs. */
    uint32_t dual_us;       /**< Среднее время dual-CPU, µs. */
    uint32_t speedup_pct;   /**< (single − dual) * 100 / single. */
    uint32_t single_hash;
    uint32_t dual_hash;
};

/**
 * Инициализация workload.
 *
 * @param mesh Загруженный CUBE.MSH (может быть nullptr — встроенный куб).
 * @return true при успехе.
 */
bool parallel_workload_init(const MeshAsset* mesh);

/** Сброс статистики/указателя на mesh (worker останавливается отдельно). */
void parallel_workload_shutdown(void);

/**
 * Один кадр: Slave считает вторую половину, Master — первую.
 *
 * @param mvp Матрица proj·view (fixed 16.16, column-major).
 * @param timeout_ms Таймаут ожидания Slave (fallback при превышении).
 * @return Результат кадра (hash детерминирован).
 */
ParallelFrameResult parallel_workload_run_frame(const Mat4* mvp, uint32_t timeout_ms);

/**
 * Boot-бенчмарк: @p iterations прогонов single-CPU и dual-CPU.
 *
 * @param mvp Матрица proj·view.
 * @param iterations Число прогонов каждого варианта (≥ 1).
 * @return Средние времена и сравнение hash.
 */
ParallelBenchResult parallel_workload_benchmark(const Mat4* mvp, uint32_t iterations);

/** @return Hash последнего кадра. */
uint32_t parallel_workload_last_hash(void);

/** @return Полный размер shared static-буфера workload в байтах. */
uint32_t parallel_workload_shared_bytes(void);

/** @return Использованная часть shared-буфера (для отчёта). */
uint32_t parallel_workload_shared_used_bytes(void);

}  // namespace saturn
