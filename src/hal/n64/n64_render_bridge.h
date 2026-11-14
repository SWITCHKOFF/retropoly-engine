// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_render_bridge.h
//
// ВНУТРЕННИЙ C-мост N64 render-пути (Спринт 43–44). libdragon/RSPQ/RDPQ
// подключаются только в n64_render_bridge.c; host-тесты подменяют мост
// fake_n64_render_bridge.cpp (аналог fake_n64_bridge.cpp).
//
// Контракты:
//   - TNL: ровно один SetMatrix перед группой Transform; входные слоты
//     заполняет CPU, выходные читаются ТОЛЬКО после rpe_n64_tnl_wait();
//   - переполнение слотов — явный false + счётчик failures, без silent drop;
//   - RDP: текстура грузится полосой (strip) не более 2048 B (верхняя
//     половина TMEM зарезервирована под TLUT), затем рисуется треугольник;
//   - read_pixels: RGBA16 framebuffer → RGBA8 (tests/W0-hash);
//   - ticks_us: монотонные микросекунды libdragon для benchmark.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- T&L --------------------------------------------------------------------

/** Половина слота: 8 компонент Q16.16 в формате hi/lo (vec_slot_t). */
typedef struct RpeN64TnlSlotHalf
{
    int16_t i[8];  /**< Старшие 16 бит компонент 0..7. */
    uint16_t f[8]; /**< Младшие 16 бит компонент 0..7. */
} RpeN64TnlSlotHalf;

/** T&L-слот: 16 компонент = два vec_slot_t (0..7, 8..15). */
typedef struct RpeN64TnlSlot
{
    RpeN64TnlSlotHalf c[2];
} RpeN64TnlSlot;

/** Ёмкость T&L-батча (треугольников/слотов; static .bss). */
#define RPE_N64_TNL_MAX_SLOTS 512u

/** TMEM: 4 KiB; верхняя половина (2 KiB) — палитры/TLUT. */
#define RPE_N64_TMEM_BYTES 4096u
#define RPE_N64_TMEM_TEXTURE_BYTES 2048u
/** Максимальная ширина/высота RDP-тайла (texels). */
#define RPE_N64_TEX_TILE_MAX 64u

/** true — RSPQ/RDPQ/VI готовы и T&L overlay зарегистрирован. */
bool rpe_n64_render_init(void);

/** Снятие overlay/очереди. Идемпотентно; безопасно без init. */
void rpe_n64_render_shutdown(void);

/** true — rpe_n64_render_init выполнен. */
bool rpe_n64_render_ready(void);

/** true — RSP T&L overlay зарегистрирован и может использоваться. */
bool rpe_n64_tnl_available(void);

/** @return Ёмкость слотов (RPE_N64_TNL_MAX_SLOTS; 0 до init). */
uint32_t rpe_n64_tnl_slot_capacity(void);

/**
 * Входной слот (пишет CPU). Указатель действителен до следующего
 * transform/wait; align 16, cached RDRAM.
 */
RpeN64TnlSlot* rpe_n64_tnl_input_slot(uint32_t index);

/** Выходной слот (читает CPU после rpe_n64_tnl_wait). */
RpeN64TnlSlot* rpe_n64_tnl_output_slot(uint32_t index);

/**
 * Поставить transform слота @p index в очередь RSP. Матрица передаётся
 * В САМОЙ КОМАНДЕ (без общего DMEM-буфера и гонок между кадрами).
 * @return false — не готово/индекс вне ёмкости/NULL-матрица.
 */
bool rpe_n64_tnl_transform(uint32_t index, const RpeN64TnlSlot* matrix);

/** Дождаться выполнения всех команд RSP и инвалидировать output-кэш. */
void rpe_n64_tnl_wait(void);

/** @return Число явных отказов T&L (переполнение/не готово). */
uint32_t rpe_n64_tnl_failures(void);

// --- RDP --------------------------------------------------------------------

/** Экранная вершина RDP (y вниз; z 0..1; s/t в texels; invw — 1/w_clip). */
typedef struct RpeN64RdpVertex
{
    float x, y, z;
    float r, g, b, a;
    float s, t;
    float invw;
} RpeN64RdpVertex;

/**
 * Загрузить полосу текстуры CI8 (sx..sx+sw, полная высота) и TLUT.
 * @return false — полоса не влезает в TMEM/ошибка RDP.
 */
bool rpe_n64_rdp_upload_texture_strip(const uint8_t* pixels_ci8, const uint16_t* palette,
                                      uint32_t width, uint32_t height, uint32_t sx,
                                      uint32_t sw);

/**
 * Настроить RDP-режим группы (Z test/write, combiner, persp, TLUT).
 * Вызывается один раз на группу texture/strip — не на каждый треугольник.
 */
bool rpe_n64_rdp_begin_group(bool textured);

/**
 * Нарисовать один треугольник. @p textured=true — TRIFMT_ZBUF_SHADE_TEX;
 * false — TRIFMT_ZBUF_SHADE. Режим — из rpe_n64_rdp_begin_group.
 */
bool rpe_n64_rdp_draw_triangle(const RpeN64RdpVertex v[3], bool textured);

/** Настроить 2D-overlay режим (z-тест выключен) — перед rdpq_text/triangles. */
bool rpe_n64_rdp_begin_overlay_mode(void);

/** Нарисовать overlay-треугольники (экранные, z-тест выключен). */
bool rpe_n64_rdp_draw_overlay(const RpeN64RdpVertex* vertices, uint32_t count);

/** true — framebuffer доступен для readback (был хотя бы один кадр). */
bool rpe_n64_frame_read_pixels(uint8_t* dst_rgba8, uint32_t dst_size);

// --- timing -----------------------------------------------------------------

/** Монотонные микросекунды libdragon (uint64, benchmark/G12). */
uint64_t rpe_n64_ticks_us(void);

#ifdef __cplusplus
}
#endif
