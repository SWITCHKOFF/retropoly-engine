// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/asset_converter/texture_converter.h
//
// Конвертация PNG/JPG/TGA/BMP → .tex v1 (host-only; Спринт 9–10, неделя 19).
#pragma once

#include <stdint.h>

/**
 * Конвертация «изображение-файл → .tex v1» (CI8 + палитра RGB555+STP).
 *
 * Требования MVP §8.2: стороны изображения — 64/128/256; палитра — Median
 * Cut ≤256 цветов (цвет_quantize_median_cut); упаковка каналов — RGB555+STP:
 * R[4:0] | G[9:5] | B[14:10] | STP[15], все слоты STP=0 (MVP-прозрачность
 * выключена); слот 0 — reserved «transparent black» (0x0000), CI8-индекс 0
 * не входит в payload (см. color_quantizer.h).
 *
 * @param input_path Имя входного изображения (PNG/TGA/BMP/JPG через stb_image).
 * @param output_path Имя выходного .tex.
 * @param out_error_code Выход: код ConverterDiag (NULL допустим).
 * @return true при успехе; иначе false и записанный код.
 */
bool convert_texture(const char* input_path, const char* output_path, uint32_t* out_error_code);
