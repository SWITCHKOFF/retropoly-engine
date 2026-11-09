// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/asset_converter/color_quantizer.cpp
//
// Median Cut → CI8 + палитра ≤256 RGBA8 (host-only; Спринт 9–10, неделя 19).
// Политика слота 0 — reserved «transparent black» (фиксируется в ASSETS.md).
//
// Детерминизм: нет зависимости от адресов/хешей; сортировки — по (ось-значение,
// код цвета), середина сплита — по гистограмме counts; пенальти при tie —
// строгий предел, выбор по индексу массива (первый встреченный).
// Octree-фоллбек не реализован: Median Cut на 256×256 (65536 пикселей,
// ≤65536 уникальных цветов) укладывается в единицы миллисекунд — ветка
// «fallback Octree если медленно» плана (sprint_09_10.md, неделя 19) остаётся
// неактивной по решению спринта.
#include "color_quantizer.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <vector>

// ---------------------------------------------------------------------------
// Вспомогательные структуры (pixel = RGBA8 упаковку в u32: A<<24|R<<16|G<<8|B)
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t CODE_ALPHA_SHIFT = 24u;
constexpr uint32_t CODE_R_SHIFT = 16u;
constexpr uint32_t CODE_G_SHIFT = 8u;
constexpr uint32_t CODE_B_SHIFT = 0u;

/** Уникальный цвет карты квантователя (число пикселей — counts). */
struct UniqueColor
{
    uint32_t code;   /**< A<<24 | R<<16 | G<<8 | B (упакованный RGBA8). */
    uint64_t count;  /**< Число пикселей с этим кодом. */
};

/** Компонента кода по оси (0=R, 1=G, 2=B; альфа не входит в разбивку). */
uint32_t code_channel(uint32_t code, uint32_t axis)
{
    switch (axis) {
        case 0u:
            return (code >> CODE_R_SHIFT) & 0xFFu;
        case 1u:
            return (code >> CODE_G_SHIFT) & 0xFFu;
        default:
            return (code >> CODE_B_SHIFT) & 0xFFu;
    }
}

/** Коробка Median Cut: непрерывный диапазон в отсортированном массиве items. */
struct Box
{
    size_t begin;   /**< Первый элемент диапазона [begin, end). */
    size_t end;     /**< Эксклюзивный конец диапазона. */
    uint64_t count; /**< Сумма counts диапазона. */
};

/**
 * Диапазон компоненты axis в коробке (max − min по её элементам).
 *
 * @param items Массив уникальных цветов.
 * @param box Коробка.
 * @param axis Ось (0=R, 1=G, 2=B).
 * @return Диапазон в 0..255 (0 — вырожденный).
 */
uint32_t box_channel_range(const std::vector<UniqueColor>& items, const Box& box, uint32_t axis)
{
    if (box.begin >= box.end) return 0u;
    uint32_t lo = 255u;
    uint32_t hi = 0u;
    for (size_t i = box.begin; i < box.end; ++i) {
        const uint32_t channel = code_channel(items[i].code, axis);
        if (channel < lo) lo = channel;
        if (channel > hi) hi = channel;
    }
    return hi - lo;
}

/**
 * Отсортировать диапазон коробки по оси (детерминированный tie: полный код).
 *
 * @param items Массив (сортируется диапазон [begin,end)).
 * @param box Коробка (обновляет границы после сортировки — остаются прежними).
 * @param axis Ось 0=R, 1=G, 2=B.
 */
void sort_box_on_axis(std::vector<UniqueColor>* items, const Box& box, uint32_t axis)
{
    if (box.begin >= box.end) return;
    const size_t le = box.begin;
    const size_t re = box.end;
    std::sort(items->begin() + le, items->begin() + re,
              [axis](const UniqueColor& f, const UniqueColor& s) {
                  const uint32_t f_axis = code_channel(f.code, axis);
                  const uint32_t s_axis = code_channel(s.code, axis);
                  if (f_axis != s_axis) return f_axis < s_axis;
                  return f.code < s.code;
              });
}

/**
 * Среднее коробки по компоненте (округление half-up; взвешено counts).
 *
 * @param items Массив уникальных цветов.
 * @param box Коробка.
 * @param axis Ось.
 * @return Округлённое среднее 0..255.
 */
uint32_t box_channel_mean(const std::vector<UniqueColor>& items, const Box& box, uint32_t axis)
{
    uint64_t sum = 0u;
    uint64_t count = 0u;
    for (size_t i = box.begin; i < box.end; ++i) {
        sum += (uint64_t)code_channel(items[i].code, axis) * items[i].count;
        count += items[i].count;
    }
    if (count == 0u) return 0u;
    return (uint32_t)((sum + count / 2u) / count);
}

} // namespace

