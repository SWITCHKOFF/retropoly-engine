// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_memory.h
//
// Бюджет памяти Sega Saturn (ADR-007/ADR-008, Спринты 21–24).
//
// Reference-факты сверены по первоисточникам (docs/SATURN.md):
//   - Work RAM: 1 MiB Work RAM-H (0x06000000, высокоскоростная шина CPU)
//     + 1 MiB Work RAM-L (0x00200000, низкоскоростная общая шина) = 2 MiB;
//   - Sound RAM: 512 KiB (SCSP);
//   - VDP1: 512 KiB VRAM + два 256 KiB frame buffer (A/B);
//   - VDP2: 512 KiB VRAM + 4 KiB Color RAM (CRAM);
//   - SCU DSP: 32 KiB SRAM (в бюджет движка не входит).
//
// Runtime new/delete/malloc/free запрещены (ADR-007): все долгоживущие
// данные — static storage/арены, кадровые — StackAllocator/LinearAllocator.
// Заголовок не включает Jo Engine/SH-2: только числовые контракты бюджета
// и host-совместимые типы (модуль памяти тестируется на хосте).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/memory/stack_allocator.h"

namespace saturn {

constexpr uint32_t WRAM_H_BYTES = 1024u * 1024u;
constexpr uint32_t WRAM_L_BYTES = 1024u * 1024u;
constexpr uint32_t WORK_RAM_BYTES = WRAM_H_BYTES + WRAM_L_BYTES;

constexpr uint32_t SOUND_RAM_BYTES = 512u * 1024u;
constexpr uint32_t SCU_DSP_SRAM_BYTES = 32u * 1024u;

constexpr uint32_t VDP1_VRAM_BYTES = 512u * 1024u;
constexpr uint32_t VDP1_FRAME_BUFFER_BYTES = 256u * 1024u;
constexpr uint32_t VDP1_FRAME_BUFFER_COUNT = 2u;
constexpr uint32_t VDP2_VRAM_BYTES = 512u * 1024u;
constexpr uint32_t VDP2_COLOR_RAM_BYTES = 4u * 1024u;

// Jo Engine linker script: программа начинается с 0x06004000, master stack
// занимает верхние 16 KiB Work RAM-H (0x060FBC00..0x060FFC00). Верх
// Work RAM-H между концом образа и 0x060FBC00 отдаётся jo_malloc().
constexpr uint32_t MASTER_STACK_BYTES = 16u * 1024u;
constexpr uint32_t WRAM_H_PROGRAM_LIMIT_BYTES = WRAM_H_BYTES - MASTER_STACK_BYTES;

// v0.2.0: Work RAM-L (0x00200000) целиком отдан HAL и разбит на четыре региона:
//   [0 KiB .. 932 KiB)     asset heap — .mesh/.tex и прочие persistent-ассеты;
//   [932 KiB .. 980 KiB)   frame scratch — StackAllocator кадра (48 KiB);
//   [980 KiB .. 996 KiB)   command arena — каркас VDP1-команд (16 KiB);
//   [996 KiB .. 1024 KiB)  parallel work — shared Master/Slave (28 KiB, S25–26).
// Разбиение фиксировано static_assert'ами: сумма ровно 1 MiB.
constexpr uint32_t ASSET_HEAP_BYTES = 932u * 1024u;
constexpr uint32_t FRAME_SCRATCH_BYTES = 48u * 1024u;
constexpr uint32_t COMMAND_BUFFER_BYTES = 16u * 1024u;
constexpr uint32_t PARALLEL_WORK_BYTES = 28u * 1024u;
constexpr uint32_t WRAM_L_ALLOCATED_BYTES =
    ASSET_HEAP_BYTES + FRAME_SCRATCH_BYTES + COMMAND_BUFFER_BYTES + PARALLEL_WORK_BYTES;

// Историческое имя бюджета ассетов v0.2.0 (совместимость docs/tests).
constexpr uint32_t ASSET_ARENA_BYTES = ASSET_HEAP_BYTES;

static_assert(WORK_RAM_BYTES == 2u * 1024u * 1024u,
              "Saturn Work RAM contract must stay 2 MiB (WRAM-H + WRAM-L)");
static_assert(WRAM_H_BYTES == WRAM_L_BYTES, "Saturn WRAM-H/WRAM-L split must stay 1 MiB + 1 MiB");
static_assert(SOUND_RAM_BYTES == 512u * 1024u, "Saturn sound RAM contract must stay 512 KiB");
static_assert(VDP1_VRAM_BYTES == 512u * 1024u, "VDP1 VRAM contract must stay 512 KiB (4 Mbit)");
static_assert(VDP1_FRAME_BUFFER_BYTES * VDP1_FRAME_BUFFER_COUNT == 512u * 1024u,
              "VDP1 frame buffers are 2 x 256 KiB (A/B page flip)");
static_assert(VDP2_VRAM_BYTES == 512u * 1024u,
              "VDP2 VRAM contract must stay 512 KiB (4 Mbit standard fit)");
static_assert(VDP2_COLOR_RAM_BYTES == 4u * 1024u,
              "VDP2 Color RAM contract must stay 4 KiB (32 Kbit)");
static_assert(SCU_DSP_SRAM_BYTES == 32u * 1024u, "SCU DSP SRAM contract must stay 32 KiB");
static_assert(WRAM_H_PROGRAM_LIMIT_BYTES + MASTER_STACK_BYTES == WRAM_H_BYTES,
              "WRAM-H ledger must cover program + master stack");
static_assert(WRAM_L_ALLOCATED_BYTES == WRAM_L_BYTES,
              "WRAM-L ledger must cover assets + scratch + command + parallel work");
static_assert(ASSET_ARENA_BYTES == ASSET_HEAP_BYTES, "asset arena alias must match asset heap");

/** Диагностический код операций арены ассетов. Порядок — контракт. */
enum AssetHeapDiag
{
    ASSET_HEAP_OK = 0,
    ASSET_HEAP_NOT_INITIALIZED,
    ASSET_HEAP_NULL_ARGUMENT,
    ASSET_HEAP_BAD_ALIGNMENT,
    ASSET_HEAP_OUT_OF_MEMORY,
    ASSET_HEAP_FOREIGN_POINTER,
    ASSET_HEAP_DOUBLE_FREE,
    ASSET_HEAP_COUNT
};

/**
 * First-fit free-list аллокатор persistent-ассетов в Work RAM-L.
 *
 * Блоки хранятся в самой арене (header перед payload), список — в порядке
 * адресов. Освобождение коалесцирует соседей. Двойное освобождение и
 * чужие указатели распознаются и возвращают false (без краша/порчи heap).
 *
 * Не владеет буфером: арена подключается через init(). Глобальный экземпляр
 * constant-initialized (без C++ static constructor).
 */
class AssetHeap {
public:
    constexpr AssetHeap()
        : m_buffer(nullptr), m_capacity(0u), m_inited(false), m_head(nullptr), m_diag(ASSET_HEAP_OK)
    {
    }

