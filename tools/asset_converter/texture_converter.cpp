// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/asset_converter/texture_converter.cpp
//
// PNG/JPG/TGA/BMP → .tex v1: stb_image → RGBA8 → Median Cut (CI8 ≤255) →
// палитра RGB555+STP (bit layout §1.2.5: R[4:0]|G[9:5]|B[14:10]|STP[15]).
// Сериализация — побайтово LE (никаких struct→bytes кастов).
//
// Политика слота 0 (фиксируется в docs/ASSETS.md, неделя 19 — «entry 0 —
// политика резервирования — зафиксировать»): слот 0 зарезервирован под
// «transparent black» — запись {0,0,0,0}/слово 0x0000, CI8-индекс 0 никогда
// не входит в payload пикселей (квантование переходит на слоты 1..255);
// STP всех слотов = 0 (MVP-прозрачность выключена, §8.2).
#include "texture_converter.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "assets/assets.h"
#include "color_quantizer.h"
#include "converter_common.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {

/** Побайтовый append LE u32. */
void append_le_u32(std::vector<uint8_t>* out, uint32_t v)
{
    uint8_t tmp[4];
    assets_write_le_u32(tmp, v);
    out->insert(out->end(), tmp, tmp + 4u);
}

/** Побайтовый append LE u16. */
void append_le_u16(std::vector<uint8_t>* out, uint16_t v)
{
    uint8_t tmp[2];
    assets_write_le_u16(tmp, v);
    out->insert(out->end(), tmp, tmp + 2u);
}

/**
 * 8→5 бит канала: усечение старших бит (canonical PS1-конверсия; обратная
 * 5→8 парная деко — (c<<3)|(c>>2), §1.2.5 — круговой trip (c>>3) идемпотентен).
 */
uint8_t channel8_to_5(uint8_t c8)
{
    return (uint8_t)(c8 >> 3);
}

/** Пакет RGBA8-элемента палитры в u16 RGB555+STP (STP=0 — MVP). */
uint16_t pack_palette_word(const uint8_t entry[4])
{
    const uint8_t r5 = channel8_to_5(entry[0]);
    const uint8_t g5 = channel8_to_5(entry[1]);
    const uint8_t b5 = channel8_to_5(entry[2]);
    const bool stp = false; // MVP: прозрачность выключена (§8.2)
    return (uint16_t)((r5 & 0x1Fu) | ((g5 & 0x1Fu) << 5) | ((b5 & 0x1Fu) << 10) |
                      (stp ? 0x8000u : 0u));
}

} // namespace

bool convert_texture(const char* input_path, const char* output_path, uint32_t* out_error_code)
{
    uint32_t diag = CONVERT_OK;

    if (input_path == nullptr || output_path == nullptr) {
        diag = CONVERT_ERR_PARSE;
        if (out_error_code != nullptr) *out_error_code = diag;
        return false;
    }

    // 1) stb_image: форсируем RGBA8 (нормализованный вход квантователя).
    int width = 0;
    int height = 0;
    int original_channels = 0;
    uint8_t* image = stbi_load(input_path, &width, &height, &original_channels, 4 /* RGBA */);
    if (image == nullptr) {
        diag = CONVERT_ERR_OPEN_INPUT;
        if (out_error_code != nullptr) *out_error_code = diag;
        return false;
    }

    // 2) MVP §8.2: стороны ∈ {64,128,256}.
    const int allowed_dims[3] = {64, 128, 256};
    bool dims_ok = false;
    for (int d = 0; d < 3; ++d) {
        if (width == allowed_dims[d] && height == allowed_dims[d]) dims_ok = true;
    }
    if (!dims_ok) {
        stbi_image_free(image);
        diag = CONVERT_ERR_UNSUPPORTED;
        if (out_error_code != nullptr) *out_error_code = diag;
        return false;
    }

    // 3) Квантование RGBA8 → CI8 + палитра (слот 0 reserved).
    const uint32_t pixel_count = (uint32_t)width * (uint32_t)height;
    std::vector<uint8_t> indices((size_t)pixel_count, 0u);
    std::vector<uint8_t> palette((size_t)QUANTIZER_PALETTE_BYTES, 0u);
    uint8_t palette_used = 0u;
    if (!color_quantize_median_cut(image, pixel_count, indices.data(), palette.data(),
                                   &palette_used)) {
        stbi_image_free(image);
        diag = CONVERT_ERR_ALLOCATION;
        if (out_error_code != nullptr) *out_error_code = diag;
        return false;
    }

    // 4) Сбор файла: заголовок (20) + палитра (512 = 256×LE u16) + CI8
    //    (width×height). Смещения по §8.2: палитра сразу за заголовком.
    const uint32_t palette_offset = ASSETS_TEX_HEADER_BYTES;
    const uint32_t pixel_offset = ASSETS_TEX_HEADER_BYTES + ASSETS_TEX_PALETTE_BYTES;

    std::vector<uint8_t> file;
    file.reserve((size_t)pixel_offset + (size_t)pixel_count);
    append_le_u32(&file, ASSETS_TEX_MAGIC);
    append_le_u32(&file, ASSETS_TEX_VERSION);
    append_le_u16(&file, (uint16_t)width);
    append_le_u16(&file, (uint16_t)height);
    append_le_u32(&file, palette_offset);
    append_le_u32(&file, pixel_offset);

    for (uint32_t slot = 0u; slot < ASSETS_TEX_PALETTE_ENTRIES; ++slot) {
        const uint8_t* entry = palette.data() + (size_t)slot * 4u;
        append_le_u16(&file, pack_palette_word(entry));
    }
    file.insert(file.end(), indices.begin(), indices.end());

    stbi_image_free(image);

    // 5) Запись файла.
    FILE* out = fopen(output_path, "wb");
    if (out == nullptr) {
        diag = CONVERT_ERR_WRITE_OUTPUT;
        if (out_error_code != nullptr) *out_error_code = diag;
        return false;
    }
    const size_t written = fwrite(file.data(), 1u, file.size(), out);
    if (fclose(out) != 0 || written != file.size()) {
        diag = CONVERT_ERR_WRITE_OUTPUT;
        if (out_error_code != nullptr) *out_error_code = diag;
        return false;
    }

    if (out_error_code != nullptr) *out_error_code = CONVERT_OK;
    return true;
}
