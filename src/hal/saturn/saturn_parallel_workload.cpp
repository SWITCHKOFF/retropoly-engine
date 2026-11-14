// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_parallel_workload.cpp
//
// Детерминированный parallel workload (S25–26): transform + clip + screen
// mapping + backface + depth-key для будущего VDP1 command list (S27–28).
//
// Split фиксирован по индексам, математика — только fixed 16.16, поэтому
// single-CPU и dual-CPU результаты обязаны совпадать побайтно.
#include "hal/saturn/saturn_parallel_workload.h"

#include "hal/saturn/saturn_memory.h"
#include "hal/saturn/scu_sync.h"
#include "hal/saturn/slave_worker.h"

#include <stddef.h>

namespace saturn {

namespace {

constexpr uint32_t kFnvOffset = 2166136261u;
constexpr uint32_t kFnvPrime = 16777619u;
constexpr uint32_t kWaitTimeoutMs = 20u;

struct ParallelBatch
{
    Mat4 mvp;
    const Vertex* vertices;
    const Triangle* triangles;
    uint32_t vertex_count;
    uint32_t triangle_count;
    uint32_t start;
    uint32_t count;
    PreparedTriangle* out;
};

// --- Встроенный куб (fallback, если CUBE.MSH недоступен) -------------------

const Vertex kCubeVertices[8] = {
    {-FIXED_ONE, -FIXED_ONE, -FIXED_ONE, 0, 0, 0, 0, 0, 255u, 255u, 255u, 255u},
    {FIXED_ONE, -FIXED_ONE, -FIXED_ONE, 0, 0, 0, 0, 0, 255u, 255u, 255u, 255u},
    {FIXED_ONE, FIXED_ONE, -FIXED_ONE, 0, 0, 0, 0, 0, 255u, 255u, 255u, 255u},
    {-FIXED_ONE, FIXED_ONE, -FIXED_ONE, 0, 0, 0, 0, 0, 255u, 255u, 255u, 255u},
    {-FIXED_ONE, -FIXED_ONE, FIXED_ONE, 0, 0, 0, 0, 0, 255u, 255u, 255u, 255u},
    {FIXED_ONE, -FIXED_ONE, FIXED_ONE, 0, 0, 0, 0, 0, 255u, 255u, 255u, 255u},
    {FIXED_ONE, FIXED_ONE, FIXED_ONE, 0, 0, 0, 0, 0, 255u, 255u, 255u, 255u},
    {-FIXED_ONE, FIXED_ONE, FIXED_ONE, 0, 0, 0, 0, 0, 255u, 255u, 255u, 255u},
};

const Triangle kCubeTriangles[12] = {
    {0u, 1u, 2u}, {0u, 2u, 3u},  // −Z
    {4u, 6u, 5u}, {4u, 7u, 6u},  // +Z
    {0u, 4u, 5u}, {0u, 5u, 1u},  // −Y
    {3u, 2u, 6u}, {3u, 6u, 7u},  // +Y
    {0u, 3u, 7u}, {0u, 7u, 4u},  // −X
    {1u, 5u, 6u}, {1u, 6u, 2u},  // +X
};

// --- Shared state -----------------------------------------------------------

const Vertex* s_vertices = kCubeVertices;
const Triangle* s_triangles = kCubeTriangles;
uint32_t s_vertex_count = 8u;
uint32_t s_triangle_count = 12u;
uint32_t s_last_hash = 0u;

// Буферы живут в Work RAM-L (shared-регион saturn::parallel_work_base),
// а не в BSS: высокоскоростная Work RAM-H должна оставаться для кода/стека.
PreparedTriangle* s_master_out = nullptr;
PreparedTriangle* s_slave_out = nullptr;
PreparedTriangle* s_fallback_out = nullptr;
ParallelBatch* s_slave_batch = nullptr;

constexpr uint32_t kSharedStaticBytes =
    (sizeof(PreparedTriangle) * PARALLEL_TRIANGLES_PER_CPU * 3u) + sizeof(ParallelBatch);
static_assert(kSharedStaticBytes <= PARALLEL_WORK_BYTES,
              "parallel workload shared budget exceeded");

uint16_t clamp_u16(int32_t value)
{
    if (value < 0) {
        return 0u;
    }
    if (value > 65535) {
        return 65535u;
    }
    return static_cast<uint16_t>(value);
}

uint32_t fnv1a_continue(uint32_t hash, const void* data, uint32_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0u; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
    return hash;
}

void fill_batch(ParallelBatch* batch, const Mat4* mvp, uint32_t start, uint32_t count,
                PreparedTriangle* out)
{
    batch->mvp = *mvp;
    batch->vertices = s_vertices;
    batch->triangles = s_triangles;
    batch->vertex_count = s_vertex_count;
    batch->triangle_count = s_triangle_count;
    batch->start = start;
    batch->count = count;
    batch->out = out;
}

// 16.16-приведение int64 → int32 с усечением к нулю (без дорогого деления).
int32_t q16_trunc(int64_t value)
{
    if (value >= 0) {
        return fixed_clamp_i64(value >> FIXED_BITS);
    }
    return fixed_clamp_i64(-((-value) >> FIXED_BITS));
}

// Специализированный transform для Saturn workload: 32×32→64 (dmulu.l) и
// один финальный clamp на компоненту. Без fixed_sat_add_i64-веток в каждом
// слагаемом: диапазон mesh/MVP гарантированно мал (|v| ≤ 2^20).
Vec4 transform_clip(const Mat4& matrix, const Vec4& input)
{
    const int64_t x = (static_cast<int64_t>(matrix.m[0]) * input.x) +
                      (static_cast<int64_t>(matrix.m[4]) * input.y) +
                      (static_cast<int64_t>(matrix.m[8]) * input.z) +
                      (static_cast<int64_t>(matrix.m[12]) * input.w);
    const int64_t y = (static_cast<int64_t>(matrix.m[1]) * input.x) +
                      (static_cast<int64_t>(matrix.m[5]) * input.y) +
                      (static_cast<int64_t>(matrix.m[9]) * input.z) +
                      (static_cast<int64_t>(matrix.m[13]) * input.w);
    const int64_t z = (static_cast<int64_t>(matrix.m[2]) * input.x) +
                      (static_cast<int64_t>(matrix.m[6]) * input.y) +
                      (static_cast<int64_t>(matrix.m[10]) * input.z) +
                      (static_cast<int64_t>(matrix.m[14]) * input.w);
    const int64_t w = (static_cast<int64_t>(matrix.m[3]) * input.x) +
                      (static_cast<int64_t>(matrix.m[7]) * input.y) +
                      (static_cast<int64_t>(matrix.m[11]) * input.z) +
                      (static_cast<int64_t>(matrix.m[15]) * input.w);
    return Vec4{q16_trunc(x), q16_trunc(y), q16_trunc(z), q16_trunc(w)};
}

PreparedTriangle prepare_triangle(const ParallelBatch& batch, uint32_t global_index)
{
    PreparedTriangle result = {};

    if (batch.vertices == nullptr || batch.triangles == nullptr || batch.out == nullptr ||
        batch.vertex_count == 0u || batch.triangle_count == 0u) {
        result.flags = PT_CLIPPED;
        return result;
    }

    const uint32_t instance = global_index / batch.triangle_count;
    const uint32_t local_index = global_index % batch.triangle_count;
    const Triangle& triangle = batch.triangles[local_index];

    // Детерминированная сетка инстансов: 8×8×4 вокруг начала координат.
    const int32_t grid_x = static_cast<int32_t>(instance % 8u) - 4;
    const int32_t grid_y = static_cast<int32_t>((instance / 8u) % 8u) - 4;
    const int32_t grid_z = static_cast<int32_t>((instance / 64u) % 4u) - 2;
    const fixed_t offset_x = static_cast<fixed_t>(grid_x * 2 * FIXED_ONE);
    const fixed_t offset_y = static_cast<fixed_t>(grid_y * 2 * FIXED_ONE);
    const fixed_t offset_z = static_cast<fixed_t>(grid_z * 2 * FIXED_ONE);

    const uint16_t indices[3] = {triangle.v0, triangle.v1, triangle.v2};
    Vec4 clip[3] = {};
    bool clipped = false;

    for (uint32_t i = 0u; i < 3u; ++i) {
        if (static_cast<uint32_t>(indices[i]) >= batch.vertex_count) {
            clipped = true;
            continue;
        }
        const Vertex& vertex = batch.vertices[indices[i]];
        const Vec4 input = Vec4{
            fixed_add(vertex.x, offset_x),
            fixed_add(vertex.y, offset_y),
            fixed_add(vertex.z, offset_z),
            FIXED_ONE,
        };
        clip[i] = transform_clip(batch.mvp, input);
        // Near/far clip в clip-space без деления: -w <= z <= w при w > 0.
        if (clip[i].w <= 0 || clip[i].z < -clip[i].w || clip[i].z > clip[i].w) {
            clipped = true;
        }
    }

    result.depth = clamp_u16(static_cast<int32_t>(clip[0].z >> FIXED_BITS));
    result.hash = kFnvOffset;
    result.hash = fnv1a_continue(result.hash, &clip[0], static_cast<uint32_t>(sizeof(Vec4)));
    result.hash = fnv1a_continue(result.hash, &clip[1], static_cast<uint32_t>(sizeof(Vec4)));
    result.hash = fnv1a_continue(result.hash, &clip[2], static_cast<uint32_t>(sizeof(Vec4)));

    result.flags = 0u;
    if (clipped) {
        result.flags |= PT_CLIPPED;
    } else {
        result.flags |= PT_VISIBLE;
    }
    return result;
}

void execute_batch(void* argument)
{
    ParallelBatch* batch = static_cast<ParallelBatch*>(argument);
    if (batch == nullptr || batch->out == nullptr) {
        return;
    }
    for (uint32_t i = 0u; i < batch->count; ++i) {
        batch->out[i] = prepare_triangle(*batch, batch->start + i);
    }
}

uint32_t hash_outputs_n(const PreparedTriangle* second_half, uint32_t per_cpu)
{
    const uint32_t chunk_bytes = static_cast<uint32_t>(sizeof(PreparedTriangle)) * per_cpu;
    uint32_t hash = kFnvOffset;
    hash = fnv1a_continue(hash, s_master_out, chunk_bytes);
    hash = fnv1a_continue(hash, second_half, chunk_bytes);
    return hash;
}

}  // namespace

bool parallel_workload_init(const MeshAsset* mesh)
{
    uint8_t* shared_base = parallel_work_base();
    if (shared_base == nullptr) {
        return false;
    }
    s_master_out = reinterpret_cast<PreparedTriangle*>(shared_base);
    s_slave_out = s_master_out + PARALLEL_TRIANGLES_PER_CPU;
    s_fallback_out = s_slave_out + PARALLEL_TRIANGLES_PER_CPU;
    s_slave_batch = reinterpret_cast<ParallelBatch*>(s_fallback_out + PARALLEL_TRIANGLES_PER_CPU);

    if (mesh != nullptr && mesh->vertices != nullptr && mesh->indices != nullptr &&
        mesh->vertex_count > 0u && mesh->index_count > 0u) {
        s_vertices = mesh->vertices;
        s_triangles = mesh->indices;
        s_vertex_count = mesh->vertex_count;
        s_triangle_count = mesh->index_count;
    } else {
        s_vertices = kCubeVertices;
        s_triangles = kCubeTriangles;
        s_vertex_count = 8u;
        s_triangle_count = 12u;
    }
    s_last_hash = 0u;
    return true;
}

void parallel_workload_shutdown(void)
{
    s_vertices = kCubeVertices;
    s_triangles = kCubeTriangles;
    s_vertex_count = 8u;
    s_triangle_count = 12u;
    s_last_hash = 0u;
}

ParallelFrameResult parallel_workload_run_frame_n(const Mat4* mvp, uint32_t per_cpu,
                                                   uint32_t timeout_ms)
{
    ParallelFrameResult result = {};
    if (per_cpu == 0u || per_cpu > PARALLEL_TRIANGLES_PER_CPU) {
        per_cpu = PARALLEL_TRIANGLES_PER_CPU;
    }
    result.master_triangles = per_cpu;

    if (mvp == nullptr || s_master_out == nullptr || s_slave_out == nullptr ||
        s_fallback_out == nullptr || s_slave_batch == nullptr) {
        return result;
    }

    ParallelBatch* slave_batch = static_cast<ParallelBatch*>(shared_ptr(s_slave_batch));
    fill_batch(slave_batch, mvp, per_cpu, per_cpu,
               static_cast<PreparedTriangle*>(shared_ptr(s_slave_out)));
    shared_barrier();

    const bool dispatched = slave_worker_dispatch(execute_batch, slave_batch);

    ParallelBatch master_batch;
    fill_batch(&master_batch, mvp, 0u, per_cpu, s_master_out);
    execute_batch(&master_batch);

    bool slave_ok = false;
    if (dispatched) {
        slave_ok = slave_worker_wait(timeout_ms == 0u ? kWaitTimeoutMs : timeout_ms);
    }

    if (!slave_ok) {
        ParallelBatch fallback_batch;
        fill_batch(&fallback_batch, mvp, per_cpu, per_cpu, s_fallback_out);
        execute_batch(&fallback_batch);
        slave_worker_record_fallback();
    }

    const PreparedTriangle* second_half = slave_ok ? s_slave_out : s_fallback_out;
    const uint32_t hash = hash_outputs_n(second_half, per_cpu);

    result.ok = true;
    result.used_slave = slave_ok;
    result.fallback = !slave_ok;
    result.hash = hash;
    result.slave_triangles = slave_ok ? per_cpu : 0u;
    result.wait_us = slave_ok ? slave_worker_stats().last_wait_us : 0u;
    s_last_hash = hash;
    return result;
}

ParallelFrameResult parallel_workload_run_frame(const Mat4* mvp, uint32_t timeout_ms)
{
    return parallel_workload_run_frame_n(mvp, PARALLEL_TRIANGLES_PER_CPU, timeout_ms);
}

ParallelBenchResult parallel_workload_benchmark(const Mat4* mvp, uint32_t iterations)
{
    ParallelBenchResult result = {};
    if (mvp == nullptr || iterations == 0u || s_master_out == nullptr || s_slave_out == nullptr ||
        s_fallback_out == nullptr || s_slave_batch == nullptr) {
        return result;
    }
    result.iterations = iterations;
    result.hashes_match = true;

    uint32_t single_total = 0u;
    uint32_t dual_total = 0u;

    for (uint32_t iteration = 0u; iteration < iterations; ++iteration) {
        // --- single-CPU baseline: обе половины на Master ---
        const uint16_t single_start = profile_ticks();
        ParallelBatch batch0;
        fill_batch(&batch0, mvp, 0u, PARALLEL_TRIANGLES_PER_CPU, s_master_out);
        execute_batch(&batch0);
        ParallelBatch batch1;
        fill_batch(&batch1, mvp, PARALLEL_TRIANGLES_PER_CPU, PARALLEL_TRIANGLES_PER_CPU,
                   s_fallback_out);
        execute_batch(&batch1);
        const uint16_t single_end = profile_ticks();
        single_total += profile_elapsed_us(single_start, single_end);

        constexpr uint32_t kChunkBytes =
            static_cast<uint32_t>(sizeof(PreparedTriangle)) * PARALLEL_TRIANGLES_PER_CPU;
        uint32_t single_hash = kFnvOffset;
        single_hash = fnv1a_continue(single_hash, s_master_out, kChunkBytes);
        single_hash = fnv1a_continue(single_hash, s_fallback_out, kChunkBytes);

        // --- dual-CPU: вторая половина на Slave ---
        ParallelBatch* slave_batch = static_cast<ParallelBatch*>(shared_ptr(s_slave_batch));
        fill_batch(slave_batch, mvp, PARALLEL_TRIANGLES_PER_CPU, PARALLEL_TRIANGLES_PER_CPU,
                   static_cast<PreparedTriangle*>(shared_ptr(s_slave_out)));
        shared_barrier();

        const uint16_t dual_start = profile_ticks();
        const bool dispatched = slave_worker_dispatch(execute_batch, slave_batch);
        ParallelBatch master_batch;
        fill_batch(&master_batch, mvp, 0u, PARALLEL_TRIANGLES_PER_CPU, s_master_out);
        execute_batch(&master_batch);

        const bool slave_ok = dispatched && slave_worker_wait(kWaitTimeoutMs);
        const uint16_t dual_end = profile_ticks();
        dual_total += profile_elapsed_us(dual_start, dual_end);

        uint32_t dual_hash = kFnvOffset;
        dual_hash = fnv1a_continue(dual_hash, s_master_out, kChunkBytes);
        if (slave_ok) {
            result.slave_used = true;
            dual_hash = fnv1a_continue(dual_hash, s_slave_out, kChunkBytes);
        } else {
            ParallelBatch fallback_batch;
            fill_batch(&fallback_batch, mvp, PARALLEL_TRIANGLES_PER_CPU, PARALLEL_TRIANGLES_PER_CPU,
                       s_fallback_out);
            execute_batch(&fallback_batch);
            slave_worker_record_fallback();
            dual_hash = fnv1a_continue(dual_hash, s_fallback_out, kChunkBytes);
        }

        result.single_hash = single_hash;
        result.dual_hash = dual_hash;
        if (single_hash != dual_hash) {
            result.hashes_match = false;
        }
    }

    result.single_us = single_total / iterations;
    result.dual_us = dual_total / iterations;
    if (result.single_us > result.dual_us) {
        result.speedup_pct = ((result.single_us - result.dual_us) * 100u) / result.single_us;
    }
    result.ok = result.hashes_match;
    return result;
}

uint32_t parallel_workload_last_hash(void)
{
    return s_last_hash;
}

uint32_t parallel_workload_shared_bytes(void)
{
    return kSharedStaticBytes;
}

uint32_t parallel_workload_shared_used_bytes(void)
{
    return kSharedStaticBytes;
}

}  // namespace saturn
