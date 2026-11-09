// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/asset_converter/mesh_converter.cpp
//
// Конвертация glTF → .mesh v1. Хост-инструмент: float/STL разрешены
// (STYLE_GUIDE §3 — точки допуска float: host-only инструменты);
// runtime-ограничения движка (new/malloc/printf) на tools/ не действуют.
//
// Контракт сериализации (normativ §8.1): little-endian ПОБАЙТОВО — никаких
// кастов struct→bytes; ABI-padding хоста не сериализуется. Конверсии:
//   - позиции → fixed 16.16 строго через fixed_from_float (контракт ядра);
//   - нормали → i8 с сатурацией ±127;
//   - UV и цвет → u8 округлением к ближайшему (детерминированно).
#include "mesh_converter.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "assets/assets.h"
#include "converter_common.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

// ---------------------------------------------------------------------------
// Внутренние утилиты конвертации
// ---------------------------------------------------------------------------

namespace {

/** Побайтовый append LE u32 (сериализация §8.1 без кастов struct→bytes). */
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
 * Округление к ближайшему f∈[0..1] → u8; NaN/отрицательное → 0; >1 → 255.
 * Одна операция на строке; детерминированный half-up.
 */
uint8_t convert_unit_to_u8(float value)
{
    if (!(value == value) || value < 0.0f) return 0u;
    if (value > 1.0f) return 255u;
    return (uint8_t)(value * 255.0f + 0.5f);
}

/**
 * Преобразование позиции VEC3 world-матрицей узла (column-major glTF),
 * вычисления в double для детерминированного host-результата.
 */
void transform_position(const cgltf_node* node, const float position[3], double out[3])
{
    cgltf_float matrix[16];
    cgltf_node_transform_world(node, matrix);
    const double x = (double)position[0];
    const double y = (double)position[1];
    const double z = (double)position[2];
    out[0] =
        (double)matrix[0] * x + (double)matrix[4] * y + (double)matrix[8] * z + (double)matrix[12];
    out[1] =
        (double)matrix[1] * x + (double)matrix[5] * y + (double)matrix[9] * z + (double)matrix[13];
    out[2] =
        (double)matrix[2] * x + (double)matrix[6] * y + (double)matrix[10] * z + (double)matrix[14];
}

/** Верхняя 3×3 world-матрицы (направления нормалей; затем нормализуем). */
void transform_direction(const cgltf_node* node, const double dir_in[3], double out[3])
{
    cgltf_float matrix[16];
    cgltf_node_transform_world(node, matrix);
    const double x = dir_in[0];
    const double y = dir_in[1];
    const double z = dir_in[2];
    out[0] = (double)matrix[0] * x + (double)matrix[4] * y + (double)matrix[8] * z;
    out[1] = (double)matrix[1] * x + (double)matrix[5] * y + (double)matrix[9] * z;
    out[2] = (double)matrix[2] * x + (double)matrix[6] * y + (double)matrix[10] * z;
}

/** Нормализация; нулевые векторы остаются нулевыми (без NaN). */
void normalize_double(double v[3])
{
    const double len2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (len2 <= 0.0) return;
    const double inv_len = 1.0 / sqrt(len2);
    v[0] *= inv_len;
    v[1] *= inv_len;
    v[2] *= inv_len;
}

/**
 * double-нормаль → i8 с сатурацией ±127 (contract §8.1: диапазон −127..127).
 * Округление к ближайшему; NaN → 0.
 */
void pack_normal_i8(const double normal[3], int8_t out[3])
{
    for (uint32_t i = 0; i < 3u; ++i) {
        double v = normal[i];
        if (!(v == v)) v = 0.0;
        double scaled = v * 127.0;
        if (scaled >= 0.0)
            scaled += 0.5;
        else
            scaled -= 0.5;
        int32_t quantized = (int32_t)scaled;
        if (quantized > 127) quantized = 127;
        if (quantized < -127) quantized = -127;
        out[i] = (int8_t)quantized;
    }
}

/** Запись одной 21-байтной вершины в буфер (§8.1, побайтово). */
void append_vertex_record(std::vector<uint8_t>* out, const Vertex* v)
{
    uint8_t record[21];
    assets_write_le_u32(record + 0u, (uint32_t)v->x);
    assets_write_le_u32(record + 4u, (uint32_t)v->y);
    assets_write_le_u32(record + 8u, (uint32_t)v->z);
    record[12] = (uint8_t)v->nx;
    record[13] = (uint8_t)v->ny;
    record[14] = (uint8_t)v->nz;
    record[15] = v->u;
    record[16] = v->v;
    record[17] = v->r;
    record[18] = v->g;
    record[19] = v->b;
    record[20] = v->a;
    out->insert(out->end(), record, record + sizeof(record));
}

/** Запись одной 6-байтной индексной записи (3 × LE u16). */
void append_index_record(std::vector<uint8_t>* out, uint16_t v0, uint16_t v1, uint16_t v2)
{
    append_le_u16(out, v0);
    append_le_u16(out, v1);
    append_le_u16(out, v2);
}

/** Чтение позиции vertex-света (0,0,0 при отказе чтения — без sparse). */
bool read_vec3(const cgltf_accessor* accessor, cgltf_size index, float out[3])
{
    if (accessor == nullptr || accessor->type != cgltf_type_vec3) return false;
    return cgltf_accessor_read_float(accessor, index, out, 3u) ? true : false;
}

/**
 * Плоские нормали (фоллбек при отсутствии NORMAL): по каждому треугольнику
 * примитива грани нормаль переписывается всем трём вершинам (flat shading).
 * Работает по УЖЕ ЗАПИСАННЫМ fixed-позициям payload'а (детерминично).
 *
 * @param buffer Буфер вершинных записей (21 байт/запись).
 * @param vertex_base Смещение первой вершины примитива.
 * @param primer_raws Сырые индексы треугольников примитива (кратно 3).
 */
void assign_flat_normals(std::vector<uint8_t>* buffer, uint32_t vertex_base,
                         const std::vector<uint32_t>& primer_raws)
{
    for (size_t t = 0; t + 2u < primer_raws.size(); t += 3u) {
        double a[3] = {0.0, 0.0, 0.0};
        double b[3] = {0.0, 0.0, 0.0};
        double c[3] = {0.0, 0.0, 0.0};
        const uint32_t slot[3] = {primer_raws[t + 0u], primer_raws[t + 1u], primer_raws[t + 2u]};
        const uint8_t* rec[3] = {
            buffer->data() + (size_t)(vertex_base + slot[0]) * ASSETS_MESH_VERTEX_BYTES,
            buffer->data() + (size_t)(vertex_base + slot[1]) * ASSETS_MESH_VERTEX_BYTES,
            buffer->data() + (size_t)(vertex_base + slot[2]) * ASSETS_MESH_VERTEX_BYTES,
        };
        for (uint32_t i = 0; i < 3u; ++i) {
            a[i] = (double)assets_read_le_u32(rec[0] + i * 4u) / 65536.0;
            b[i] = (double)assets_read_le_u32(rec[1] + i * 4u) / 65536.0;
            c[i] = (double)assets_read_le_u32(rec[2] + i * 4u) / 65536.0;
        }
        const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        double n[3];
        n[0] = e1[1] * e2[2] - e1[2] * e2[1];
        n[1] = e1[2] * e2[0] - e1[0] * e2[2];
        n[2] = e1[0] * e2[1] - e1[1] * e2[0];
        normalize_double(n);
        int8_t packed[3];
        pack_normal_i8(n, packed);
        for (uint32_t vi = 0; vi < 3u; ++vi) {
            uint8_t* dest =
                buffer->data() + (size_t)(vertex_base + slot[vi]) * ASSETS_MESH_VERTEX_BYTES + 12u;
            dest[0] = (uint8_t)packed[0];
            dest[1] = (uint8_t)packed[1];
            dest[2] = (uint8_t)packed[2];
        }
    }
}

} // namespace

