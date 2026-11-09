// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/overlay_text.h
//
// Debug-overlay: растровый шрифт 3×5 и сборка экранных треугольников
// (норматив: docs/plans/sprint_17_18.md, неделя 36 — «Debug overlay: FPS,
// RAM usage, triangle count, (опц. VRAM)»).
//
// Модель: глиф = битовая матрица 3×5 (5 строк; запись как в двоичной
// нотации — СТАРШИЙ бит (bit2) = ЛЕВЫЙ пиксель строки: «110» = 6 —
// два левых пикселя). Каждый ЗАЖЖЁННЫЙ пиксель — quad (2 треугольника) в
// пиксельных координатах фреймбуфера (y вниз от верхнего левого угла —
// общая конвенция PC/PS1). Такой текст не зависит от камеры/режима
// текстурирования/прозрачности (transparency off W1 не мешает: фон не
// рисуется вовсе).
//
// Детерминизм: целочисленная геометрия (int16 пиксели), цвет u8 — никаких
// float/таблиц; одинаковый вход даёт байт-в-байт вершины.
#pragma once

#include "hal/render_backend.h"

#include <stdint.h>

namespace overlay_text {

/** Ширина глифа (пикс). */
constexpr int32_t kGlyphWidth = 3;
/** Высота глифа (пикс). */
constexpr int32_t kGlyphHeight = 5;
/** Межстрочный шаг по X (глиф + 1 пиксель зазора). */
constexpr int32_t kAdvance = 4;

/**
 * Таблица глифов: 5 строк × 3 бита в двоичной нотации (bit2 = ЛЕВЫЙ пиксель
 * строки; «110» = 6 — два левых пикселя). Поддерживаются: A–Z, 0–9, пробел
 * и пунктуация ":./%-+=,[]()". Неизвестный символ рисуется как пробел
 * (детерминированная деградация). Строчные переводятся в верхний регистр.
 */
struct Glyph
{
    char code;
    uint8_t rows[kGlyphHeight];
};

static constexpr Glyph kGlyphs[] = {
    {' ', {0, 0, 0, 0, 0}},       {'A', {2, 5, 7, 5, 5}},      // 010/101/111/101/101
    {'B', {6, 5, 6, 5, 6}},       {'C', {3, 4, 4, 4, 3}},      // 110/101/110/101/110
    {'D', {6, 5, 5, 5, 6}},       {'E', {7, 4, 6, 4, 7}},
    {'F', {7, 4, 6, 4, 4}},       {'G', {3, 4, 5, 5, 3}},
    {'H', {5, 5, 7, 5, 5}},       {'I', {7, 2, 2, 2, 7}},
    {'J', {1, 1, 1, 5, 2}},       {'K', {5, 5, 6, 5, 5}},
    {'L', {4, 4, 4, 4, 7}},       {'M', {5, 7, 7, 5, 5}},
    {'N', {5, 7, 7, 7, 5}},       {'O', {2, 5, 5, 5, 2}},
    {'P', {6, 5, 6, 4, 4}},       {'Q', {2, 5, 5, 6, 3}},
    {'R', {6, 5, 6, 5, 5}},       {'S', {3, 4, 2, 1, 6}},
    {'T', {7, 2, 2, 2, 2}},       {'U', {5, 5, 5, 5, 7}},
    {'V', {5, 5, 5, 5, 2}},       {'W', {5, 5, 7, 7, 5}},
    {'X', {5, 5, 2, 5, 5}},       {'Y', {5, 5, 2, 2, 2}},
    {'Z', {7, 1, 2, 4, 7}},       {'0', {7, 5, 5, 5, 7}},
    {'1', {2, 6, 2, 2, 7}},       {'2', {6, 1, 2, 4, 7}},
    {'3', {6, 1, 2, 1, 6}},       {'4', {5, 5, 7, 1, 1}},
    {'5', {7, 4, 6, 1, 6}},       {'6', {3, 4, 7, 5, 7}},
    {'7', {7, 1, 2, 2, 2}},       {'8', {7, 5, 7, 5, 7}},
    {'9', {7, 5, 7, 1, 6}},       {':', {0, 2, 0, 2, 0}},
    {'.', {0, 0, 0, 0, 2}},       {'/', {1, 1, 2, 4, 4}},
    {'%', {5, 1, 2, 4, 5}},       {'-', {0, 0, 7, 0, 0}},
    {'+', {0, 2, 7, 2, 0}},       {'=', {0, 7, 0, 7, 0}},
    {',', {0, 0, 0, 2, 4}},       {'[', {3, 2, 2, 2, 3}},
    {']', {6, 2, 2, 2, 6}},       {'(', {1, 2, 2, 2, 1}},
    {')', {4, 2, 2, 2, 4}},
};

/** Верхний регистр для поддерживаемых строчных (иначе символ как есть). */
inline char to_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/** Таблица глифа для символа @p c (nullptr — символ не поддержан). */
inline const Glyph* find_glyph(char c)
{
    const char upper = to_upper(c);
    for (const Glyph& glyph : kGlyphs) {
        if (glyph.code == upper) {
            return &glyph;
        }
    }
    return nullptr;
}

/**
 * Сборка вершин текста @p text в буфер @p out (пиксельные координаты;
 * y вниз от верха фреймбуфера).
 *
 * Каждый зажжённый пиксель глифа — quad (2 треугольника, 6 вершин) со
 * стороной @p scale. Новая строка '\n' — переход на следующую строку
 * (шаг (kGlyphHeight+1)·scale). Переполнение буфера — ДЕТЕРМИНИРОВАННАЯ
 * обрезка (текст усечён, false возвращается один раз).
 *
 * @param out Буфер вершин (может быть nullptr при cap 0).
 * @param max_vertices Ёмкость буфера (кратно 6 для полного глифа).
 * @param x Пиксельный X начала (левый верх глифа).
 * @param y Пиксельный Y начала (верх глифа).
 * @param r,g,b Цвет (Gouraud, 0..255).
 * @param scale Целочисленный масштаб глифа (1 — 3×5 пикс; 2 — 6×10).
 * @param text NUL-терминированная ASCII-строка.
 * @return Число записанных вершин (кратно 6).
 */
inline uint32_t build(HalRenderBackendOverlayVertex* out, uint32_t max_vertices, int32_t x,
                      int32_t y, uint8_t r, uint8_t g, uint8_t b, uint32_t scale,
                      const char* text)
{
    if (out == nullptr || text == nullptr || scale == 0u) {
        return 0u;
    }
    const int16_t pixel = (int16_t)scale;
    int32_t pen_x = x;
    int32_t pen_y = y;
    uint32_t count = 0u;

    for (const char* it = text; *it != '\0'; ++it) {
        if (*it == '\n') {
            pen_x = x;
            pen_y += (kGlyphHeight + 1) * (int32_t)scale;
            continue;
        }
        const Glyph* glyph = find_glyph(*it);
        if (glyph == nullptr) {
            glyph = find_glyph(' '); // Неизвестный символ — пробел.
        }
        for (int32_t row = 0; row < kGlyphHeight; ++row) {
            const uint8_t bits = glyph->rows[row];
            for (int32_t col = 0; col < kGlyphWidth; ++col) {
                // Старший бит — левый пиксель (двоичная нотация таблицы).
                if ((bits & (uint8_t)(1u << (kGlyphWidth - 1 - col))) == 0u) {
                    continue;
                }
                if (count + 6u > max_vertices) {
                    return count; // Детерминированная обрезка.
                }
                const int16_t x0 = (int16_t)(pen_x + col * (int32_t)scale);
                const int16_t y0 = (int16_t)(pen_y + row * (int32_t)scale);
                const int16_t x1 = (int16_t)(x0 + pixel);
                const int16_t y1 = (int16_t)(y0 + pixel);
                // Два треугольника quad'а (порядок вершин стабилен —
                // детерминизм; бэкенды не кулят overlay).
                const HalRenderBackendOverlayVertex v0{x0, y0, r, g, b};
                const HalRenderBackendOverlayVertex v1{x1, y0, r, g, b};
                const HalRenderBackendOverlayVertex v2{x1, y1, r, g, b};
                const HalRenderBackendOverlayVertex v3{x0, y1, r, g, b};
                out[count++] = v0;
                out[count++] = v1;
                out[count++] = v2;
                out[count++] = v0;
                out[count++] = v2;
                out[count++] = v3;
            }
        }
        pen_x += kAdvance * (int32_t)scale;
    }
    return count;
}

} // namespace overlay_text
