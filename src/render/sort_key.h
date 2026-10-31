// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/sort_key.h
//
// Внутренние типы SORT-этапа рендера (Painter's algorithm, §7.2).
// Отдельный заголовок — единая точка правды порядка подачи для ядра
// (renderer.cpp) И конформанс-теста (tests/render/contract_test.cpp):
// порядок far→near + tie-breaker submission ASC — normative-контракт.
#pragma once

#include "hal/platform.h"

/**
 * Ссылка сортировки (SORT stage): cheap per-tri ключи + индекс команды.
 * depth_key валиден всегда (DEPTH_REJECT отфильтрован на PACK-входе —
 * rejected-треугольник не попадает в command buffer).
 */
typedef struct SortRef
{
    uint32_t depth_key;        /**< Ключ far→near. */
    uint32_t submission_index; /**< Tie-breaker вставки (детерминизм). */
    uint32_t cmd_index;        /**< Индекс команды в CommandBuffer. */
} SortRef;

/**
 * Строгое сравнение «a раньше b в порядке Painter's» (норматив §7.2):
 *   - ПЕРВИЧНО по depth_key ПО УБЫВАНИЮ (дальние — сначала, back-to-front);
 *   - при равном ключе — submission_index ПО ВОЗРАСТАНИЮ (детерминированный
 *     tie-breaker: порядок вставки сохраняется, никакого UB-недетермизма
 *     сортировки).
 * Строгий слабый порядок (вход детерминирован, пар без эквивалентности
 * нет: submission_index уникален в кадре).
 *
 * @param a Левая ссылка.
 * @param b Правая ссылка.
 * @return true — a подаётся раньше b.
 */
static inline bool sort_ref_before(const SortRef& a, const SortRef& b)
{
    if (a.depth_key != b.depth_key) {
        return a.depth_key > b.depth_key; // Дальние СНАЧАЛА (back-to-front).
    }
    return a.submission_index < b.submission_index; // Tie-breaker.
}
