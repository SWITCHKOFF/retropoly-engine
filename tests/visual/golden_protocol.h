// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/visual/golden_protocol.h
//
// Единый протокол golden-сравнения (метод нормативен — docs/plans/
// sprint_07_08.md, неделя 16). Выделен из render_golden_test.cpp в Спринте
// 11–12: hello_triangle golden (tests/visual/hello_triangle_golden_test.cpp)
// обязан использовать ТОТ ЖЕ метод readback+hash — два протокола сравнения
// в одном методе регрессии недопустимы.
//
// Метод: framebuffer readback + FNV-1a 64 над 15-битным сжатием RGBA
// (гашение LSB-джиттера драйверов — §1.2.9). Эталоны: 8-байтные big-endian
// хэши в tests/visual/reference/<name>.bin (+ px-партнёр для фолбэка v2).
// Rebase — ТОЛЬКО env RENDER_GOLDEN_REBASE=1 (осознанная запись эталона;
// никогда не «подгонка» падающего теста).
#pragma once

#include <stdint.h>

namespace golden {

/// Headless-обнаружение (Linux DISPLAY/WAYLAND; канонически репозиторием).
bool display_available();

/// Упаковка RGBA-8 в PS1-родные 15 бит: R>>3 | G>>3<<5 | B>>3<<10.
uint16_t px_pack15(const uint8_t* px);

/**
 * Снятие readback ТЕКУЩЕГО кадра: hash + внутренний снимок пикселей
 * (фолбэк/rebase читается ИЗ ЭТОГО снимка — вызывать пока GL жив,
 * до Render_Shutdown/Platform_Shutdown).
 *
 * При env RENDER_GOLDEN_DUMP=1 дополнительно пишет диагностический
 * px-дамп <reference>/<test>.dump.px (throwaway, не коммитится).
 *
 * @return FNV-1a 64 readback-хэша (0 — readback недоступен).
 */
uint64_t capture_readback();

/**
 * Сравнение кадра с эталоном <name> (или запись при RENDER_GOLDEN_REBASE=1).
 *
 * Протокол v2 («hash fast-path + фолбэк»):
 *  1. Хэш равен эталону — PASS (сырые пиксели не читаются: 7 из 8 эталонов
 *     bit-стабильны между драйверами — fast-path канон).
 *  2. @p budget > 0 — фолбэк при рассинхроне хэша: rasterizer tie-breaking
 *     на силуэтных рёбрах треугольников НЕ является GL-инвариантом; фолбэк
 *     считает отличия 15-битных пикселей против px-партнёра эталона
 *     (<name>.px): PASS при (отличий ≤ budget) И (дельта каждого канала
 *     ≤ 2 LSB). Сдвиг целого региона (деградация сцены) проваливает оба
 *     порога.
 *
 * @param name Имя эталона («hello_triangle_default.bin»).
 * @param hash Хэш актуального кадра (из capture_readback).
 * @param budget Допуск фолбэка в px; 0 — строгий hash-режим.
 * @return true при совпадении (или успешной rebase-записи).
 */
bool check(const char* name, uint64_t hash, uint32_t budget);

} // namespace golden
