// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/memory/memory_profiler.cpp
//
// Реализация профилировщика памяти MVP. Не использует heap/dynamic allocation:
// все источники хранятся в статическом массиве.
#include "core/memory/memory_profiler.h"

#include "core/log/log.h"

#include <stdint.h>
#include <string.h>

#define MEMORY_PROFILER_MAX_SOURCES 32u

struct MemorySource
{
    const char* name;
    MemoryProfilerSourceType type;
    size_t capacity;
    MemoryProfilerUsedFn used_fn;
    void* user;
};

static MemorySource s_sources[MEMORY_PROFILER_MAX_SOURCES];
static size_t s_source_count = 0u;
static size_t s_budgets[2] = {0u, 0u};

static size_t static_used_callback(void* user)
{
    return (size_t)(uintptr_t)user;
}

static const char* type_tag(MemoryProfilerSourceType type)
{
    return (type == MEMORY_PROFILER_VRAM) ? "VRAM" : "RAM";
}

void memory_profiler_reset(void)
{
    memset(s_sources, 0, sizeof(s_sources));
    s_source_count = 0u;
    s_budgets[0] = 0u;
    s_budgets[1] = 0u;
}

void memory_profiler_set_budget(MemoryProfilerSourceType type, size_t budget_bytes)
{
    if (type <= MEMORY_PROFILER_VRAM) {
        s_budgets[type] = budget_bytes;
    }
}

void memory_profiler_register_source(const char* name, MemoryProfilerSourceType type,
                                     size_t capacity, MemoryProfilerUsedFn used_fn, void* user)
{
    if (name == nullptr || s_source_count >= MEMORY_PROFILER_MAX_SOURCES) {
        return;
    }
    MemorySource* src = &s_sources[s_source_count];
    ++s_source_count;
    src->name = name;
    src->type = type;
    src->capacity = capacity;
    src->used_fn = used_fn;
    src->user = user;
}

void memory_profiler_register_static(const char* name, MemoryProfilerSourceType type, size_t size)
{
    memory_profiler_register_source(name, type, size, static_used_callback, (void*)(uintptr_t)size);
}

size_t memory_profiler_total_used(MemoryProfilerSourceType type)
{
    if (type > MEMORY_PROFILER_VRAM) {
        return 0u;
    }
    size_t total = 0u;
    for (size_t i = 0u; i < s_source_count; ++i) {
        const MemorySource* src = &s_sources[i];
        if (src->type == type) {
            total += (src->used_fn != nullptr) ? src->used_fn(src->user) : 0u;
        }
    }
    return total;
}

void memory_profiler_print_stats(void)
{
    size_t total_used[2] = {0u, 0u};

    Log_Message(LOG_LEVEL_INFO, "memory", "MemoryProfiler stats:");
    for (size_t i = 0u; i < s_source_count; ++i) {
        const MemorySource* src = &s_sources[i];
        const size_t used = (src->used_fn != nullptr) ? src->used_fn(src->user) : 0u;
        total_used[src->type] += used;
        const uint32_t pct =
            (src->capacity > 0u)
                ? (uint32_t)((unsigned long long)used * 100ull / (unsigned long long)src->capacity)
                : 0u;
        Log_Message(LOG_LEVEL_INFO, "memory", "  %s %s: %llu / %llu bytes (%u%%)",
                    type_tag(src->type), src->name, (unsigned long long)used,
                    (unsigned long long)src->capacity, pct);
    }

    for (int t = 0; t < 2; ++t) {
        const MemoryProfilerSourceType type = (MemoryProfilerSourceType)t;
        const uint32_t pct = (s_budgets[t] > 0u)
                                 ? (uint32_t)((unsigned long long)total_used[t] * 100ull /
                                              (unsigned long long)s_budgets[t])
                                 : 0u;
        Log_Message(LOG_LEVEL_INFO, "memory", "  total %s: %llu / %llu budget bytes (%u%%)",
                    type_tag(type), (unsigned long long)total_used[t],
                    (unsigned long long)s_budgets[t], pct);
    }
}

bool memory_profiler_check_thresholds(uint32_t ram_percent_threshold,
                                      uint32_t vram_percent_threshold)
{
    size_t total_used[2] = {0u, 0u};
    bool ok = true;

    for (size_t i = 0u; i < s_source_count; ++i) {
        const MemorySource* src = &s_sources[i];
        const uint32_t threshold =
            (src->type == MEMORY_PROFILER_VRAM) ? vram_percent_threshold : ram_percent_threshold;
        const size_t used = (src->used_fn != nullptr) ? src->used_fn(src->user) : 0u;
        total_used[src->type] += used;

        if (src->capacity > 0u &&
            (unsigned long long)used * 100ull >
                (unsigned long long)src->capacity * (unsigned long long)threshold) {
            Log_Message(LOG_LEVEL_ERROR, "memory",
                        "ALERT: %s %s usage %llu/%llu bytes exceeds %u%% threshold",
                        type_tag(src->type), src->name, (unsigned long long)used,
                        (unsigned long long)src->capacity, threshold);
            ok = false;
        }
    }

    for (int t = 0; t < 2; ++t) {
        const uint32_t threshold =
            (t == (int)MEMORY_PROFILER_VRAM) ? vram_percent_threshold : ram_percent_threshold;
        if (s_budgets[t] > 0u &&
            (unsigned long long)total_used[t] * 100ull >
                (unsigned long long)s_budgets[t] * (unsigned long long)threshold) {
            Log_Message(LOG_LEVEL_ERROR, "memory",
                        "ALERT: total %s usage %llu/%llu bytes exceeds %u%% threshold",
                        type_tag((MemoryProfilerSourceType)t), (unsigned long long)total_used[t],
                        (unsigned long long)s_budgets[t], threshold);
            ok = false;
        }
    }

    return ok;
}
