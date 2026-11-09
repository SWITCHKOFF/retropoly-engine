// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/asset_converter/color_quantizer.h
//
// Квантование RGBA8 → CI8 + палитра ≤256 цветов (host-only; Спринт 9–10,
// неделя 19). Median Cut; политика слота 0 — reserved (см. cpp).
#pragma once

#include <stdint.h>

#define QUANTIZER_PALETTE_BYTES (256u * 4u) /**< Палитра RGBA8: 256 × (r,g,b,a). */

/**
 * Квантование RGBA8-изображения в CI8 + палитру RGBA8.
 *
 * Политика (фиксируется ASSETS.md):
 *   - слот 0 RESERVED: никогда не выдаётся в out_indices и содержит
 *     нулевую запись {0,0,0,0} («transparent black» под будущую STP-политику);
 *   - квантуются до 255 цветов в слотах 1..255 (Median Cut);
 *   - при ≤255 уникальных цветах палитра точная (без потерь, кроме слота 0);
 *   - pixels цвета (0,0,0) — квантуются в ближайший ТЕМНЫЙ ненулевой entry
 *     (слот 0 всегда reserved).
 *
 * @param rgba Входные пиксели (4 байта/пиксель, pixel_count элементов).
 * @param pixel_count Число пикселей (> 0).
 * @param out_indices Выход: CI8-индексы (pixel_count байт; никогда не 0).
 * @param out_palette Выход: 256 × RGBA8 (байты; слот 0 — нулевой).
 * @param out_used Выход: число занятых слотов без зарезервированного (0..255).
 * @return true при успехе; false при отказе (bad input / host OOM).
 */
bool color_quantize_median_cut(const uint8_t* rgba, uint32_t pixel_count, uint8_t* out_indices,
                               uint8_t* out_palette, uint8_t* out_used);
