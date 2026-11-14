// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_memory.cpp
//
// Реализация memory ledger/арен N64 (контракт — n64_memory.h).
//
// Все буферы — static storage (.bss), без heap. Арены подключены к
// MemoryProfiler (current) и ведут peak самостоятельно.
#include "hal/n64/n64_memory.h"

#include "core/log/log.h"
#include "core/memory/memory_profiler.h"
#include "hal/n64/n64_bridge.h"

#include <stdio.h>

namespace n64 {

namespace {

alignas(8) uint8_t s_asset_storage[ASSET_ARENA_BYTES];
alignas(8) uint8_t s_frame_storage[FRAME_ARENA_BYTES];
alignas(8) uint8_t s_command_storage[COMMAND_ARENA_BYTES];
alignas(8) uint8_t s_fs_storage[FS_STAGING_BYTES];

Arena s_asset_arena;
Arena s_frame_arena;
Arena s_command_arena;
Arena s_fs_arena;
bool s_memory_ready = false;

size_t arena_used_callback(void* user)
{
    return (user != nullptr) ? static_cast<Arena*>(user)->used() : 0u;
}

void register_arena(Arena& arena)
{
    memory_profiler_register_source(arena.name(), MEMORY_PROFILER_RAM, arena.capacity(),
                                    arena_used_callback, &arena);
}

}  // namespace

void Arena::init(void* buffer, size_t capacity, const char* name)
{
    m_alloc.init(buffer, capacity);
    m_peak = 0u;
    m_capacity = (buffer != nullptr) ? capacity : 0u;
    m_storage = static_cast<uint8_t*>(buffer);
    m_name = name;
    m_inited = buffer != nullptr;
}

void Arena::reset()
{
    m_alloc.reset();
    m_peak = 0u;
}

void* Arena::allocate(size_t size, size_t alignment)
{
    void* result = m_alloc.allocate(size, alignment);
    if (result != nullptr && m_alloc.used() > m_peak) {
        m_peak = m_alloc.used();
    }
    return result;
}

bool memory_init()
{
    memory_profiler_reset();
    memory_profiler_set_budget(MEMORY_PROFILER_RAM, RDRAM_BASE_BYTES);
    memory_profiler_set_budget(MEMORY_PROFILER_VRAM, FRAMEBUFFER_BYTES);

    s_asset_arena.init(s_asset_storage, sizeof(s_asset_storage), "n64_asset");
    s_frame_arena.init(s_frame_storage, sizeof(s_frame_storage), "n64_frame");
    s_command_arena.init(s_command_storage, sizeof(s_command_storage), "n64_command");
    s_fs_arena.init(s_fs_storage, sizeof(s_fs_storage), "n64_fs");

    // Полный RDRAM-ledger: арены + stack + фиксированный резерв; framebuffer
    // учитывается как VRAM-источник (VI-память в RDRAM).
    register_arena(s_asset_arena);
    register_arena(s_frame_arena);
    register_arena(s_command_arena);
    register_arena(s_fs_arena);
    memory_profiler_register_static("n64_stack", MEMORY_PROFILER_RAM, CPU_STACK_BYTES);
    memory_profiler_register_static("n64_code_data_sdk", MEMORY_PROFILER_RAM,
                                    CODE_DATA_SDK_RESERVE_BYTES);
    memory_profiler_register_static("n64_framebuffers", MEMORY_PROFILER_VRAM, FRAMEBUFFER_BYTES);

    s_memory_ready = memory_ledger_check();
    return s_memory_ready;
}

void memory_shutdown()
{
    s_asset_arena.deinit();
    s_frame_arena.deinit();
    s_command_arena.deinit();
    s_fs_arena.deinit();
    s_memory_ready = false;
}

bool memory_ready()
{
    return s_memory_ready;
}

Arena& asset_arena()
{
    return s_asset_arena;
}

Arena& frame_arena()
{
    return s_frame_arena;
}

Arena& command_arena()
{
    return s_command_arena;
}

Arena& fs_staging_arena()
{
    return s_fs_arena;
}

uint32_t memory_rdram_size()
{
    return rpe_n64_rdram_size();
}

bool memory_expansion_pak()
{
    return memory_rdram_size() >= RDRAM_EXPANSION_THRESHOLD_BYTES;
}

bool memory_ledger_check()
{
    const uint32_t rdram = memory_rdram_size();
    const uint32_t ledger =
        ENGINE_ARENAS_BYTES + CPU_STACK_BYTES + FRAMEBUFFER_BYTES + CODE_DATA_SDK_RESERVE_BYTES;

    if (ENGINE_ARENAS_BYTES + CPU_STACK_BYTES + FRAMEBUFFER_BYTES > RDRAM_BASE_BYTES) {
        Log_Message(LOG_LEVEL_ERROR, "n64_mem", "ledger overflow: arenas+stack+fb exceed 4 MiB");
        return false;
    }
    if (ledger != RDRAM_BASE_BYTES) {
        Log_Message(LOG_LEVEL_ERROR, "n64_mem", "ledger mismatch: %u != %u bytes",
                    static_cast<unsigned>(ledger), static_cast<unsigned>(RDRAM_BASE_BYTES));
        return false;
    }
    if (rdram < RDRAM_BASE_BYTES) {
        Log_Message(LOG_LEVEL_ERROR, "n64_mem", "RDRAM detected %u B < 4 MiB base",
                    static_cast<unsigned>(rdram));
        return false;
    }
    return true;
}

void memory_log_stats()
{
    Log_Message(LOG_LEVEL_INFO, "n64_mem",
                "ledger: RDRAM=%u B (%s), arenas=%u, stack=%u, fb=%u, reserve=%u",
                static_cast<unsigned>(memory_rdram_size()),
                memory_expansion_pak() ? "8 MiB expansion" : "4 MiB base",
                static_cast<unsigned>(ENGINE_ARENAS_BYTES), static_cast<unsigned>(CPU_STACK_BYTES),
                static_cast<unsigned>(FRAMEBUFFER_BYTES),
                static_cast<unsigned>(CODE_DATA_SDK_RESERVE_BYTES));
    Log_Message(LOG_LEVEL_INFO, "n64_mem", "peak: asset=%u frame=%u command=%u fs=%u",
                static_cast<unsigned>(asset_arena().peak()),
                static_cast<unsigned>(frame_arena().peak()),
                static_cast<unsigned>(command_arena().peak()),
                static_cast<unsigned>(fs_staging_arena().peak()));
    memory_profiler_print_stats();
}

size_t memory_overlay_line(char* out, size_t size)
{
    if (out == nullptr || size == 0u) {
        return 0u;
    }
    const int written = snprintf(out, size, "A%u/%uK F%u/%uK C%u/%uK S%u/%uK",
                                 static_cast<unsigned>(asset_arena().used() / 1024u),
                                 static_cast<unsigned>(asset_arena().peak() / 1024u),
                                 static_cast<unsigned>(frame_arena().used() / 1024u),
                                 static_cast<unsigned>(frame_arena().peak() / 1024u),
                                 static_cast<unsigned>(command_arena().used() / 1024u),
                                 static_cast<unsigned>(command_arena().peak() / 1024u),
                                 static_cast<unsigned>(fs_staging_arena().used() / 1024u),
                                 static_cast<unsigned>(fs_staging_arena().peak() / 1024u));
    if (written < 0 || static_cast<size_t>(written) >= size) {
        out[size - 1u] = '\0';
        return size - 1u;
    }
    return static_cast<size_t>(written);
}

}  // namespace n64
