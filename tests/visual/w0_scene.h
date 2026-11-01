// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/visual/w0_scene.h
//
// W0-конформанс-сцена (docs/plans/sprint_07_08.md, неделя 16):
// комната 10×10×10 из 12 CI8-текстурированных треугольников (6 плоскостей
// × 2 треугольника, чекер 64×64 × палитра 256), Gouraud-цвета повершинно,
// прозрачность выключена (a = 255), фиксированная камера. Куб стоит ЦЕЛИКОМ
// перед камерой (z ∈ [−20,−5]; камера в (0,0,5) смотрит вдоль −Z) — все
// вершины внутри [near..far], все плоскости фронтальные: 12 подано,
// 0 reject (конформанс FULL-SUBMIT-сцены).
//
// Детерминированность: все координаты — целые fixed (fixed_from_int),
// текстуры и цвета — константы; никакой ia random/времени. Сцена строит
// Vertex/Triangle-массивы, совместимые с Render_DrawMesh.
#pragma once

#include "hal/platform.h"

#include <stdint.h>

/** Число треугольников комнаты W0 (норматив: 12 текстурированных). */
#define W0_ROOM_TRIANGLE_COUNT 12u

/** Число вершин комнаты (4 на плоскость × 6 плоскостей). */
#define W0_ROOM_VERTEX_COUNT 24u

/** Элементы геометрии W0-кейсов. */
typedef enum
{
    W0_CASE_VISIBLE = 0,      /**< Полностью видимый фронтальный tri. */
    W0_CASE_BACKFACE,         /**< Зеркальный winding → CULL reject. */
    W0_CASE_NEAR_CROSS,       /**< Вершина ближе near → whole-tri clip. */
    W0_CASE_FAR_CROSS,        /**< Вершина дальше far → whole-tri clip. */
    W0_CASE_BEHIND_CAMERA,    /**< Все вершины за камерой → clip/invalid. */
    W0_CASE_COUNT
} W0CaseId;

/**
 * Построение комнаты (12 треугольников). Входные массивы должны быть
 *Capacity: vertices ≥ W0_ROOM_VERTEX_COUNT, triangles ≥ 12.
 *
 * @param vertices Выходной массив вершин (24).
 * @param triangles Выходной массив индексов (12).
 * @param texture Хэндл CI8-чекер-текстуры (для ссылочной записи не нужен,
 *                связывание — Render_BindTexture вызывающего).
 */
void w0_build_room(Vertex* vertices, Triangle* triangles);

/**
 * Построение одного конформанс-кейса (одна вершина×3, один triangle).
 *
 * @param case_id Идентификатор кейса.
 * @param vertices Выходной массив вершин (3).
 * @param triangle Выходной треугольник (1).
 */
void w0_build_case(W0CaseId case_id, Vertex* vertices, Triangle* triangle);

/**
 * Создание CI8-чекер-текстуры (64×64, палитра 256): источник данных
 * детерминирован, честные №-пространственные индексы (0/1 через ячейки
 * 8×8). Возвращает пул, память — статическая (без free; размер одного
 * вызова фикс- ).
 *
 * @param out_w Выход: ширина (64).
 * @param out_h Выход: высота (64).
 * @return Указатель на статический пиксель-массив CI8 ( lifelong).
 */
const uint8_t* w0_make_checker_pixels(uint32_t* out_w, uint32_t* out_h);

/**
 * Палитра (256×u16 RGB555+STP) чекер-текстуры: 2 активных цвета (честный
 * PE- формат §1.2.5), остальные — оттенок фона.
 *
 * @return Указатель на статический массив палитры.
 */
const uint16_t* w0_make_checker_palette(void);

/**
 * Проспект-матрицы W0-камеры (фиксированные): view — gluLookAt
 * (позиция (0,0,5), цель (0,0,0), up (0,1,0) — камера смотрит вдоль −Z);
 * projection — gluPerspective (fov 60°, aspect 4:3, near 1, far 100).
 * Комната стоит перед камерой целиком (z ∈ [−20,−5]): все d ∈ [10..25].
 *
 * @param view Выходная view-матрица (не NULL).
 * @param proj Выходная projection-матрица (не NULL).
 */
void w0_camera(Mat4* view, Mat4* proj);
