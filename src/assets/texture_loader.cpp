// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/assets/texture_loader.cpp
//
// Безопасный загрузчик .tex v1 (CI8 + палитра RGB555+STP).
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §8.2 (формат),
//     §1.2.7 (обязательные проверки 1..7 в порядке), §1.2.5 (RGB555+STP);
//   - docs/plans/sprint_09_10.md, неделя 20.
//
// Инварианты (симметрично mesh_loader.cpp):
//   - все поля читаются явным LE-декодированием;
//   - width*height и offset/end-арифметика — в uint64_t;
//   - payload палитры — ровно 512 байт, payload CI8 — ровно width×height;
//   - payload'ы целиком в файле, не пересекают заголовок и друг друга;
//   - allocation/копирование только после валидации; откат к marker'у при
//     любой пост-аллокационной ошибке: failed load расходует арену на 0 байт.
#include "assets/assets.h"

namespace {

/** Выделение массива @p count элементов типа T в арене (nullptr при отказе). */
template <typename T>
T* assets_alloc_array(LinearAllocator* alloc, uint32_t count)
{
    const uint64_t bytes = (uint64_t)count * (uint64_t)sizeof(T);
    if (count == 0u || bytes > (uint64_t)SIZE_MAX) return (T*)nullptr;
    return static_cast<T*>(alloc->allocate((size_t)bytes, alignof(T)));
}

/** Выделение одного объекта типа T в арене (nullptr при отказе). */
template <typename T>
T* assets_alloc_object(LinearAllocator* alloc)
{
    return static_cast<T*>(alloc->allocate(sizeof(T), alignof(T)));
}

/**
 * Декодирование заголовка .tex из байтов (20 байт, §8.2).
 * Вызов разрешён только когда file_size ≥ ASSETS_TEX_HEADER_BYTES.
 *
 * @param bytes Байты файла.
 * @param h Выход: декодированный заголовок.
 */
void assets_decode_tex_header(const uint8_t* bytes, TexHeader* h)
{
    h->magic = assets_read_le_u32(bytes + 0u);
    h->version = assets_read_le_u32(bytes + 4u);
    h->width = assets_read_le_u16(bytes + 8u);
    h->height = assets_read_le_u16(bytes + 10u);
    h->palette_offset = assets_read_le_u32(bytes + 12u);
    h->pixel_offset = assets_read_le_u32(bytes + 16u);
}

} // namespace

bool texture_validate_and_load(const uint8_t* data, size_t file_size, LinearAllocator* alloc,
                               TextureAsset** out, uint32_t* out_error_code)
{
    uint32_t diag = ASSET_ERR_OK;

    if (data == (const uint8_t*)nullptr || alloc == (LinearAllocator*)nullptr ||
        out == (TextureAsset**)nullptr) {
        diag = ASSET_ERR_NULL_ARGUMENT;
        goto finish_fail;
    }

    // (1) file_size ≥ header_size.
    if (file_size < ASSETS_TEX_HEADER_BYTES) {
        diag = ASSET_ERR_TRUNCATED_HEADER;
        goto finish_fail;
    }

    // Заголовок — побайтовой LE-декодировкой из data (никаких struct-кастов).
    {
        TexHeader h;
        assets_decode_tex_header(data, &h);

        // (2) magic/version.
        if (h.magic != ASSETS_TEX_MAGIC) {
            diag = ASSET_ERR_BAD_MAGIC;
            goto finish_fail;
        }
        if (h.version != ASSETS_TEX_VERSION) {
            diag = ASSET_ERR_BAD_VERSION;
            goto finish_fail;
        }

        // Sanity размеров (до арифметики): стороны ∈ {64,128,256} (§8.2 MVP);
        // нулевые/чужие значения отсекаются этим же фильтром.
        if (!assets_tex_dim_valid((uint32_t)h.width) || !assets_tex_dim_valid((uint32_t)h.height)) {
            diag = ASSET_ERR_BAD_COUNTS;
            goto finish_fail;
        }

        // (3) Арифметика площадей/смещений — в uint64_t (§8.2).
        const uint64_t pixel_bytes = (uint64_t)h.width * (uint64_t)h.height;
        const uint64_t palette_end = (uint64_t)h.palette_offset + ASSETS_TEX_PALETTE_BYTES;
        const uint64_t pixel_end = (uint64_t)h.pixel_offset + pixel_bytes;
        if ((palette_end - ASSETS_TEX_PALETTE_BYTES) != (uint64_t)h.palette_offset ||
            (pixel_end - pixel_bytes) != (uint64_t)h.pixel_offset) {
            diag = ASSET_ERR_ARITHMETIC;
            goto finish_fail;
        }

        // (4) Payload'ы целиком в файле; не пересекают заголовок; палитра и
        // пиксели не перекрывают друг друга (§8.2 «must not overlap»).
        if ((uint64_t)h.palette_offset < ASSETS_TEX_HEADER_BYTES ||
            (uint64_t)h.pixel_offset < ASSETS_TEX_HEADER_BYTES ||
            palette_end > (uint64_t)file_size || pixel_end > (uint64_t)file_size) {
            diag = ASSET_ERR_PAYLOAD_RANGE;
            goto finish_fail;
        }
        if (!(palette_end <= (uint64_t)h.pixel_offset || pixel_end <= (uint64_t)h.palette_offset)) {
            diag = ASSET_ERR_PAYLOAD_RANGE;
            goto finish_fail;
        }

        // (5) Проверка «каждый индекс» для .tex не требуется (CI8-индекс —
        // полный байт 0..255); разделения payload'ов уже гарантированы.
    }

    // (6) Только теперь persistent allocation; (7) rollback при отказе.
    {
        // Декодированный заголовок не переходит через goto-границу — берём
        // поля повторно (все проверки выше уже пройдены).
        TexHeader h;
        assets_decode_tex_header(data, &h);

        const LinearAllocator::Marker mark = alloc->mark();

        TextureAsset* texture = assets_alloc_object<TextureAsset>(alloc);
        uint8_t* pixels =
            (texture != (TextureAsset*)nullptr)
                ? assets_alloc_array<uint8_t>(alloc, (uint32_t)h.width * (uint32_t)h.height)
                : (uint8_t*)nullptr;
        uint16_t* palette = (pixels != (uint8_t*)nullptr)
                                ? assets_alloc_array<uint16_t>(alloc, ASSETS_TEX_PALETTE_ENTRIES)
                                : (uint16_t*)nullptr;

        if (texture == (TextureAsset*)nullptr || pixels == (uint8_t*)nullptr ||
            palette == (uint16_t*)nullptr) {
            (void)alloc->rollback(mark); // (7): failed load не расходует арену.
            diag = ASSET_ERR_ALLOC_FAIL;
            goto finish_fail;
        }

        // payload'ы валидны: копирование из файла в арену с LE-декодированием.
        for (uint32_t i = 0u; i < h.width * h.height; ++i) {
            pixels[i] = data[(size_t)h.pixel_offset + i];
        }
        for (uint32_t i = 0u; i < ASSETS_TEX_PALETTE_ENTRIES; ++i) {
            palette[i] = assets_read_le_u16(data + h.palette_offset + (size_t)i * 2u);
        }

        texture->pixels = pixels;
        texture->palette = palette;
        texture->width = h.width;
        texture->height = h.height;

        *out = texture;
        if (out_error_code != (uint32_t*)nullptr) *out_error_code = ASSET_ERR_OK;
        return true;
    }

finish_fail:
    if (out_error_code != (uint32_t*)nullptr) *out_error_code = diag;
    return false;
}