bool color_quantize_median_cut(const uint8_t* rgba, uint32_t pixel_count, uint8_t* out_indices,
                               uint8_t* out_palette, uint8_t* out_used)
{
    if (rgba == nullptr || pixel_count == 0u || out_indices == nullptr || out_palette == nullptr ||
        out_used == nullptr) {
        return false;
    }

    // 1) Уникальные коды с counts (сортировка → пробеги одинаковых).
    std::vector<uint32_t> codes(pixel_count);
    for (uint32_t i = 0; i < pixel_count; ++i) {
        codes[i] = ((uint32_t)rgba[i * 4u + 0u] << CODE_R_SHIFT) |
                   ((uint32_t)rgba[i * 4u + 1u] << CODE_G_SHIFT) |
                   ((uint32_t)rgba[i * 4u + 2u] << CODE_B_SHIFT) |
                   ((uint32_t)rgba[i * 4u + 3u] << CODE_ALPHA_SHIFT);
    }
    std::sort(codes.begin(), codes.end());

    std::vector<UniqueColor> items;
    items.reserve(codes.size());
    {
        size_t run_begin = 0u;
        for (size_t i = 1u; i <= codes.size(); ++i) {
            if (i == codes.size() || codes[i] != codes[run_begin]) {
                UniqueColor unique;
                unique.code = codes[run_begin];
                unique.count = (uint64_t)(i - run_begin);
                items.push_back(unique);
                run_begin = i;
            }
        }
    }

    // 2) Политика слота 0: reserved zero entry; квантование — слоты 1..255.
    uint32_t palette_used = 1u; // следующий свободный слот (слот 0 reserved)
    memset(out_palette, 0, 256u * 4u);
    memset(out_indices, 0, (size_t)pixel_count);

    // 3) Median Cut при нужно; иначе — точная палитра уникальных цветов.
    std::vector<Box> boxes; // непрерывные диапазоны [begin,end) в items
    std::vector<uint8_t> box_slot; // слот палитры по индексу коробки

    const size_t max_boxes = 255u;
    if (items.size() <= max_boxes) {
        // ≤255 уникальных: каждая точка — своя коробка; палитра точная 1:1
        // (без потерь, кроме зарезервированного слота 0).
        for (size_t i = 0; i < items.size(); ++i) {
            Box one;
            one.begin = i;
            one.end = i + 1u;
            one.count = items[i].count;
            boxes.push_back(one);
            box_slot.push_back(0u);
        }
    } else {
        Box first;
        first.begin = 0u;
        first.end = items.size();
        uint64_t total = 0u;
        for (size_t i = first.begin; i < first.end; ++i)
            total += items[i].count;
        first.count = total;
        boxes.push_back(first);
        box_slot.push_back(0u);

        // Разбиваем коробку с самой широкой осью (tie — больше пикселей; при
        // равенстве — первая встреченная в списке, строгий > при скане).
        while (boxes.size() < max_boxes) {
            uint32_t best_range = 0u;
            uint64_t best_count = 0u;
            size_t best_box = 0u;
            uint32_t best_axis = 0u;
            bool found = false;
            for (size_t b = 0; b < boxes.size(); ++b) {
                for (uint32_t axis = 0u; axis < 3u; ++axis) {
                    const uint32_t range = box_channel_range(items, boxes[b], axis);
                    if (range == 0u) continue;
                    if (range > best_range ||
                        (range == best_range && boxes[b].count > best_count)) {
                        best_range = range;
                        best_count = boxes[b].count;
                        best_box = b;
                        best_axis = axis;
                        found = true;
                    }
                }
            }
            if (!found) break; // вырожденные коробки (все цветовые классы точны)

            // Сортируем коробку по самой широкой оси; сплит — по гистограмме.
            sort_box_on_axis(&items, boxes[best_box], best_axis);
            const Box& source = boxes[best_box];
            uint64_t cumulative = 0u;
            size_t split = source.begin;
            for (size_t i = source.begin; i < source.end; ++i) {
                cumulative += items[i].count;
                if (cumulative * 2u >= source.count) {
                    split = i + 1u;
                    break;
                }
            }
            if (split <= source.begin || split >= source.end) {
                // Защита вырожденности: сплит по половине элементов диапазона.
                split = source.begin + (source.end - source.begin) / 2u;
                if (split <= source.begin) split = source.begin + 1u;
                if (split >= source.end) split = source.end - 1u;
            }

            Box left;
            left.begin = source.begin;
            left.end = split;
            Box right;
            right.begin = split;
            right.end = source.end;
            uint64_t left_count = 0u;
            for (size_t i = left.begin; i < left.end; ++i)
                left_count += items[i].count;
            uint64_t right_count = 0u;
            for (size_t i = right.begin; i < right.end; ++i)
                right_count += items[i].count;
            left.count = left_count;
            right.count = right_count;

            boxes[best_box] = left;
            Box right_copy = right;
            // Новая коробка — в конец; слоты выделяются позже (по порядку).
            (void)box_slot.push_back(0u);
            boxes.push_back(right_copy);
        }
    }

    // Каждый box — colour-class; слот палитры — mean коробки (полу-баланс).
    // Порядок прохода — по массиву boxes (детерминированный).
    for (size_t b = 0; b < boxes.size(); ++b) {
        if (box_slot[b] != 0u) continue;
        const uint32_t r = box_channel_mean(items, boxes[b], 0u);
        const uint32_t g = box_channel_mean(items, boxes[b], 1u);
        const uint32_t bl = box_channel_mean(items, boxes[b], 2u);
        // Средний альфа-канал (MVP: альфа-информирование; в RGB555 не входит).
        uint64_t alpha_sum = 0u;
        uint64_t alpha_count = 0u;
        for (size_t i = boxes[b].begin; i < boxes[b].end; ++i) {
            alpha_sum += (uint64_t)((items[i].code >> CODE_ALPHA_SHIFT) & 0xFFu) * items[i].count;
            alpha_count += items[i].count;
        }
        const uint32_t a =
            (uint32_t)((alpha_sum + alpha_count / 2u) / (alpha_count > 0u ? alpha_count : 1u));

        // Дедупликация mean-цветов (две коробки могут дать одинаковую mean):
        // пиксели уйдут в уже существующий слот — палитра лишний раз не растёт.
        uint8_t found_slot = 0u;
        for (uint8_t slot = 1u; slot < palette_used; ++slot) {
            const uint8_t* entry = out_palette + (size_t)slot * 4u;
            if (entry[0] == r && entry[1] == g && entry[2] == bl) {
                found_slot = slot;
                break;
            }
        }
        if (found_slot == 0u && palette_used <= 255u) {
            uint8_t* entry = out_palette + (size_t)palette_used * 4u;
            entry[0] = (uint8_t)r;
            entry[1] = (uint8_t)g;
            entry[2] = (uint8_t)bl;
            entry[3] = (uint8_t)a;
            found_slot = (uint8_t)palette_used;
            palette_used = (palette_used + 1u);
        }
        box_slot[b] = found_slot != 0u ? found_slot : 1u; // защита вырождений
    }

    // Мапа «код цвета → слот» (каждой уникальный цвет попадает в свою коробку).
    std::map<uint32_t, uint8_t> code_to_slot;
    for (size_t b = 0; b < boxes.size(); ++b) {
        for (size_t i = boxes[b].begin; i < boxes[b].end; ++i) {
            code_to_slot[items[i].code] = box_slot[b];
        }
    }

    // 4) Назначение пикселей: точный матч по коду; reserved (0) не выдаётся.
    for (uint32_t i = 0; i < pixel_count; ++i) {
        const uint32_t code = codes[i];
        std::map<uint32_t, uint8_t>::const_iterator found = code_to_slot.find(code);
        if (found != code_to_slot.end()) {
            out_indices[i] = found->second;
            continue;
        }
        // Защитный маршрут (не должен случиться): ближайший slot 1..used-1
        // минимальной суммарной канальной дистанцией — детерминированный
        // первый встреченный.
        const uint32_t r = (code >> CODE_R_SHIFT) & 0xFFu;
        const uint32_t g = (code >> CODE_G_SHIFT) & 0xFFu;
        const uint32_t bl = (code >> CODE_B_SHIFT) & 0xFFu;
        uint32_t best_dist = 0xFFFFFFFFu;
        uint8_t best_slot = 1u;
        for (uint8_t slot = 1u; slot < palette_used; ++slot) {
            const uint8_t* entry = out_palette + (size_t)slot * 4u;
            const uint32_t dist = (r > entry[0] ? r - entry[0] : entry[0] - r) +
                                  (g > entry[1] ? g - entry[1] : entry[1] - g) +
                                  (bl > entry[2] ? bl - entry[2] : entry[2] - bl);
            if (dist < best_dist) {
                best_dist = dist;
                best_slot = slot;
            }
        }
        out_indices[i] = best_slot;
    }

    *out_used = (uint8_t)(palette_used - 1u); // без reserved слота 0
    return true;
}
