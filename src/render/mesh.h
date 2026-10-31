// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/mesh.h
//
// Runtime-представление меша рендера. Вершина — нормативный layout §3
// (docs/RetroEngine_Engineering_Documentation_v1.43.md) + .mesh v1
// (будущий Спринт 9–10): 21-байтная запись { i32 x,y,z 16.16;
// i8 nx,ny,nz; u8 u,v; u8 r,g,b,a }, little-endian, БЕЗ ABI-padding
// (сериализация — задача loader'а Спринта 9–10, здесь — декларация).
//
// Render_DrawMesh(vertices, vertex_count, triangles, triangle_count, model)
// принимает РАЗОБРАННЫЕ массивы (без загрузки файлов — файловые форматы
// появляются в Спринте 9–10); индексы — всегда кратно 3 (v0/v1/v2).
#pragma once

#include "hal/platform.h"

/**
 * Правила валидации DrawMesh-входов (Failure policy §1: malformed input
 * reject before dereference; весь вызов отклоняется целиком при любой
 * ошибке — частичного добавления в command buffer не происходит):
 *   - vertices != NULL, indices != NULL, model != NULL;
 *   - vertex_count ≥ 3, triangle_count ≥ 1;
 *   - каждый индекс < vertex_count;
 *   - capacity command buffer достаточна (overflow — явная ошибка).
 */
