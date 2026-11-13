// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/assets/assets.h
//
// Бинарные ассеты .mesh v1 / .tex v1: форматы и безопасные загрузчики.
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md, §8.1–8.3 (.mesh/.tex
//     v1 — little-endian, байтово-определённые форматы без ABI-padding;
//     loader contract; валидация ДО аллокации), §1.2.7 (ASSET FORMAT
//     VALIDATION CONTRACT — обязательные проверки 1..7 в заданном порядке);
//   - docs/plans/sprint_09_10.md (недели 18 и 20).
//
// Загрузчики НИКОГДА не кастуют file.data к структурам: все поля читаются
// явным LE-декодированием из байтов. Каждая проверка арифметики счётчиков и
// смещений — в uint64_t ДО приведения к size_t. Выделение persistent-памяти
// происходит только после успешной валидации; любая пост-аллокационная
// ошибка откатывает арену к marker'у (used() не изменяется) — malformed
// вход расходует persistent-память ровно на 0 байт.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <retro_engine/core/math/fixed.h>
#include <retro_engine/core/memory/linear_allocator.h>
#include <retro_engine/hal/platform.h> /* Vertex, Triangle */

// ---------------------------------------------------------------------------
// Формат .mesh v1 (§8.1): [Header:24][Vertex*21 × vertex_count][Index*6 × index_count]
// ---------------------------------------------------------------------------

/** Размер заголовка .mesh v1 (байт). */
#define ASSETS_MESH_HEADER_BYTES 24u
/** Размер одной вершинной записи .mesh v1 (байт). */
#define ASSETS_MESH_VERTEX_BYTES 21u
/** Размер одной индексной записи .mesh v1 (шесть u16: v0,v1,v2 = треугольник). */
#define ASSETS_MESH_INDEX_BYTES 6u
/** Magic .mesh v1: байты «M» «E» «S» «H» как LE u32. */
#define ASSETS_MESH_MAGIC UINT32_C(0x4853454D)
/** Версия формата .mesh. */
#define ASSETS_MESH_VERSION UINT32_C(1)

// ---------------------------------------------------------------------------
// Формат .tex v1 (§8.2): [Header:20][payload: палитра 512][payload: CI8 w*h]
// ---------------------------------------------------------------------------

/** Размер заголовка .tex v1 (байт). */
#define ASSETS_TEX_HEADER_BYTES 20u
/** Число записей палитры CI8-текстуры (полная CLUT размера 1). */
#define ASSETS_TEX_PALETTE_ENTRIES 256u
/** Размер payload'а палитры: 256 × LE u16 RGB555+STP (байт). */
#define ASSETS_TEX_PALETTE_BYTES 512u
/** Magic .tex v1: байты «T» «E» «X» «\0» как LE u32. */
#define ASSETS_TEX_MAGIC UINT32_C(0x00584554)
/** Версия формата .tex. */
#define ASSETS_TEX_VERSION UINT32_C(1)

/**
 * Валидная сторона текстуры .tex v1 (MVP: 64/128/256 — §8.2).
 *
 * @param dim Сторона (width или height).
 * @return true, если значение допустимо.
 */
static inline int assets_tex_dim_valid(uint32_t dim)
{
    return dim == 64u || dim == 128u || dim == 256u;
}

// ---------------------------------------------------------------------------
// Декодированные заголовки (host-структуры; layout НЕ сериализуется)
// ---------------------------------------------------------------------------

/** Декодированный заголовок .mesh v1 (поля — хостовый порядок байт). */
typedef struct MeshHeader
{
    uint32_t magic;                 /**< 0x4853454D. */
    uint32_t version;               /**< 1. */
    uint32_t vertex_count;          /**< Число 21-байтовых вершинных записей. */
    uint32_t index_count;           /**< Число 6-байтовых индексных записей. */
    uint32_t texture_id;            /**< Идентификатор текстуры движка (слот рантайма). */
    int32_t bounding_sphere_radius; /**< Радиус ограничивающей сферы, signed 16.16. */
} MeshHeader;

/** Декодированный заголовок .tex v1 (поля — хостовый порядок байт). */
typedef struct TexHeader
{
    uint32_t magic;           /**< 0x00584554. */
    uint32_t version;         /**< 1. */
    uint16_t width;           /**< Сторона: 64/128/256 для MVP. */
    uint16_t height;          /**< Сторона: 64/128/256 для MVP. */
    uint32_t palette_offset;  /**< Смещение payload'а палитры (512 байт). */
    uint32_t pixel_offset;    /**< Смещение payload'а CI8 (width×height байт). */
} TexHeader;

/** Результат загрузчика .mesh: разобранный меш, память — в арене @p alloc. */
typedef struct MeshAsset
{
    const Vertex* vertices;  /**< Разобранные вершины (vertex_count записей). */
    const Triangle* indices; /**< Разобранные индексные записи (index_count). */
    uint32_t vertex_count;   /**< Число вершин. */
    uint32_t index_count;    /**< Число треугольников (записей по 6 байт). */
    uint32_t texture_id;     /**< Указатель на слот текстуры рантайма (из заголовка). */
    fixed_t bounding_sphere_radius; /**< Радиус сферы (fixed 16.16, из заголовка). */
} MeshAsset;

/** Результат загрузчика .tex: разобранная CI8-текстура, память — в арене. */
typedef struct TextureAsset
{
    const uint8_t* pixels;   /**< CI8-индексы, width×height байт. */
    const uint16_t* palette; /**< Палитра: 256 × u16 RGB555+STP. */
    uint32_t width;          /**< Ширина. */
    uint32_t height;         /**< Высота. */
} TextureAsset;

/**
 * Детерминированные диагностические коды загрузчиков. Порядок — контракт;
 * расширение — только перед ASSET_ERR_COUNT.
 */
typedef enum
{
    ASSET_ERR_OK = 0,             /**< Успех. */
    ASSET_ERR_NULL_ARGUMENT,      /**< data/alloc/out == NULL. */
    ASSET_ERR_TRUNCATED_HEADER,   /**< Проверка (1): file_size < header_size. */
    ASSET_ERR_BAD_MAGIC,          /**< Проверка (2): magic не соответствует формату. */
    ASSET_ERR_BAD_VERSION,        /**< Проверка (2): version != 1. */
    ASSET_ERR_BAD_COUNTS, /**< Счётчики .mesh нулевые/не кратны 3; длина .tex вне {64,128,256}. */
    ASSET_ERR_ARITHMETIC, /**< Проверка (3): переполнение арифметики u64. */
    ASSET_ERR_PAYLOAD_RANGE, /**< Проверка (4): payload вне файла/перекрытие. */
    ASSET_ERR_INDEX_RANGE, /**< Проверка (5): индекс ≥ vertex_count. */
    ASSET_ERR_ALLOC_FAIL, /**< Проверка (6): арена исчерпана (после rollback). */
    ASSET_ERR_COUNT
} AssetDiag;

// ---------------------------------------------------------------------------
// Явное little-endian кодирование/декодирование (байтовый контракт форматов).
// Функции НЕ опираются на порядок байт хоста: хост может быть как LE, так
// и BE; сериализация всегда побайтовая.
// ---------------------------------------------------------------------------

/**
 * Прочитано little-endian u16 из байтов.
 *
 * @param bytes Указатель на ≥ 2 байта.
 * @return Декодированное значение.
 */
static inline uint16_t assets_read_le_u16(const uint8_t* bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

/**
 * Прочитано little-endian u32 из байтов.
 *
 * @param bytes Указатель на ≥ 4 байта.
 * @return Декодированное значение.
 */
static inline uint32_t assets_read_le_u32(const uint8_t* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

/**
 * Запись little-endian u16 поадресно (побайтово; никаких struct-cast'ов).
 *
 * @param dst Выход (≥ 2 байта).
 * @param v Значение.
 */
static inline void assets_write_le_u16(uint8_t* dst, uint16_t v)
{
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
}

/**
 * Запись little-endian u32 поадресно (побайтово; никаких struct-cast'ов).
 *
 * @param dst Выход (≥ 4 байта).
 * @param v Значение.
 */
static inline void assets_write_le_u32(uint8_t* dst, uint32_t v)
{
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
    dst[2] = (uint8_t)((v >> 16) & 0xFFu);
    dst[3] = (uint8_t)((v >> 24) & 0xFFu);
}

// ---------------------------------------------------------------------------
// Загрузчики (реализации — src/assets/mesh_loader.cpp, texture_loader.cpp)
// ---------------------------------------------------------------------------

/**
 * Валидация и загрузка .mesh v1 (§8.3, порядок §1.2.7 — 7 шагов).
 *
 * Проверки ДО выделения памяти (нормативный порядок):
 *   (1) file_size ≥ header_size; (2) magic/version; затем sanity счётчиков
 *   (> 0, index_count % 3 == 0); (3) арифметика count/offset в uint64_t;
 *   (4) payload_end ≤ file_size; (5) каждый индекс < vertex_count.
 * Только затем — (6) persistent allocation (MeshAsset + массивы) и
 * (7) rollback арены при любой пост-аллокационной ошибке.
 *
 * @param data Байты файла (не NULL; время жизни ≥ вызова).
 * @param file_size Размер в байтах.
 * @param alloc Арена для результата (не NULL).
 * @param out Выход: указатель на разобранный меш (только при успехе).
 * @param out_error_code Выход: код AssetDiag (NULL допустим; всегда записывается).
 * @return true при успехе (арена выросла на объём результата); false при
 *         любой ошибке (содержимое арены НЕ изменяется — rollback).
 */
bool mesh_validate_and_load(const uint8_t* data, size_t file_size, LinearAllocator* alloc,
                            MeshAsset** out, uint32_t* out_error_code);

/**
 * Валидация и загрузка .tex v1 (симметрично mesh_validate_and_load).
 *
 * Дополнительно к шагам (1)–(7): width/height обязаны быть ∈ {64,128,256};
 * payload палитры — ровно 512 байт; payload пикселей — ровно width×height;
 * payload'ы не выходят за file_size, не пересекают заголовок и друг друга.
 * Палитра/пиксели копируются в арену (файл может быть освобождён после).
 *
 * @param data Байты файла (не NULL).
 * @param file_size Размер в байтах.
 * @param alloc Арена для результата (не NULL).
 * @param out Выход: указатель на разобранную текстуру (только при успехе).
 * @param out_error_code Выход: код AssetDiag (NULL допустим).
 * @return true при успехе; false при ошибке (арена не изменяется — rollback).
 */
bool texture_validate_and_load(const uint8_t* data, size_t file_size, LinearAllocator* alloc,
                               TextureAsset** out, uint32_t* out_error_code);
