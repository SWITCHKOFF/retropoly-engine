// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/render_backend.h
//
// ВНУТРЕННИЙ интерфейс рендер-бэкенда HAL (НЕ публичный API; ядро
// src/render/renderer.cpp вызывает его, реализация выбирается компиляцией —
// «ровно один HAL»: PC → src/hal/pc/render_opengl.cpp (Спринт 7–8),
// PS1 → src/hal/ps1/render_gpu.cpp (Спринт 13–14)).
//
// SDL/glad/PSn00bSDK НЕ включаются в этот заголовок (контракт platform.h:
// бэкенды подключают зависимости только в .cpp). Конвертация engine-
// представления (fixed 16.16, Mat4) в нативное представление — ОБЯЗАННОСТЬ
// бэкенда (§4.4 «Transforms contract»: сырые касты запрещены).
//
// Гранулярность подачи: по одному треугольнику на вызов (Painter's порядок
// формирует ядро рендера — детерминированная поэлементная подача).
#pragma once

#include "hal/platform.h"

#include <stdint.h>

/**
 * Вершина backend-подачи: НУЖНО держать совместимой с normative GLSL
 * (ps1_emu.vert: a_position vec3 / a_texcoord vec2 / a_color vec4).
 * Позиция — view-space (камера смотрит −Z); float — точка конверсии
 * fixed→native (§4.4; конверсии допустимы на границе бэкенда).
 */
typedef struct HalRenderBackendVertex
{
    float x, y, z;    /**< Позиция view-space (float). */
    float u, v;       /**< UV 0..1 (из u8/255). */
    float r, g, b, a; /**< Gouraud-цвет 0..1 (из u8/255; a — пост-MVP). */
} HalRenderBackendVertex;

/**
 * Инициализация backend'а рендера: reference FBO @p width×@p height,
 * компиляция шейдеров (эмуляция PS1: CI8, Gouraud, affine,
 * 1/16-px quantize), отключение Z-теста (glDisable(GL_DEPTH_TEST); текст
 * недели 13). Вызывается РОВНО ОДИН РАЗ из Render_Init после Platform_Init.
 *
 * @param width Ширина reference-фреймбуфера (320).
 * @param height Высота reference-фреймбуфера (240).
 * @return true при успехе; false — окно/GL-контекст отсутствуют или
 *         побочный сбой GL (штатная диагностика наверх).
 */
bool hal_render_backend_init(uint32_t width, uint32_t height);

/** Деинициализация backend'а (идемпотентна; безопасна без Init). */
void hal_render_backend_shutdown(void);

/**
 * Начало кадра: привязка reference-FBO, glClear цветом очистки кадра
 * (цвет «по умолчанию» = (RENDER_CLEAR_DEFAULT_*), else задан Clear).
 *
 * @param r Красная компонента очистки 0..255.
 * @param g Зелёная компонента.
 * @param b Синяя компонента.
 */
void hal_render_backend_begin_frame(uint8_t r, uint8_t g, uint8_t b);

/**
 * Создание GPU-текстуры из пары «CI8-пиксели + палитра» (данные читает ЯДРО
 * через render_texture_* и передаёт сюда — backend не знает про пул ядра).
 *
 * @param handle Хэндл ядровой текстуры (index+1; идентификатор клонов).
 * @param w Ширина (1..256).
 * @param h Высота (1..256).
 * @param pixels_ci8 Указатель на w×h байт индексов (не NULL).
 * @param palette Указатель на 256×u16 RGB555+STP (не NULL).
 * @return true при успехе; false — отказ GL.
 */
bool hal_render_backend_create_texture(TextureHandle handle, uint32_t w,
                                       uint32_t h, const uint8_t* pixels_ci8,
                                       const uint16_t* palette);

/** Удаление GPU-текстуры по хэндлу (идемпотентно; невалидные — no-op). */
void hal_render_backend_destroy_texture(TextureHandle handle);

/**
 * Установка очистки текущего кадра ПОВЕРХ уже начатого кадра
 * (semantics Render_Clear).
 *
 * @param r Красная компонента 0..255.
 * @param g Зелёная компонента.
 * @param b Синяя компонента.
 */
void hal_render_backend_clear(uint8_t r, uint8_t g, uint8_t b);

/**
 * Подача ОДНОГО треугольника после Painter's-сортировки (кадровый SUBMIT):
 * MVP — GL-представление (column-major float 16). texture == INVALID —
 * нетекстурированный Gouraud (flat, без семпла; недели 15–16); валидный
 * хэндл — привязка CI8-пары, текстуры должны существовать. GL-ошибка →
 * false (явная ошибка кадра).
 *
 * @param vertices Ровно 3 вершины (view-space float + атрибуты).
 * @param mvp_gl Матрица proj·view·model (float, column-major, 16).
 * @param texture Хэндл ядровой CI8-текстуры или RENDER_TEXTURE_INVALID.
 * @return true при успехе; false при GL-отказе/невалидной текстуре.
 */
bool hal_render_backend_draw_triangle(const HalRenderBackendVertex vertices[3],
                                      const float* mvp_gl, TextureHandle texture);

/**
 * Завершение кадра: выкладка reference-FBO на окно (fullscreen quad,
 * nearest sampling 320×240 → текущий размер окна). Swap окна выполняет
 * Platform_SwapBuffers (разделение владений сохранено с недели 9).
 */
void hal_render_backend_end_frame(void);

/**
 * Чтение фреймбуфера REFERENCE-FBO в RGBA8 (tests-only API для W0-хэша).
 *
 * @param dst_rgba8 Буфер ≥ width×height×4 байт (RGBA8 построчно).
 * @param dst_size Размер буфера в байтах.
 * @return true при успехе; false — отказ GL или невалидный буфер.
 */
bool hal_render_backend_read_pixels(uint8_t* dst_rgba8, uint32_t dst_size);
