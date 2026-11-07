// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/gte_adapter.h
//
// ВНУТРЕННИЙ интерфейс GTE-адаптера HAL PS1 (Спринты 13–14). Мост между
// engine-представлением (fixed 16.16, view-space «Y вверх, камера в −Z») и
// конвенцией GTE (Y вниз, Z в глубину, SVECTOR-подача 1.3.12, Divider H/SZ3).
//
// КОНТРАКТ: заголовки PSn00bSDK (psxgte.h, inline_c.h) включаются ТОЛЬКО в
// gte_adapter.cpp — этот заголовок компилируется обоими бэкендами без них.
//
// Потребители: gpu_backend.cpp (draw_triangle вызывает transform на каждый
// треугольник), platform_ps1.cpp (reset при инициализации устройства).
#pragma once

#include "hal/platform.h" // Mat4 + fixed_t (ключевой контракт; PSn00bSDK — в .cpp)
#include "hal/ps1/hal_ps1_internal.h"

/**
 * Статус per-draw адаптера одного треугольника:
 *
 *   OK     — экранные координаты получены и записаны в примитив;
 *   CULLED — NCLIP ≥ 0 (знаковая площадь ≥ 0 — эквивалент backface-региона
 *            ядра): примитив НЕ добавляется в OT. Это НОРМАЛЬНАЯ стадия
 *            конвейера (ядро отбрасывает backface ДО бэкенда; сюда попадают
 *            только расхождения на границе «площадь ≈ 0» из-за round-off):
 *            draw_triangle возвращает true БЕЗ диагностики;
 *   FAIL   — нарушено активное ограничение представления GTE (int16 IR/SZ,
 *            NCLIP-подача, GPU-окно координат — см. transform): draw_triangle
 *            возвращает false; цепочка fail-fast ядра фиксирует
 *            RENDER_DIAG_BACKEND_FAIL и прерывает Submission кадра
 *            (clamp и silent drop запрещены).
 */
typedef enum HalPs1AdapterStatus
{
    HAL_PS1_ADAPTER_OK = 0,
    HAL_PS1_ADAPTER_CULLED = 1,
    HAL_PS1_ADAPTER_FAIL = 2
} HalPs1AdapterStatus;

/** Сброс адаптера (identity-матрица в GTE-регистры, обнуление камеры). */
void hal_ps1_adapter_reset(void);

/**
 * Подача камеры (ядро вызывает из Render_SetCamera ПОСЛЕ валидации near/far).
 *
 * Из proj (fixed 16.16, column-major) выводится:
 *   - H = ⌊m[5]·120⌋ (регистр C2_H GTE; f = m[5] = cot(fov_y/2),
 *     высота фреймбуфера 240 → h/2 = 120);
 *   - near = B/(A−1), far = B/(A+1) (A = m[10], B = m[14] — та же fixed-
 *     математика обратного вывода, что derive_near_far_from_projection ядра;
 *     дублирование целенаправленное: слоёный бэкенд не ходит в ядро);
 *   - k — масштаб raw-единиц GTE: целые единицы движка → int (селекция из
 *     обеих границ: нижняя H/(2·near) — главный порог Divider, верхняя
 *     из IR/SZ-диапазонов; см. hal_ps1_internal.h).
 *
 * @param view View-матрица (PS1 НЕ читает: вершины приходят уже view-space;
 *             параметр под общий контракт обеих платформ).
 * @param proj Projection-матрица (fixed 16.16); NULL — безопасный отказ
 *             (состояние камеры сбрасывается: все последующие draw = FAIL).
 */
void hal_ps1_adapter_set_camera(const Mat4* view, const Mat4* proj);

/**
 * Трансформация одного треугольника через GTE (rtpt → nclip → stsxy).
 *
 * Вход: ТРИ view-space float вершины (граница бэкенда §4.4: ядро
 * конвертировало fixed→float, здесь обратный snap через fixed_from_float —
 * побитно однозначный для 16.16-точности входа). Далее все значения — fixed:
 * поза нормализуется D = diag(1,−1,−1) (y вниз, z в глубину), Centroid
 * масштабируется k в TR-регистры (int32), дельты (v − Centroid) — в SVECTOR
 * (int16; обход предела ±8 единиц большой дистанцией в TR).
 *
 * Выход: GTE SXY0..2 (пары sx,sy — 12.4 пикселя) записываются в @p prim:
 * текстурный POLY_GT3 — по офсетам 8/20/32 Б (macro gte_stsxy3_gt3 — то же
 * раскладка struct psxgpu), нетекстурный POLY_G3 — 8/16/24 Б (gte_stsxy3_g3).
 *
 * Per-draw явные проверки (каждое нарушение → FAIL; clamp запрещён):
 *   |x·k|, |y·k| ≤ 32766 (IR1/IR2 int16-насыщение +/- slack MAC-суммы);
 *   d·k ∈ [2..65532] (SZ3 unsigned 16-bit +/- slack);
 *   |v−Centroid|·k ≤ 32767 (SVECTOR подача);
 *   |sx|, |sy| ≤ 1023 px (GPU-окно + защита от wrap 12.4-инт16);
 *   |Centroid·k| ≤ 524287 (int32 MAC при sf = 1).
 *
 * @param vx ВХОД: float x трёх вершин (не NULL).
 * @param vy ВХОД: float y трёх вершин (не NULL).
 * @param vz ВХОД: float z трёх вершин (не NULL).
 * @param prim указатель на экземпляр POLY_GT3 или POLY_G3 (поля SXY ЗАПИСАНЫ
 *             здесь; остальные поля — задача gpu_backend).
 * @param textured 1 — prim = POLY_GT3 (офсеты 8/20/32); 0 — POLY_G3 (8/16/24).
 *
 * @return Статус (OK/CULLED/FAIL).
 */
HalPs1AdapterStatus hal_ps1_adapter_transform(const float* vx, const float* vy, const float* vz,
                                              void* prim, uint32_t textured);
