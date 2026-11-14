// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/scu_sync.cpp
//
// Реализация shared-атомиков и ограниченных ожиданий (S25–26).
// Никаких блокирующих ожиданий без таймаута: это требование sprint_25_26.
#include "hal/saturn/scu_sync.h"

#include "hal/saturn/jo_saturn_bridge.h"

namespace saturn {

uint32_t shared_load_u32(const volatile uint32_t* cell)
{
    if (cell == nullptr) {
        return 0u;
    }
    // 32-битный aligned load; на SH-2 lock-free.
    return __atomic_load_n(cell, __ATOMIC_ACQUIRE);
}

void shared_store_u32(volatile uint32_t* cell, uint32_t value)
{
    if (cell == nullptr) {
        return;
    }
    __atomic_store_n(cell, value, __ATOMIC_RELEASE);
}

void shared_barrier(void)
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

void cpu_relax(void)
{
    rpe_saturn_cpu_relax();
}

bool wait_u32_until(const volatile uint32_t* cell, uint32_t value, uint32_t timeout_ms,
                    ScuSyncDiag* out_diag)
{
    if (out_diag != nullptr) {
        *out_diag = SCU_SYNC_OK;
    }
    if (cell == nullptr) {
        if (out_diag != nullptr) {
            *out_diag = SCU_SYNC_NULL_ARGUMENT;
        }
        return false;
    }

    // FRT на 16 бит: накапливаем дельты, чтобы не потерять wrap (78 мс).
    // Таймаут вызывающего кода всегда < 60 с; uint32_us хватает.
    uint16_t previous = rpe_saturn_frt_ticks();
    uint32_t elapsed_us = 0u;
    const uint32_t timeout_us = timeout_ms * 1000u;

    for (;;) {
        if (shared_load_u32(cell) == value) {
            return true;
        }

        const uint16_t now = rpe_saturn_frt_ticks();
        elapsed_us += rpe_saturn_frt_ticks_to_us(static_cast<uint16_t>(now - previous));
        previous = now;

        if (elapsed_us >= timeout_us) {
            if (out_diag != nullptr) {
                *out_diag = SCU_SYNC_TIMEOUT;
            }
            return false;
        }
        cpu_relax();
    }
}

void* shared_ptr(const void* local)
{
    return rpe_saturn_cache_through(local);
}

void cache_invalidate_local(void)
{
    rpe_saturn_cache_purge();
}

uint32_t profile_elapsed_us(uint16_t start, uint16_t end)
{
    return rpe_saturn_frt_ticks_to_us(static_cast<uint16_t>(end - start));
}

uint16_t profile_ticks(void)
{
    return rpe_saturn_frt_ticks();
}

uint32_t profile_ticks_to_us(uint16_t ticks)
{
    return rpe_saturn_frt_ticks_to_us(ticks);
}

const char* diag_string(ScuSyncDiag diag)
{
    switch (diag) {
        case SCU_SYNC_OK:
            return "ok";
        case SCU_SYNC_NULL_ARGUMENT:
            return "null argument";
        case SCU_SYNC_TIMEOUT:
            return "timeout";
        case SCU_SYNC_DISABLED:
            return "disabled";
        case SCU_SYNC_COUNT:
        default:
            return "unknown";
    }
}

}  // namespace saturn
