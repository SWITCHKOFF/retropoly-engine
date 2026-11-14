// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp1_backend.h
//
// VDP1-путь HAL Saturn (Спринт 27–28): проекция view-space → экран
// 320×224, triangle→quad, текстуры/CLUT, Gouraud-таблицы, Painter's порядок
// через pinned command pipeline Jo Engine.
//
// HAL render_saturn.cpp делегирует сюда; публичные Render_* остаются в ядре.
#pragma once

#include "hal/render_backend.h"
#include "hal/saturn/vdp1_command_list.h"

#include <stdint.h>

namespace saturn {

/** Статистика VDP1-пути (детерминированная, для тестов/логов). */
struct Vdp1BackendStats
{
    uint32_t submitted;         /**< Принято треугольников. */
    uint32_t textured;          /**< Из них с текстурой. */
    uint32_t flat;              /**< Из них flat/Gouraud без текстуры. */
    uint32_t screen_rejects;    /**< Полностью вне 320×224. */
    uint32_t degenerate;        /**< Нулевая площадь. */
    uint32_t uv_fallbacks;      /**< UV вне углов → flat fallback. */
    uint32_t gouraud_fallbacks; /**< Кончились Gouraud-таблицы → flat. */
    uint32_t overflows;         /**< Переполнение command list/pipeline. */
    uint32_t commands_last;     /**< Команд в последнем кадре. */
    uint32_t pipeline_drops;    /**< Отказы __jo_vdp1_add_command. */
    uint32_t texture_clamps;    /**< height 256 → 255. */
    uint32_t pixels_last;       /**< Суммарная площадь quad'ов кадра (px). */
    uint32_t gouraud_tables_last; /**< Уникальных Gouraud-таблиц в кадре. */
};

/** Инициализация VDP1-пути (после Platform_Init/memory_init). */
bool vdp1_backend_init(void);

/** Деинициализация (идемпотентна). */
void vdp1_backend_shutdown(void);

/** Камера: проекция (view уже применён ядром к вершинам). */
void vdp1_backend_set_camera(const Mat4* view, const Mat4* proj);

/** Начало кадра: сброс списка, выбор набора Gouraud-таблиц, erase color. */
void vdp1_backend_begin_frame(uint16_t erase_color);

/** Создание VDP1-текстуры (CI8 + CLUT). */
bool vdp1_backend_create_texture(TextureHandle handle, uint32_t width, uint32_t height,
                                 const uint8_t* pixels, const uint16_t* palette);

/** Удаление VDP1-текстуры (идемпотентно). */
void vdp1_backend_destroy_texture(TextureHandle handle);

/** Подача треугольника (view-space + proj из set_camera). */
bool vdp1_backend_draw_triangle(const HalRenderBackendVertex vertices[3], const float* mvp,
                                TextureHandle texture);

/** Подача debug-overlay (экран 320×240, y вниз; Saturn crop 8..231). */
bool vdp1_backend_draw_overlay(const HalRenderBackendOverlayVertex* vertices, uint32_t count);

/** Конец кадра: финализация и передача команд в Jo Engine pipeline. */
void vdp1_backend_end_frame(void);

/** @return Накопленная статистика (не сбрасывается между кадрами). */
const Vdp1BackendStats& vdp1_backend_stats(void);

/** Сброс статистики (Render_Init). */
void vdp1_backend_reset_stats(void);

}  // namespace saturn
