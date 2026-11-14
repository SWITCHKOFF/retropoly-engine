// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/rsp_tnl.h
//
// Чистая (host-тестируемая) математика RSP T&L и RDP-разбиения текстур
// (Спринт 43–44). libdragon здесь не подключается: те же функции
// используются N64-бэкендом, CPU-fallback'ом и host-тестами.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/n64/n64_render_bridge.h"

namespace n64 {

/** Q16.16 единица (fixed_t-совместимая). */
constexpr int32_t TNL_ONE = 65536;

/** Вершина T&L: view-space позиция + UV 0..1 + Gouraud RGBA 0..1. */
struct TnlVertex
{
    float x, y, z;
    float u, v;
    float r, g, b, a;
};

/** Треугольник T&L (подача после clip/cull ядра). */
struct TnlTriangle
{
    TnlVertex v[3];
};

/** Экранная проекция clip-вершины (RDP-конвенция: y вниз, z 0..1). */
struct TnlScreenVertex
{
    float x, y, z;
    float invw;
};

/** Матрица 4×4, GL column-major Q16.16 (вход RSP-микрокода). */
struct TnlMatrix
{
    int32_t m[16];
};

/** Q16.16 → float (детерминированное деление). */
float tnl_fixed_to_float(int32_t value);

/** float → Q16.16 с насыщением (границы берутся от int32). */
int32_t tnl_float_to_fixed(float value);

/**
 * MVP (GL column-major float[16]) → row-major Q16.16 матрица.
 * @return false — NULL/NaN/Inf/переполнение хотя бы одного элемента.
 */
bool tnl_matrix_from_mvp_gl(const float* mvp_gl, TnlMatrix* out);

/** Матрица → RSP-слот (hi/lo, 16 компонент column-major). */
void tnl_matrix_to_slot(const TnlMatrix& matrix, RpeN64TnlSlot* out);

/** Три вершины → RSP-слот (v0/v1/v2.xyzw, w=1; остальное 0). */
void tnl_vertices_to_slot(const TnlVertex v[3], RpeN64TnlSlot* out);

/** 4-вектор (Q16.16) → RSP-слот (компоненты 0..3, остальное 0). */
void tnl_vec4_to_slot(const float v[4], RpeN64TnlSlot* out);

/** Компонент слота (0..15) как int32 Q16.16. */
int32_t tnl_slot_component(const RpeN64TnlSlot& slot, uint32_t index);

/**
 * CPU-эталон RSP transform: clip = M · (x,y,z,1), Q16.16.
 * Используется CPU-fallback'ом и сверкой с RSP в ROM self-test.
 */
void tnl_transform_clip_cpu(const TnlMatrix& matrix, const TnlVertex v[3], int32_t out_clip[3][4]);

/**
 * clip-space (Q16.16) → экран RDP 320×240 (y вниз, z 0..1, invw=1/w).
 * @return false — w ≤ 0 (за камерой; ядро обязано отсечь раньше).
 */
bool tnl_project_clip(const int32_t clip[4], uint32_t width, uint32_t height,
                      TnlScreenVertex* out);

/**
 * Ширина полосы CI8-текстуры, помещающейся в TMEM (с TLUT):
 * max(8, min(w, 2048/h, 64)) с выравниванием вниз до 8 texels.
 * @return 0 — текстура не помещается (h > 256 или w == 0).
 */
uint32_t tnl_texture_strip_width(uint32_t width, uint32_t height);

/** Число полос текстуры (ceil(width / strip_width)); 0 — unsupported. */
uint32_t tnl_texture_strip_count(uint32_t width, uint32_t height);

/** Байты TMEM для полосы sw×height CI8 + 256-entry TLUT (512 B). */
uint32_t tnl_tmem_bytes(uint32_t strip_width, uint32_t height);

/**
 * Отсечение треугольника плоскостями u∈[u0,u1] (полоса текстуры).
 * Атрибуты интерполируются линейно в view-space (UV/цвет линейны по
 * объекту; перспективная коррекция — аппаратная RDP).
 *
 * @param in Исходный треугольник.
 * @param u0,u1 Границы полосы (0..1, u0 < u1).
 * @param out Выход: до 4 треугольников.
 * @return Число треугольников (0 — вне полосы).
 */
uint32_t tnl_clip_to_u_range(const TnlTriangle& in, float u0, float u1, TnlTriangle out[4]);

/** FNV-1a 32-bit (W0 framebuffer-hash). */
uint32_t tnl_fnv1a32(const void* data, size_t size);

/**
 * CPU-эталон vertex-lighting (ambient + directional): color_v =
 * clamp255(ambient + light·max(0, dot(normal, light_dir))). Нормали
 * view-space float; результат 0..255. Проверяется host-тестами и
 * ROM self-test'ом против RSP-lighting команды.
 */
void tnl_light_cpu(const float normal[3], const float light_dir[3], const uint8_t ambient[3],
                   const uint8_t light_color[3], uint8_t out_rgb[3]);

/**
 * CPU-эталон RSP diffuse: out_q16[c] = light_color[c] · max(0, dot(n, L))
 * в Q16.16 (0..255·65536). RSP-light команда возвращает ровно это;
 * ambient добавляется на CPU при упаковке в u8.
 */
void tnl_light_diffuse_cpu(const float normal[3], const float light_dir[3],
                           const uint8_t light_color[3], int32_t out_q16[3]);

}  // namespace n64
