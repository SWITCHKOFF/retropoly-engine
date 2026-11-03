// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/assets/mesh_loader.cpp
//
// Безопасный загрузчик .mesh v1. Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §8.1 (формат),
//     §8.3 (loader contract), §1.2.7 (обязательные проверки 1..7 в порядке);
//   - docs/plans/sprint_09_10.md, неделя 20.
//
// Инварианты:
//   - все поля читаются явным LE-декодированием (никаких кастов file.data →
//     Header/Vertex/struct);
//   - арифметика счётчиков и смещений — в uint64_t; приведение к size_t —
//     только после проверки границы payload'а;
//   - persistent allocation — только после валидации (шаг 6); откат к
//     marker'у при любой пост-аллокационной ошибке (шаг 7): failed load
//     расходует persistent-память арены ровно на 0 байт.
#include "assets/assets.h"

// ---------------------------------------------------------------------------
// Выделение из арены и декодеры записей (внутренние детали загрузчика)
// ---------------------------------------------------------------------------

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
 * Декодирование заголовка .mesh из байтов (24 байта, §8.1).
 * Вызов разрешён только когда file_size ≥ ASSETS_MESH_HEADER_BYTES.
 *
 * @param bytes Байты файла.
 * @param h Выход: декодированный заголовок.
 */
void assets_decode_mesh_header(const uint8_t* bytes, MeshHeader* h)
{
    h->magic = assets_read_le_u32(bytes + 0u);
    h->version = assets_read_le_u32(bytes + 4u);
    h->vertex_count = assets_read_le_u32(bytes + 8u);
    h->index_count = assets_read_le_u32(bytes + 12u);
    h->texture_id = assets_read_le_u32(bytes + 16u);
    h->bounding_sphere_radius = (int32_t)assets_read_le_u32(bytes + 20u);
}

/**
 * Декодирование одной 21-байтной вершины .mesh (§8.1) в Vertex движка.
 * Раскладка байт: i32 x,y,z (16.16); i8 nx,ny,nz; u8 u,v; u8 r,g,b,a.
 *
 * @param bytes Указатель на 21 байт вершины.
 * @param v Выход: вершина движка.
 */
void assets_decode_vertex(const uint8_t* bytes, Vertex* v)
{
    v->x = (fixed_t)assets_read_le_u32(bytes + 0u);
    v->y = (fixed_t)assets_read_le_u32(bytes + 4u);
    v->z = (fixed_t)assets_read_le_u32(bytes + 8u);
    v->nx = (int8_t)bytes[12];
    v->ny = (int8_t)bytes[13];
    v->nz = (int8_t)bytes[14];
    v->u = bytes[15];
    v->v = bytes[16];
    v->r = bytes[17];
    v->g = bytes[18];
    v->b = bytes[19];
    v->a = bytes[20];
}

/**
 * Декодирование одной 6-байтной индексной записи .mesh в Triangle движка.
 *
 * @param bytes Указатель на 6 байт записи.
 * @param t Выход: треугольник движка.
 */
void assets_decode_index(const uint8_t* bytes, Triangle* t)
{
    t->v0 = assets_read_le_u16(bytes + 0u);
    t->v1 = assets_read_le_u16(bytes + 2u);
    t->v2 = assets_read_le_u16(bytes + 4u);
}

} // namespace

