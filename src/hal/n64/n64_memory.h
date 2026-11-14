// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_memory.h
//
// Memory ledger N64 (Спринт 41–42, ADR-014): RDRAM 4 MiB base, статические
// арены движка, запрет heap и профилирование current/peak.
//
// Ledger (policy v0.3.0, 4 MiB base):
//   RDRAM base                       4194304 B (4 MiB)
//   CPU stack (libdragon STACK_SIZE)   65536 B (64 KiB)
//   VI framebuffers 2×320×240×2       307200 B (300 KiB)
//   asset arena                      1572864 B (1.5 MiB)
//   frame arena                       262144 B (256 KiB)
//   command arena                     131072 B (128 KiB)
//   FS staging arena                  131072 B (128 KiB)
//   code/data/SDK reserve            остаток ledger (≥ 1 MiB)
//
// Expansion Pak (8 MiB) определяется, но бюджет v0.3.0 — только 4 MiB base;
// 8 MiB не обязателен и не используется для роста арен.
//
// Heap запрещён: все арены — static storage (.bss), выделение — только
// LinearAllocator. Заголовок не включает libdragon и host-тестируем.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/memory/linear_allocator.h"
#include "retro_engine/hal/platform.h" /* RENDER_FB_WIDTH/HEIGHT */

namespace n64 {

// --- Hardware/VI contract ---------------------------------------------------
constexpr uint32_t RDRAM_BASE_BYTES = 4u * 1024u * 1024u;
constexpr uint32_t RDRAM_EXPANDED_BYTES = 8u * 1024u * 1024u;
/** Порог «Expansion Pak есть» (libdragon is_memory_expanded ≈ 7.75 MiB). */
constexpr uint32_t RDRAM_EXPANSION_THRESHOLD_BYTES = 6u * 1024u * 1024u;
constexpr uint32_t CPU_STACK_BYTES = 64u * 1024u;
constexpr uint32_t FRAMEBUFFER_COUNT = 2u;
constexpr uint32_t FRAMEBUFFER_BYTES =
    (uint32_t)RENDER_FB_WIDTH * (uint32_t)RENDER_FB_HEIGHT * 2u * FRAMEBUFFER_COUNT;

// --- Engine arenas ----------------------------------------------------------
constexpr uint32_t ASSET_ARENA_BYTES = 1536u * 1024u;
constexpr uint32_t FRAME_ARENA_BYTES = 256u * 1024u;
constexpr uint32_t COMMAND_ARENA_BYTES = 128u * 1024u;
constexpr uint32_t FS_STAGING_BYTES = 128u * 1024u;
constexpr uint32_t ENGINE_ARENAS_BYTES =
    ASSET_ARENA_BYTES + FRAME_ARENA_BYTES + COMMAND_ARENA_BYTES + FS_STAGING_BYTES;

/** Остаток 4 MiB ledger: код/данные движка, libdragon и его boot-heap. */
constexpr uint32_t CODE_DATA_SDK_RESERVE_BYTES =
    RDRAM_BASE_BYTES - CPU_STACK_BYTES - FRAMEBUFFER_BYTES - ENGINE_ARENAS_BYTES;

static_assert(RENDER_FB_WIDTH == 320u && RENDER_FB_HEIGHT == 240u,
              "N64 reference VI must stay 320x240 (docs/N64.md)");
static_assert(FRAMEBUFFER_BYTES == 320u * 240u * 2u * 2u,
              "VI 16-bit double buffer must be 300 KiB");
static_assert(ENGINE_ARENAS_BYTES == 2u * 1024u * 1024u,
              "N64 engine arenas must fit exactly 2 MiB of the 4 MiB ledger");
static_assert(CPU_STACK_BYTES == 64u * 1024u, "libdragon STACK_SIZE is 64 KiB");
static_assert(ENGINE_ARENAS_BYTES + CPU_STACK_BYTES + FRAMEBUFFER_BYTES <= RDRAM_BASE_BYTES,
              "N64 ledger exceeds 4 MiB base RDRAM");
static_assert(CODE_DATA_SDK_RESERVE_BYTES >= 1024u * 1024u,
              "N64 ledger must leave at least 1 MiB for code/data/SDK");

/**
 * Линейная арена с учётом пикового использования.
 *
 * Не владеет буфером; constant-initialized (libdragon start не вызывает
 * C++ static constructors).
 */
class Arena {
public:
    constexpr Arena()
        : m_alloc(),
          m_peak(0u),
          m_capacity(0u),
          m_storage(nullptr),
          m_name(nullptr),
          m_inited(false)
    {
    }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    /**
     * Подключение/сброс арены.
     *
     * @param buffer static storage (nullptr — пустая арена).
     * @param capacity размер в байтах.
     * @param name статическая строка для профилировщика.
     */
    void init(void* buffer, size_t capacity, const char* name);

