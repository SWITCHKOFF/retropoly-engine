// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp2_backend.h
//
// VDP2-backend Saturn (Спринт 29–30): один NBG1 8bpp bitmap-слой
// (1024×256 VRAM, 320×224 screen), палитра 7, scroll и VBlank-коммит.
//
// VDP1 остаётся выше NBG1 (порядок слоёв задаёт C-мост). Модуль не
// включает Jo Engine/SH-2 — host-тестируемый через fake bridge.
#pragma once

#include "assets/assets.h"
#include "core/math/fixed.h"

#include <stdint.h>

namespace saturn {

/** Палитра фона: 7 зарезервирована (VDP1 CLUT — 1..6). */
constexpr uint8_t VDP2_BACKGROUND_PALETTE = 7u;

/** Размер NBG1 bitmap в VRAM (8bpp, 256 KiB). */
constexpr uint32_t VDP2_BITMAP_WIDTH = 1024u;
constexpr uint32_t VDP2_BITMAP_HEIGHT = 256u;

/** Логический экран Saturn. */
constexpr uint32_t VDP2_SCREEN_WIDTH = 320u;
constexpr uint32_t VDP2_SCREEN_HEIGHT = 224u;

/** Статистика VDP2-пути (детерминированная, для логов/тестов). */
struct Vdp2BackendStats
{
    uint32_t init_calls;       /**< Успешных init. */
    uint32_t uploads;          /**< Успешных заливок background. */
    uint32_t scroll_commits;   /**< VBlank-коммитов scroll. */
    uint32_t upload_failures;  /**< Отказов upload (размеры/состояние). */
    int16_t last_scroll_x;     /**< Последний применённый scroll X (px). */
    int16_t last_scroll_y;     /**< Последний применённый scroll Y (px). */
};

/** Инициализация NBG1 bitmap и порядка слоёв. */
bool vdp2_backend_init(void);

/** Деинициализация (идемпотентна). */
void vdp2_backend_shutdown(void);

/**
 * Заливка 256×256 (или кратной 8) текстуры в NBG1 bitmap тайлами.
 *
 * @param texture Валидированный TextureAsset.
 * @return true при успехе.
 */
bool vdp2_backend_upload_background(const TextureAsset* texture);

/**
 * Задать целевой scroll (fixed 16.16, пиксели). Применяется в VBlank.
 */
void vdp2_backend_set_scroll(fixed_t x, fixed_t y);

/** Применить pending scroll в VBlank (вызывается из vblank callback). */
void vdp2_backend_commit_vblank(void);

/** @return true после успешного init. */
bool vdp2_backend_available(void);

/** @return Накопленная статистика. */
const Vdp2BackendStats& vdp2_backend_stats(void);

/** Сброс статистики (init). */
void vdp2_backend_reset_stats(void);

}  // namespace saturn
