// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/texture.h
//
// Текстуры движка: CI8 (индексированные 0..255) + палитра RGB555+STP.
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md, §1.2.5
//     (PS1 RGB555+STP CONTRACT — bit layout R[4:0], G[9:5], B[14:10], STP[15]);
//   - docs/plans/sprint_07_08.md, неделя 15 (CreateTexture/Destroy/Bind;
//     «запрет тестовых заглушек вида (i<<8)|i вместо честной RGB555+STP»).
//
//   - docs/plans/sprint_17_18.md (неделя 33: ёмкость арены W1-ворклоада —
//     4×256×256 CI8 = 256 KiB, по текстуре 256×256 на каждый из 4 кубов;
//     «one 256x256 texture per cube at most» §W1 — release-ворклоад).
//
// Хранение: фиксированные статические области (без new/delete — §1 «Явное
// управление памятью»): максимум RENDER_TEXTURE_MAX текстур, сторона одной —
// не более RENDER_TEXTURE_MAX_DIM (256×256), общий байт CI8 — не более
// RENDER_TEXTURE_POOL_BYTES (256 KiB — W1, см. выше). Переполнение — явный
// отказ (hэндл 0), расширение памяти запрещено; backend'и читают данные по
// хэндлу.
#pragma once

#include "hal/platform.h"

/** Максимум текстур в пуле рендера (совокупно с HAL-backend'ом). */
#define RENDER_TEXTURE_MAX 16u

/** Максимум стороны одной текстуры (PS1-реалистичный CLUT-лимит MVP). */
#define RENDER_TEXTURE_MAX_DIM 256u

/** Общий объём арены пикселей CI8 (байт): 4×256×256 = 256 KiB. */
#define RENDER_TEXTURE_POOL_BYTES (256u * 256u * 4u)

// ---------------------------------------------------------------------------
// Нормативный bit-layout палитры (§1.2.5)
// ---------------------------------------------------------------------------

/**
 * Упаковка 5-битных компонент + STP в u16 PS1-формата:
 *   R[4:0] | G[9:5] | B[14:10] | STP[15].
 * Нормативные тест-векторы (НУЖНО держать в tests/render/texture_test.cpp):
 *   pack_rgb555_stp(31,0,0,true) == 0x801F, (0,31,0,true) == 0x83E0,
 *   (0,0,31,true) == 0xFC00. Компоненты маскируются к 5 битам (безопасно).
 *
 * @param r5 Красная компонента 0..31.
 * @param g5 Зелёная компонента 0..31.
 * @param b5 Синяя компонента 0..31.
 * @param stp Флаг STP (semi-transparency bit; MVP-прозрачность выключена).
 * @return 16-битное слово палитры PS1-формата.
 */
static inline uint16_t pack_rgb555_stp(uint8_t r5, uint8_t g5, uint8_t b5, bool stp)
{
    return (uint16_t)((r5 & 0x1Fu) | ((g5 & 0x1Fu) << 5) | ((b5 & 0x1Fu) << 10) |
                      (stp ? 0x8000u : 0u));
}

/**
 * Распаковка 5-битной компоненты в 8-битную PS1-способом: бит дублируется
 * вниз ((c<<3)|(c>>2), 31→255, 0→0) — честное 5→8 расширение.
 *
 * @param c5 Компонента 0..31 (маскируется).
 * @return 8-битное значение канала.
 */
static inline uint8_t unpack_channel5_to_8(uint8_t c5)
{
    const uint8_t c = (uint8_t)(c5 & 0x1Fu);
    return (uint8_t)(((unsigned)c << 3) | ((unsigned)c >> 2));
}

/**
 * Полная распаковка слова палитры в 8-битные RGB-каналы (канал STP не
 * конвертируется — MVP-прозрачность выключена, бит хранится в слове).
 *
 * @param packed Слово RGB555+STP.
 * @param r8 Выход: красный канал 0..255.
 * @param g8 Выход: зелёный канал 0..255.
 * @param b8 Выход: синий канал 0..255.
 */
static inline void unpack_rgb555_stp(uint16_t packed, uint8_t* r8, uint8_t* g8, uint8_t* b8)
{
    *r8 = unpack_channel5_to_8((uint8_t)(packed & 0x1Fu));
    *g8 = unpack_channel5_to_8((uint8_t)((packed >> 5) & 0x1Fu));
    *b8 = unpack_channel5_to_8((uint8_t)((packed >> 10) & 0x1Fu));
}

// ---------------------------------------------------------------------------
// Пул текстур ядра (создание/удаление через renderer, данные — по хэндлу)
// ---------------------------------------------------------------------------

/**
 * Полный сброс пула текстур (все слоты освободить, arena очистить).
 * Вызывается Render_Init (детерминированное старт-состояние).
 */
void render_texture_pool_reset(void);

/**
 * Занято байт арены CI8-пикселей (S17–18: debug-overlay / VRAM-метрики).
 *
 * @return Использовано байт (0 — пул пуст/после reset).
 */
uint32_t render_texture_pool_used_bytes(void);

/**
 * Захват слота текстуры: копирует @p pixels (w×h CI8) и @p palette
 * (256×u16) в статические области.
 *
 * @param w Ширина 1..RENDER_TEXTURE_MAX_DIM.
 * @param h Высота 1..RENDER_TEXTURE_MAX_DIM.
 * @param pixels CI8-пиксели (w×h байт; NULL — отказ).
 * @param palette Палитра (256×u16 RGB555+STP; NULL — отказ).
 * @return Хэндл (index+1) или RENDER_TEXTURE_INVALID при отказе
 *         (пул полон / арена исчерпана / невалидные параметры).
 */
TextureHandle render_texture_acquire(uint32_t w, uint32_t h, const uint8_t* pixels,
                                     const uint16_t* palette);

/** Освобождение слота по хэндлу (пиксели возвращаются в арену). */
void render_texture_release(TextureHandle handle);

/** Ширина текстуры (0 при невалидном хэндле). */
uint32_t render_texture_width(TextureHandle handle);

/** Высота текстуры (0 при невалидном хэндле). */
uint32_t render_texture_height(TextureHandle handle);

/**
 * Пиксели CI8 живой текстуры (w×h байт).
 *
 * @param handle Хэндл (index+1; невалидный — NULL).
 * @return Указатель или NULL.
 */
const uint8_t* render_texture_pixels(TextureHandle handle);

/**
 * Палитра живой текстуры: 256 × u16 RGB555+STP.
 *
 * @param handle Хэндл (index+1; невалидный — NULL).
 * @return Указатель или NULL.
 */
const uint16_t* render_texture_palette(TextureHandle handle);