    /** Сброс used и peak (init/новый кадр). */
    void reset();

    /** Полное отключение арены (capacity/inited = 0). */
    void deinit()
    {
        m_alloc.init(nullptr, 0u);
        m_peak = 0u;
        m_capacity = 0u;
        m_storage = nullptr;
        m_inited = false;
    }

    /**
     * Выделение из арены с обновлением peak.
     *
     * @return выровненный указатель или nullptr (без изменения used).
     */
    void* allocate(size_t size, size_t alignment = 8u);

    LinearAllocator::Marker mark() const { return m_alloc.mark(); }
    bool rollback(LinearAllocator::Marker marker) { return m_alloc.rollback(marker); }

    /** Линейный аллокатор арены (для загрузчиков ассетов). */
    LinearAllocator& allocator() { return m_alloc; }

    /** Синхронизация peak после внешних аллокаций через allocator(). */
    void note_used()
    {
        if (m_alloc.used() > m_peak) {
            m_peak = m_alloc.used();
        }
    }

    size_t used() const { return m_alloc.used(); }
    size_t peak() const { return m_peak; }
    size_t capacity() const { return m_capacity; }
    const char* name() const { return m_name; }
    bool inited() const { return m_inited; }
    uint8_t* data() const { return m_storage; }

private:
    LinearAllocator m_alloc;
    size_t m_peak;
    size_t m_capacity;
    uint8_t* m_storage;
    const char* m_name;
    bool m_inited;
};

/**
 * Инициализация арен и регистрация источников MemoryProfiler.
 * Идемпотентна (повторный Init после Shutdown — чистое состояние).
 *
 * @return true — ledger согласован и арены готовы.
 */
bool memory_init();

/** Сброс арен/профилировщика (идемпотентно, безопасно без Init). */
void memory_shutdown();

/** @return true — memory_init выполнен. */
bool memory_ready();

Arena& asset_arena();
Arena& frame_arena();
Arena& command_arena();
Arena& fs_staging_arena();

/**
 * Статическая проверка ledger во время выполнения (лог + bool).
 *
 * Проверяет: 4 MiB base, сумму арен, обнаруженный RDRAM (4/8 MiB), heap-
 * бюджет. 8 MiB Expansion Pak не расширяет арены (policy v0.3.0).
 */
bool memory_ledger_check();

/** @return обнаруженный libdragon размер RDRAM (4/8 MiB; 0 — неизвестно). */
uint32_t memory_rdram_size();

/** @return true — Expansion Pak обнаружен (8 MiB). */
bool memory_expansion_pak();

/** Лог current/peak по аренам (MemoryProfiler + единая строка). */
void memory_log_stats();

/**
 * Компактная строка для debug-overlay: current/peak всех арен (байты).
 *
 * @param out буфер (не nullptr).
 * @param size размер буфера.
 * @return число записанных символов (без NUL) или 0.
 */
size_t memory_overlay_line(char* out, size_t size);

}  // namespace n64