bool convert_mesh(const char* input_path, const char* output_path, uint32_t* out_error_code)
{
    uint32_t diag = CONVERT_OK;
    cgltf_data* gltf = nullptr;

    if (input_path == nullptr || output_path == nullptr) {
        diag = CONVERT_ERR_PARSE;
        if (out_error_code != nullptr) *out_error_code = diag;
        return false;
    }

    {
        cgltf_options options;
        memset(&options, 0, sizeof(options));
        if (cgltf_parse_file(&options, input_path, &gltf) != cgltf_result_success) {
            diag = CONVERT_ERR_OPEN_INPUT;
            goto finish;
        }
        if (cgltf_load_buffers(&options, gltf, input_path) != cgltf_result_success) {
            diag = CONVERT_ERR_OPEN_INPUT;
            goto finish;
        }
        if (cgltf_validate(gltf) != cgltf_result_success) {
            diag = CONVERT_ERR_PARSE;
            goto finish;
        }
        if (gltf->scene == nullptr || gltf->scene->nodes_count == 0u) {
            diag = CONVERT_ERR_PARSE;
            goto finish;
        }

        // --- Сбор payload'а: обход меш-узлов сцены ------------------------------
        std::vector<uint8_t> payload;      // вершинные записи (21 Б × vertex_count)
        std::vector<uint32_t> index_stream; // по 3 сырых индекса на треугольник
        uint32_t vertex_total = 0u;

        std::vector<const cgltf_node*> stack;
        for (cgltf_size root = 0; root < gltf->scene->nodes_count; ++root) {
            stack.push_back(gltf->scene->nodes[root]);
        }
        while (!stack.empty()) {
            const cgltf_node* node = stack.back();
            stack.pop_back();
            for (cgltf_size child = 0; child < node->children_count; ++child) {
                stack.push_back(node->children[child]);
            }
            if (node->mesh == nullptr) continue;

            for (cgltf_size p = 0; p < node->mesh->primitives_count; ++p) {
                const cgltf_primitive* primitive = &node->mesh->primitives[p];

                // MVP §8.1: только индексированные треугольники.
                if (primitive->type != cgltf_primitive_type_triangles) {
                    diag = CONVERT_ERR_UNSUPPORTED;
                    goto finish;
                }

                const cgltf_accessor* positions =
                    cgltf_find_accessor(primitive, cgltf_attribute_type_position, 0);
                if (positions == nullptr || positions->type != cgltf_type_vec3 ||
                    positions->count == 0u) {
                    diag = CONVERT_ERR_PARSE;
                    goto finish;
                }
                const uint32_t pv_count = (uint32_t)positions->count;

                // u16-индексация v1: не более 65535 вершин суммарно на файл.
                if ((uint64_t)vertex_total + pv_count > UINT32_C(65535)) {
                    diag = CONVERT_ERR_UNSUPPORTED;
                    goto finish;
                }

                const cgltf_accessor* normals =
                    cgltf_find_accessor(primitive, cgltf_attribute_type_normal, 0);
                const cgltf_accessor* texcoords =
                    cgltf_find_accessor(primitive, cgltf_attribute_type_texcoord, 0);
                const cgltf_accessor* colors =
                    cgltf_find_accessor(primitive, cgltf_attribute_type_color, 0);

                // --- Вершины примитива ---------------------------------------
                const uint32_t vertex_base = vertex_total;
                for (uint32_t i = 0; i < pv_count; ++i) {
                    // Позиция: glTF float → world → fixed 16.16 (fixed_from_float).
                    float position[3] = {0.0f, 0.0f, 0.0f};
                    if (!read_vec3(positions, i, position)) {
                        diag = CONVERT_ERR_PARSE;
                        goto finish;
                    }
                    double world[3] = {0.0, 0.0, 0.0};
                    transform_position(node, position, world);
                    Vertex v;
                    v.x = fixed_from_float((float)world[0]);
                    v.y = fixed_from_float((float)world[1]);
                    v.z = fixed_from_float((float)world[2]);

                    // Нормаль: атрибут → world-направление → i8 ±127; при отказе
                    // чтения остаётся (0,0,0) — перепишется фоллбеком flat-нормалей
                    // (buffer normal отсутствует вовсе).
                    double normal_world[3] = {0.0, 0.0, 0.0};
                    if (normals != nullptr && normals->type == cgltf_type_vec3) {
                        float normal[3] = {0.0f, 0.0f, 0.0f};
                        if (cgltf_accessor_read_float(normals, i, normal, 3u)) {
                            double dir[3] = {(double)normal[0], (double)normal[1],
                                             (double)normal[2]};
                            double confirm[3] = {0.0, 0.0, 0.0};
                            transform_direction(node, dir, confirm);
                            normalize_double(confirm);
                            normal_world[0] = confirm[0];
                            normal_world[1] = confirm[1];
                            normal_world[2] = confirm[2];
                        }
                    }
                    int8_t packed_normal[3] = {0, 0, 0};
                    pack_normal_i8(normal_world, packed_normal);
                    v.nx = packed_normal[0];
                    v.ny = packed_normal[1];
                    v.nz = packed_normal[2];

                    // UV: 0..1 → u8 (округление к ближайшему, Κlamp; AABB 0..1).
                    float uv[2] = {0.0f, 0.0f};
                    if (texcoords != nullptr && (texcoords->type == cgltf_type_vec2) &&
                        (cgltf_accessor_read_float(texcoords, i, uv, 2u))) {
                        if (uv[0] < 0.0f) uv[0] = 0.0f;
                        if (uv[0] > 1.0f) uv[0] = 1.0f;
                        if (uv[1] < 0.0f) uv[1] = 0.0f;
                        if (uv[1] > 1.0f) uv[1] = 1.0f;
                    }
                    v.u = convert_unit_to_u8(uv[0]);
                    v.v = convert_unit_to_u8(uv[1]);

                    // Цвет: COLOR_0 (VEC3: альфа 1.0; VEC4: как есть) или белый.
                    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                    if (colors != nullptr &&
                        (colors->type == cgltf_type_vec3 || colors->type == cgltf_type_vec4)) {
                        float read[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                        const cgltf_size components = (colors->type == cgltf_type_vec4) ? 4u : 3u;
                        if (cgltf_accessor_read_float(colors, i, read, components)) {
                            color[0] = read[0];
                            color[1] = read[1];
                            color[2] = read[2];
                            color[3] = (components == 4u) ? read[3] : 1.0f;
                        }
                    }
                    v.r = convert_unit_to_u8(color[0]);
                    v.g = convert_unit_to_u8(color[1]);
                    v.b = convert_unit_to_u8(color[2]);
                    v.a = convert_unit_to_u8(color[3]);

                    append_vertex_record(&payload, &v);
                }
                vertex_total += pv_count;

                // --- Индексы примитива (перебазировка на vertex_base) ---------
                std::vector<uint32_t> primer_raws;
                if (primitive->indices != nullptr) {
                    const cgltf_size count = primitive->indices->count;
                    if (count == 0u || (count % 3u) != 0u) {
                        diag = CONVERT_ERR_UNSUPPORTED;
                        goto finish;
                    }
                    for (cgltf_size i = 0; i < count; ++i) {
                        const cgltf_size raw = cgltf_accessor_read_index(primitive->indices, i);
                        if (raw >= pv_count) {
                            diag = CONVERT_ERR_PARSE;
                            goto finish;
                        }
                        primer_raws.push_back((uint32_t)raw);
                        index_stream.push_back(vertex_base + (uint32_t)raw);
                    }
                } else {
                    for (uint32_t i = 0; i < pv_count; ++i) {
                        primer_raws.push_back(i);
                        index_stream.push_back(vertex_base + i);
                    }
                }

                // Fallback нормалей: NORMAL не задан вовсе → flat по граням.
                if (normals == nullptr) {
                    assign_flat_normals(&payload, vertex_base, primer_raws);
                }
            }
        }

        // --- Пост-валидация агрегата --------------------------------------------
        const uint32_t index_count_total = (uint32_t)index_stream.size();
        if (vertex_total == 0u || index_count_total == 0u || (index_count_total % 3u) != 0u) {
            diag = CONVERT_ERR_UNSUPPORTED;
            goto finish;
        }

        // В заголовке §8.3 index_count — число 6-байтовых ЗАПИСЕЙ MeshIndex
        // (по одной на треугольник), а не сырых u16-индексов потока.
        const uint32_t index_record_count = index_count_total / 3u;

        // Радиус ограничивающей сферы: max ‖p‖² по сериализованным позициям.
        // Чтение i32 ЗНАКОВОЕ: 0xFFFF0000 в fixed 16.16 — это −1.0; беззнаковая
        // интерпретация дала бы +65536 и ложно-сатурированный радиус (sat 2^31).
        double max_len2 = 0.0;
        for (uint32_t i = 0; i < vertex_total; ++i) {
            const uint8_t* record = payload.data() + (size_t)i * ASSETS_MESH_VERTEX_BYTES;
            const double x = (double)(int32_t)assets_read_le_u32(record + 0u) / 65536.0;
            const double y = (double)(int32_t)assets_read_le_u32(record + 4u) / 65536.0;
            const double z = (double)(int32_t)assets_read_le_u32(record + 8u) / 65536.0;
            const double len2 = x * x + y * y + z * z;
            if (len2 > max_len2) max_len2 = len2;
        }
        const fixed_t radius = fixed_from_float((float)sqrt(max_len2));

        // --- Файл: заголовок (24) + вершины + индексы ----------------------------
        std::vector<uint8_t> file;
        append_le_u32(&file, ASSETS_MESH_MAGIC);
        append_le_u32(&file, ASSETS_MESH_VERSION);
        append_le_u32(&file, vertex_total);
        append_le_u32(&file, index_record_count);
        append_le_u32(&file, 0u); // texture_id: слот рантайма — вне glTF (v1 = 0)
        append_le_u32(&file, (uint32_t)radius);
        file.insert(file.end(), payload.begin(), payload.end());
        for (size_t i = 0; i + 2u < index_stream.size(); i += 3u) {
            append_index_record(&file, (uint16_t)index_stream[i + 0u],
                                (uint16_t)index_stream[i + 1u], (uint16_t)index_stream[i + 2u]);
        }

        FILE* out = fopen(output_path, "wb");
        if (out == nullptr) {
            diag = CONVERT_ERR_WRITE_OUTPUT;
            goto finish;
        }
        const size_t written = fwrite(file.data(), 1u, file.size(), out);
        if (fclose(out) != 0 || written != file.size()) {
            diag = CONVERT_ERR_WRITE_OUTPUT;
            goto finish;
        }

        if (out_error_code != nullptr) *out_error_code = CONVERT_OK;
        cgltf_free(gltf);
        return true;
    }

finish:
    if (gltf != nullptr) cgltf_free(gltf);
    if (out_error_code != nullptr) *out_error_code = diag;
    return false;
}