bool mesh_validate_and_load(const uint8_t* data, size_t file_size, LinearAllocator* alloc,
                            MeshAsset** out, uint32_t* out_error_code)
{
    uint32_t diag = ASSET_ERR_OK;

    if (data == (const uint8_t*)nullptr || alloc == (LinearAllocator*)nullptr ||
        out == (MeshAsset**)nullptr) {
        diag = ASSET_ERR_NULL_ARGUMENT;
        goto finish_fail;
    }

    // (1) file_size ≥ header_size.
    if (file_size < ASSETS_MESH_HEADER_BYTES) {
        diag = ASSET_ERR_TRUNCATED_HEADER;
        goto finish_fail;
    }

    // Заголовок — побайтовой LE-декодировкой из data (никаких struct-кастов).
    {
        MeshHeader h;
        assets_decode_mesh_header(data, &h);

        // (2) magic/version.
        if (h.magic != ASSETS_MESH_MAGIC) {
            diag = ASSET_ERR_BAD_MAGIC;
            goto finish_fail;
        }
        if (h.version != ASSETS_MESH_VERSION) {
            diag = ASSET_ERR_BAD_VERSION;
            goto finish_fail;
        }

        // Sanity счётчиков (между шагами 2 и 3, как в §8.3): непустые записи;
        // каждая индексная запись несёт ровно 3 индекса — норматив «кратно 3».
        if (h.vertex_count == 0u || h.index_count == 0u || (h.index_count % 3u) != 0u) {
            diag = ASSET_ERR_BAD_COUNTS;
            goto finish_fail;
        }

        // (3) Арифметика счётчиков/смещений — в uint64_t.
        const uint64_t vertex_bytes = (uint64_t)h.vertex_count * ASSETS_MESH_VERTEX_BYTES;
        const uint64_t index_bytes = (uint64_t)h.index_count * ASSETS_MESH_INDEX_BYTES;
        const uint64_t payload_end =
            (uint64_t)ASSETS_MESH_HEADER_BYTES + vertex_bytes + index_bytes;
        if ((vertex_bytes / ASSETS_MESH_VERTEX_BYTES) != (uint64_t)h.vertex_count ||
            (index_bytes / ASSETS_MESH_INDEX_BYTES) != (uint64_t)h.index_count ||
            payload_end < (uint64_t)ASSETS_MESH_HEADER_BYTES || payload_end < vertex_bytes ||
            payload_end < index_bytes) {
            diag = ASSET_ERR_ARITHMETIC;
            goto finish_fail;
        }

        // (4) payload целиком внутри файла.
        if (payload_end > (uint64_t)file_size) {
            diag = ASSET_ERR_PAYLOAD_RANGE;
            goto finish_fail;
        }

        // (5) Каждый индекс < vertex_count; граница каждой записи проверяется
        // в uint64_t на каждой итерации (§8.3 — дословно).
        {
            const uint64_t index_base = (uint64_t)ASSETS_MESH_HEADER_BYTES + vertex_bytes;
            for (uint32_t i = 0u; i < h.index_count; ++i) {
                const uint64_t offset_u64 = index_base + (uint64_t)i * ASSETS_MESH_INDEX_BYTES;
                if (offset_u64 > (uint64_t)file_size ||
                    ASSETS_MESH_INDEX_BYTES > (uint64_t)file_size - offset_u64) {
                    diag = ASSET_ERR_PAYLOAD_RANGE;
                    goto finish_fail;
                }
                const uint8_t* record = data + (size_t)offset_u64;
                if (assets_read_le_u16(record + 0u) >= h.vertex_count ||
                    assets_read_le_u16(record + 2u) >= h.vertex_count ||
                    assets_read_le_u16(record + 4u) >= h.vertex_count) {
                    diag = ASSET_ERR_INDEX_RANGE;
                    goto finish_fail;
                }
            }
        }
    }

    // (6) Только теперь persistent allocation; (7) rollback при отказе.
    {
        // Декодированный заголовок не переходит через goto-границу — берём
        // поля повторно (все проверки выше уже пройдены).
        MeshHeader h;
        assets_decode_mesh_header(data, &h);

        const LinearAllocator::Marker mark = alloc->mark();

        MeshAsset* mesh = assets_alloc_object<MeshAsset>(alloc);
        Vertex* vertices = (mesh != (MeshAsset*)nullptr)
                               ? assets_alloc_array<Vertex>(alloc, h.vertex_count)
                               : (Vertex*)nullptr;
        Triangle* indices = (vertices != (Vertex*)nullptr)
                                ? assets_alloc_array<Triangle>(alloc, h.index_count)
                                : (Triangle*)nullptr;

        if (mesh == (MeshAsset*)nullptr || vertices == (Vertex*)nullptr ||
            indices == (Triangle*)nullptr) {
            (void)alloc->rollback(mark); // (7): failed load не расходует арену.
            diag = ASSET_ERR_ALLOC_FAIL;
            goto finish_fail;
        }

        for (uint32_t i = 0u; i < h.vertex_count; ++i) {
            assets_decode_vertex(
                data + ASSETS_MESH_HEADER_BYTES + (size_t)i * ASSETS_MESH_VERTEX_BYTES,
                vertices + i);
        }
        for (uint32_t i = 0u; i < h.index_count; ++i) {
            const uint8_t* record = data + ASSETS_MESH_HEADER_BYTES +
                                    (size_t)h.vertex_count * ASSETS_MESH_VERTEX_BYTES +
                                    (size_t)i * ASSETS_MESH_INDEX_BYTES;
            assets_decode_index(record, indices + i);
        }

        mesh->vertices = vertices;
        mesh->indices = indices;
        mesh->vertex_count = h.vertex_count;
        mesh->index_count = h.index_count;
        mesh->texture_id = h.texture_id;
        mesh->bounding_sphere_radius = h.bounding_sphere_radius;

        *out = mesh;
        if (out_error_code != (uint32_t*)nullptr) *out_error_code = ASSET_ERR_OK;
        return true;
    }

finish_fail:
    if (out_error_code != (uint32_t*)nullptr) *out_error_code = diag;
    return false;
}