    AssetHeap(const AssetHeap&) = delete;
    AssetHeap& operator=(const AssetHeap&) = delete;

    /**
     * Подключение арены (выравнивание базы — 8 байт).
     *
     * @param buffer Начало арены.
     * @param capacity Размер арены; всё, что меньше header+8 байт, отвергается.
     * @return true при успехе.
     */
    bool init(void* buffer, size_t capacity);

    /** Полный сброс арены в один свободный блок. */
    void reset();

    /**
     * Выделение @p size байт с выравниванием payload.
     *
     * @param size Размер; 0 — отказ.
     * @param alignment Степень двойки; значения < 8 поднимаются до 8.
     * @return Указатель или nullptr (диагностика — diag()).
     */
    void* allocate(size_t size, size_t alignment = 8u);

    /**
     * Освобождение блока, выданного этим heap.
     *
     * @param pointer Указатель, ровно совпадающий с payload блока.
     * @return true при успехе; false — foreign pointer/double free.
     */
    bool free(void* pointer);

    /** @return true, если арена подключена. */
    bool inited() const { return m_inited; }

    /** @return Ёмкость арены в байтах (включая headers). */
    size_t capacity() const { return m_capacity; }

    /** @return Занятые байты (capacity − свободные блоки). */
    size_t used() const;

    /** @return Свободные байты (включая headers свободных блоков). */
    size_t available() const;

    /** @return Последний диагностический код. */
    AssetHeapDiag diag() const { return m_diag; }

    /** @return Диагностический код в виде статической строки. */
    static const char* diag_string(AssetHeapDiag diag);

private:
    struct Block;
    static size_t header_size();
    static void* payload_of(Block* block);
    void split(Block* block, size_t total_bytes);

    uint8_t* m_buffer;
    size_t m_capacity;
    bool m_inited;
    Block* m_head;
    AssetHeapDiag m_diag;
};

/**
 * Инициализация памяти Saturn: подключение трёх регионов Work RAM-L, сброс
 * арен и регистрация источников в MemoryProfiler. Идемпотентна.
 *
 * @return true при успехе.
 */
bool memory_init(void);

/** Сброс арен (без отключения буфера). Идемпотентна. */
void memory_shutdown(void);

/** @return Глобальный asset heap (Work RAM-L, 932 KiB). */
AssetHeap& asset_heap(void);

/** @return Кадровая scratch-арена (Work RAM-L, 48 KiB). */
StackAllocator& frame_scratch(void);

/** @return Адрес базы Work RAM-L, используемой HAL. */
uint8_t* wram_l_base(void);

/**
 * @return База shared-региона Master/Slave (Work RAM-L, 28 KiB).
 *
 * Регион выровнен на 32 байта; доступ с обоих CPU — через cache-through
 * alias (saturn::shared_ptr). Владелец — parallel workload + slave worker.
 */
uint8_t* parallel_work_base(void);

/** @return Размер shared-региона Master/Slave в байтах. */
constexpr uint32_t parallel_work_bytes()
{
    return PARALLEL_WORK_BYTES;
}

}  // namespace saturn
