// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp1_texture.h
//
// Реестр VDP1-текстур: CI8 8bpp + 256-цветный CLUT (Спринт 27–28).
//
// Ограничения VDP1:
//   - character pattern width кратна 8 (1..504), height 1..255;
//   - 8bpp CLUT — 256 слов в VDP2 Color RAM; RPE занимает палитры 1..7,
//     палитра 0 (первые 256 слов) остаётся VDP2-тексту Jo Engine;
//   - адрес текстуры задаётся в 8-байтовых единицах, поэтому VRAM-блок
//     выравнивается на 32 байта.
//
// Политика v0.2.0: `.tex` v1 допускает 256×256, но VDP1 pattern height
// максимум 255. Для 256-высокой текстуры загружаются строки 0..254
// (последняя строка теряется) — явный счётчик `height_clamped`, не silent
// drop. Splitting на несколько pattern'ов — future work (ADR-010).
//
// Модуль host-тестируемый: C-мост заменяется fake в тестах.
#pragma once

#include <stdint.h>

#include "hal/platform.h"
#include "hal/saturn/vdp1_vram.h"

namespace saturn {

/** Максимум живых VDP1-текстур (core-пул — RENDER_TEXTURE_MAX = 16). */
constexpr uint32_t VDP1_TEXTURE_MAX = 16u;

/** CLUT-палитры RPE: 1..7 (палитра 0 — VDP2-текст Jo Engine). */
constexpr uint8_t VDP1_CLUT_FIRST = 1u;
// 7 зарезервирована под VDP2 NBG1 background (S29–30).
constexpr uint8_t VDP1_CLUT_LAST = 6u;
constexpr uint8_t VDP1_CLUT_COUNT = VDP1_CLUT_LAST - VDP1_CLUT_FIRST + 1u;

/** Диагностика реестра текстур. Порядок — контракт. */
enum Vdp1TextureDiag
{
    VDP1_TEX_OK = 0,
    VDP1_TEX_NOT_INITIALIZED,
    VDP1_TEX_NULL_ARGUMENT,
    VDP1_TEX_BAD_SIZE,
    VDP1_TEX_BAD_HANDLE,
    VDP1_TEX_DUPLICATE_HANDLE,
    VDP1_TEX_POOL_FULL,
    VDP1_TEX_VRAM_OUT_OF_MEMORY,
    VDP1_TEX_CLUT_OUT_OF_MEMORY,
    VDP1_TEX_UPLOAD_FAILED,
    VDP1_TEX_COUNT
};

/** Запись живой VDP1-текстуры. */
struct Vdp1Texture
{
    bool used;
    TextureHandle handle;
    uint16_t width;          /**< Ширина (кратна 8, 8..256). */
    uint16_t height;         /**< Фактическая height pattern (1..255). */
    uint16_t source_height;  /**< height исходного .tex (256 → 255). */
    uint16_t stride;         /**< width (8bpp). */
    uint16_t src_addr8;      /**< Абсолютный VDP1 VRAM адрес / 8. */
    uint32_t vram_offset;    /**< Смещение от VDP1_TEXTURE_BASE. */
    uint8_t clut;            /**< Палитра 1..7; CMDCOLR = clut * 256. */
};

/**
 * Реестр текстур поверх Vdp1TextureAllocator.
 *
 * Все операции без heap; ошибка любой стадии откатывает VRAM и CLUT
 * (persistent-состояние не меняется).
 */
class Vdp1TextureRegistry {
public:
    constexpr Vdp1TextureRegistry()
        : m_allocator(),
          m_slots{},
          m_clut_used{},
          m_inited(false),
          m_height_clamped(0u),
          m_diag(VDP1_TEX_OK)
    {
    }

    Vdp1TextureRegistry(const Vdp1TextureRegistry&) = delete;
    Vdp1TextureRegistry& operator=(const Vdp1TextureRegistry&) = delete;

    /** Сброс реестра и аллокатора. */
    void init();

    /** Полный сброс. */
    void shutdown();

    /**
     * Загрузка текстуры.
     *
     * @param handle Хэндл ядровой текстуры (не 0, уникальный).
     * @param width Ширина 8..256, кратна 8.
     * @param height Высота 1..256 (256 клампится до 255).
     * @param pixels CI8-пиксели width×height (не NULL).
     * @param palette 256 × RGB555+STP (не NULL).
     * @return true при успехе.
     */
    bool create(TextureHandle handle, uint32_t width, uint32_t height, const uint8_t* pixels,
                const uint16_t* palette);

    /** Освобождение текстуры (идемпотентно; невалидный handle — false). */
    bool destroy(TextureHandle handle);

    /** @return Текстура по хэндлу или nullptr. */
    const Vdp1Texture* find(TextureHandle handle) const;

    /** @return Число живых текстур. */
    uint32_t texture_count() const;

    /** @return Занятые байты VDP1 texture area. */
    uint32_t vram_used() const { return m_allocator.used(); }

    /** @return Число занятых CLUT-палитр. */
    uint32_t clut_count() const;

    /** @return Число текстур, у которых height был кламплен (256 → 255). */
    uint32_t height_clamped_count() const { return m_height_clamped; }

    /** @return Последний диагностический код. */
    Vdp1TextureDiag diag() const { return m_diag; }

    /** @return Диагностический код строкой. */
    static const char* diag_string(Vdp1TextureDiag diag);

private:
    Vdp1Texture* find_mutable(TextureHandle handle);
    uint8_t allocate_clut();
    void release_clut(uint8_t clut);

    Vdp1TextureAllocator m_allocator;
    Vdp1Texture m_slots[VDP1_TEXTURE_MAX];
    bool m_clut_used[8];
    bool m_inited;
    uint32_t m_height_clamped;
    Vdp1TextureDiag m_diag;
};

/** @return Глобальный реестр VDP1-текстур. */
Vdp1TextureRegistry& vdp1_textures(void);

}  // namespace saturn
