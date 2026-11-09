// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/asset_converter/mesh_converter.h
//
// Конвертация glTF → .mesh v1 (host-only; Спринт 9–10, неделя 18).
#pragma once

#include <stdint.h>

/**
 * Конвертация «glTF-файл → .mesh v1».
 *
 * Атрибуты примитивов: POSITION (обязателен), NORMAL/TEXCOORD_0/COLOR_0
 * (опциональны — fallbacks: нормаль грани, UV (0,0), белый цвет).
 * Порядок: обход scene-графа (cgltf_node_transform_world), конвертация
 * float → fixed 16.16 (fixed_from_float), нормаль → i8 с сатурацией ±127,
 * UV/цвет → u8 (округление к ближайшему). Пост-проверка: index_count
 * кратно 3 и vertex_count ≤ 65535 (u16-индексация).
 *
 * @param input_path Имя входного .gltf.
 * @param output_path Имя выходного .mesh.
 * @param out_error_code Выход: код ConverterDiag (NULL допустим).
 * @return true при успехе; иначе false и записанный код.
 */
bool convert_mesh(const char* input_path, const char* output_path, uint32_t* out_error_code);
