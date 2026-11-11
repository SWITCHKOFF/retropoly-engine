// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp1_vram.h
//
// Карта VDP1 VRAM и first-fit аллокатор текстур (Спринт 27–28).
//
// VDP1 VRAM — 512 KiB по адресу 0x25C00000 (Jo Engine: JO_VDP1_VRAM).
// Framebuffer A/B — отдельные 2×256 KiB по 0x25C80000 (JO_VDP1_FB), в этот
// регион не входят. Карта повторяет pinned Jo Engine v2026.2:
//
//   0x00000..0x0001F  jump table (VDP1 читает его первым)
//   0x00020..0x07FFF  command table A
//   0x08000..0x0FFFF  command table B
//   0x10000..0x7E4F7  texture area (RPE; 451 832 байт)
//   0x7E4F8..0x7E6F7  RPE Gouraud tables (64 × 8 байт, 2 набора по 32)
//   0x7E700..0x7FF7F  Jo Engine 3D clip Gouraud tables (не используются:
//                     JO_COMPILE_WITH_3D_SUPPORT не включён)
//   0x7FF80..0x7FEFF  reserved
//   0x7FF00..0x7FFFF  Jo Engine light Gouraud tables (32 × 8 байт)
//
// Модуль host-тестируемый: не включает Jo Engine/SH-2. Адреса — смещения
// относительно базы VDP1 VRAM; перевод в 8-байтовые единицы VDP1 делает
// backend/мост.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/saturn/saturn_memory.h"

namespace saturn {

// ---------------------------------------------------------------------------
// Нормативная карта VDP1 VRAM (ADR-010)
// ---------------------------------------------------------------------------

constexpr uint32_t VDP1_VRAM_SIZE = VDP1_VRAM_BYTES;  // 512 KiB (saturn_memory.h)

constexpr uint32_t VDP1_JUMP_TABLE_OFFSET = 0x00000u;
constexpr uint32_t VDP1_JUMP_TABLE_BYTES = 0x20u;

constexpr uint32_t VDP1_COMMAND_TABLE_A_OFFSET = 0x00020u;
constexpr uint32_t VDP1_COMMAND_TABLE_A_BYTES = 0x7FE0u;  // до 0x08000
constexpr uint32_t VDP1_COMMAND_TABLE_B_OFFSET = 0x08000u;
constexpr uint32_t VDP1_COMMAND_TABLE_B_BYTES = 0x8000u;  // до 0x10000

constexpr uint32_t VDP1_TEXTURE_BASE = 0x10000u;
constexpr uint32_t VDP1_TEXTURE_BYTES = 0x6E4F8u;  // до 0x7E4F8
constexpr uint32_t VDP1_TEXTURE_END = VDP1_TEXTURE_BASE + VDP1_TEXTURE_BYTES;

// Аллокатор выдаёт блоки, выровненные на 32 байта, поэтому последние
// (VDP1_TEXTURE_BYTES % 32) = 24 байта области — alignment padding.
constexpr uint32_t VDP1_TEXTURE_USABLE_BYTES = (VDP1_TEXTURE_BYTES / 32u) * 32u;
static_assert((VDP1_TEXTURE_USABLE_BYTES % 32u) == 0u,
              "VDP1 texture allocator capacity must be 32-byte aligned");
static_assert(VDP1_TEXTURE_USABLE_BYTES <= VDP1_TEXTURE_BYTES,
              "VDP1 usable texture bytes must fit the texture area");

constexpr uint32_t VDP1_GOURAUD_BASE = VDP1_TEXTURE_END;  // 0x7E4F8
constexpr uint32_t VDP1_GOURAUD_SLOTS = 64u;
constexpr uint32_t VDP1_GOURAUD_ENTRY_BYTES = 8u;
constexpr uint32_t VDP1_GOURAUD_BYTES = VDP1_GOURAUD_SLOTS * VDP1_GOURAUD_ENTRY_BYTES;
constexpr uint32_t VDP1_GOURAUD_END = VDP1_GOURAUD_BASE + VDP1_GOURAUD_BYTES;  // 0x7E6F8

// Наборы Gouraud-таблиц чередуются по чётности кадра: VDP1 может ещё
// дорисовывать предыдущий кадр, когда Master начинает новый.
constexpr uint32_t VDP1_GOURAUD_SET_SLOTS = VDP1_GOURAUD_SLOTS / 2u;  // 32
constexpr uint32_t VDP1_GOURAUD_SET_BYTES = VDP1_GOURAUD_SET_SLOTS * VDP1_GOURAUD_ENTRY_BYTES;

constexpr uint32_t VDP1_JO3D_CLIP_BASE = 0x7E700u;  // Jo Engine, не используется
constexpr uint32_t VDP1_JO3D_CLIP_END = 0x7FF00u;
constexpr uint32_t VDP1_LIGHT_BASE = 0x7FF00u;  // Jo Engine, 32 таблицы
constexpr uint32_t VDP1_LIGHT_END = 0x80000u;

// Контракт «карта не пересекается и покрывает ровно 512 KiB».
static_assert(VDP1_JUMP_TABLE_OFFSET + VDP1_JUMP_TABLE_BYTES == VDP1_COMMAND_TABLE_A_OFFSET,
              "VDP1 jump table must precede command table A");
static_assert(VDP1_COMMAND_TABLE_A_OFFSET + VDP1_COMMAND_TABLE_A_BYTES ==
                  VDP1_COMMAND_TABLE_B_OFFSET,
              "VDP1 command table A must end at table B");
static_assert(VDP1_COMMAND_TABLE_B_OFFSET + VDP1_COMMAND_TABLE_B_BYTES == VDP1_TEXTURE_BASE,
              "VDP1 command table B must end at texture base");
static_assert(VDP1_TEXTURE_BASE + VDP1_TEXTURE_BYTES == VDP1_GOURAUD_BASE,
              "VDP1 texture area must end at RPE Gouraud tables");
static_assert(VDP1_GOURAUD_END <= VDP1_JO3D_CLIP_BASE,
              "RPE Gouraud tables must not overlap Jo Engine 3D clip tables");
static_assert(VDP1_JO3D_CLIP_END == VDP1_LIGHT_BASE,
              "Jo Engine 3D clip tables must end at light tables");
static_assert(VDP1_LIGHT_END == VDP1_VRAM_SIZE, "VDP1 map must cover exactly 512 KiB");

// ---------------------------------------------------------------------------
// Аллокатор текстур
// ---------------------------------------------------------------------------

/** Невалидное смещение аллокатора. */
constexpr uint32_t VDP1_TEXTURE_INVALID_OFFSET = 0xFFFFFFFFu;

/** Максимум одновременно живых блоков (core-пул текстур — 16). */
constexpr uint32_t VDP1_TEXTURE_MAX_BLOCKS = 16u;

/** Диагностика аллокатора. Порядок — контракт. */
enum Vdp1VramDiag
{
    VDP1_VRAM_OK = 0,
    VDP1_VRAM_BAD_ARGUMENT,
    VDP1_VRAM_OUT_OF_MEMORY,
    VDP1_VRAM_FOREIGN_OFFSET,
    VDP1_VRAM_DOUBLE_FREE,
    VDP1_VRAM_NO_BLOCKS,
    VDP1_VRAM_COUNT
};

/**
 * First-fit free-list аллокатор области текстур VDP1.
 *
 * Адреса — смещения относительно VDP1_TEXTURE_BASE (не абсолютные).
 * Выравнивание — 32 байта (VDP1 требует /8; 32 — безопасный запас).
 * Блоки хранятся в самом объекте (без heap); освобождение коалесцирует
 * соседей. Все операции детерминированы.
 */
class Vdp1TextureAllocator {
public:
    constexpr Vdp1TextureAllocator()
        : m_blocks{}, m_count(0u), m_inited(false), m_diag(VDP1_VRAM_OK)
    {
    }

    Vdp1TextureAllocator(const Vdp1TextureAllocator&) = delete;
    Vdp1TextureAllocator& operator=(const Vdp1TextureAllocator&) = delete;

    /** Сброс в один свободный блок [0, capacity). */
    void init();

    /** Полный сброс (capacity = 0). */
    void shutdown();

    /**
     * Выделение @p bytes байт с выравниванием 32.
     *
     * @param bytes Размер; 0 — отказ.
     * @return Смещение от VDP1_TEXTURE_BASE или VDP1_TEXTURE_INVALID_OFFSET.
     */
    uint32_t allocate(uint32_t bytes);

    /**
     * Освобождение блока по смещению.
     *
     * @param offset Смещение, ровно совпадающее с выданным.
     * @return true при успехе; false — foreign/double free.
     */
    bool free(uint32_t offset);

    /** @return Занятые байты (payload блоков). */
    uint32_t used() const;

    /** @return Свободные байты (включая headers свободных блоков). */
    uint32_t available() const;

    /** @return Ёмкость области (VDP1_TEXTURE_USABLE_BYTES). */
    uint32_t capacity() const { return VDP1_TEXTURE_USABLE_BYTES; }

    /** @return true после init(). */
    bool inited() const { return m_inited; }

    /** @return Последний диагностический код. */
    Vdp1VramDiag diag() const { return m_diag; }

    /** @return Диагностический код строкой. */
    static const char* diag_string(Vdp1VramDiag diag);

private:
    struct Block
    {
        uint32_t offset;  /**< Смещение от базы области. */
        uint32_t size;    /**< Размер блока. */
        uint8_t used;     /**< 1 — занят. */
    };

    void insert_block(uint32_t offset, uint32_t size, uint8_t used);
    void coalesce();

    Block m_blocks[VDP1_TEXTURE_MAX_BLOCKS];
    uint32_t m_count;
    bool m_inited;
    Vdp1VramDiag m_diag;
};

}  // namespace saturn
