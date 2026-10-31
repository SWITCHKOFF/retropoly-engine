// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/renderer.h
//
// Ядро рендера (Спринт 7–8): lifecycle-контракт, Painter's-алгоритм
// (depth_key + tie-breaker), PACK (command buffer), конформанс-хэш W0.
// Публичные функции Render_* объявлены в hal/platform.h (единый C-API §3);
// здесь — внутренние контракты ядра рендера (видимые тестам).
//
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §1.2.6 (DEPTH/OT
//     CONTRACT — depth_key_from_camera_z), §7.1–7.4 (pipeline + painter's),
//     §1 (Lifecycle contract, Renderer safety contract, Visibility contract);
//   - docs/plans/sprint_07_08.md (недели 13–16).
//
// КОНВЕНЦИИ КАМЕРЫ (этого ядра, детерминированные):
//   - view — right-handed gluLookAt-матрица (камера смотрит в −Z);
//   - «camera-space depth» треугольника: d = −z_view > 0 (расстояние по
//     вперёд-оси, fixed 16.16);
//   - CLIP/REJECT (Visibility contract): вершина с d < near или d > far
//     выбрасывает ВЕСЬ треугольник (явный reject с диагностикой; near/far
//     выводятся из proj честной fixed-математикой: near = B/(A−1),
//     far = B/(A+1), где A=m[10], B=m[14] — контратное вывод gluPerspective);
//   - CULL (PS1-NCLIP-эмуляция): знаковая площадь в экранных координатах
//     (y вниз, 320×240) — A<0 фронт (CCW для наблюдателя), A≥0 — backface.
#pragma once

#include "core/math/fixed.h"
#include "core/math/mat4.h"
#include "hal/platform.h"

#include <stdint.h>

// ---------------------------------------------------------------------------
// Depth-контракт (§1.2.6, формате normative 1:1)
// ---------------------------------------------------------------------------

/** Depth вне диапазона (без modulo-aliasing; нормативный sentinel). */
#define DEPTH_REJECT UINT32_C(0xFFFFFFFF)

/**
 * Engine-side depth key до backend-мэппинга (normative §1.2.6).
 * z<0 или вне [depth_min..depth_max] → DEPTH_REJECT; иначе z − depth_min.
 * УПАКОВКА ДОЛЖНА быть отвергнута или зажата ДО индексации ОТ; modulo
 * wrapping запрещён. depth_min ≤ depth_max (настраивается на инстанс
 * рендера). Унит-тесты — tests/render/contract_test.
 *
 * @param camera_z Камерная глубина в ЕДИНИЦАХ (raw int32; не 16.16).
 * @param depth_min Нижняя граница диапазона (uint32).
 * @param depth_max Верхняя граница диапазона (uint32, ≥ depth_min).
 * @return depth key или DEPTH_REJECT.
 */
uint32_t depth_key_from_camera_z(int32_t camera_z, uint32_t depth_min,
                                 uint32_t depth_max);

// ---------------------------------------------------------------------------
// Диагностика (Renderer safety contract: явные ошибки, без silent drop)
// ---------------------------------------------------------------------------

/**
 * Детерминированные диагностические коды рендера. Порядок — контракт;
 * расширение — только перед RENDER_DIAG_COUNT. Каждый «reject»-этап
 * CONVEYOR-а обязан оставлять последний код в state диагностики.
 */
typedef enum
{
    RENDER_DIAG_OK = 0,              /**< Успех. */
    RENDER_DIAG_DOUBLE_INIT,         /**< Render_Init при активном рендере. */
    RENDER_DIAG_NOT_INITIALIZED,     /**< Вызов до Init (EndFrame и пр.). */
    RENDER_DIAG_END_FRAME_BEFORE_INIT,/**< EndFrame до первого Init. */
    RENDER_DIAG_NULL_ARGUMENT,       /**< NULL-аргумент API рендера. */
    RENDER_DIAG_INVALID_SIZE,        /**< (w,h) вне [1..4096]. */
    RENDER_DIAG_NO_WINDOW,           /**< Окно/GL-контекст отсутствует. */
    RENDER_DIAG_BACKEND_FAIL,        /**< Отказ GL/backend (реализация). */
    RENDER_DIAG_INVALID_PROJECTION,  /**< Не выводятся near/far (proj). */
    RENDER_DIAG_TEXTURE_FAIL,        /**< Отказ создания текстуры (пул/арена). */
    RENDER_DIAG_INVALID_VERTEX_INDEX,/**< Индекс вне [0..vertex_count). */
    RENDER_DIAG_CLIP_REJECT,         /**< Отброс near/far-plane (счётчик). */
    RENDER_DIAG_CULL_REJECT,         /**< Отброс backface (счётчик). */
    RENDER_DIAG_DEPTH_REJECT,        /**< depth_key вне диапазона (счётчик). */
    RENDER_DIAG_COMMAND_OVERFLOW,    /**< 64 KiB буфер исчерпан (явная). */
    RENDER_DIAG_READBACK_FAIL,       /**< glReadPixels неудача. */
    RENDER_DIAG_END_FRAME_WITHOUT_BEGIN, /**< EndFrame без BeginFrame. */
    RENDER_DIAG_DRAW_OUTSIDE_FRAME,  /**< DrawMesh вне BeginFrame..EndFrame. */
    RENDER_DIAG_COUNT
} RenderDiag;

/** Диагностический счётчик рендера (детерминированный, тест-совместимый). */
typedef struct RenderDiagnostics
{
    uint32_t submitted;        /**< Треугольников добавлено в буфер кадра. */
    uint32_t clip_rejects;     /**< Отброшено near/far (конвейер CLIP). */
    uint32_t backface_rejects; /**< Отброшено backface (конвейер CULL). */
    uint32_t depth_rejects;    /**< Отброшено depth вне диапазона (SORT gate). */
    uint32_t last_code;        /**< Последний код RenderDiag (0 — OK). */
} RenderDiagnostics;

/**
 * Текущие счётчики диагностики рендера (для тестов/логов; без блокировок —
 * однопоточный драйвер). Счётчики НЕ сбрасываются автоматически (reset —
 * зона вызывающего; Render_Init обнуляет).
 *
 * @return Снимок счётчиков.
 */
RenderDiagnostics render_diagnostics(void);

// ---------------------------------------------------------------------------
// W0-конформанс: framebuffer readback + hash (§1.2.9, план недели 16)
// ---------------------------------------------------------------------------

/**
 * Чтение фреймбуфера 320×240 рабочего FBO и его детерминированный хэш.
 *
 * Метод сравнения (неделя 16, normative): framebuffer readback + hash, а
 * не «не крашится». RGBA8-снимок сжимается к PS1-родному 5:5:5 — 16-битное
 * слово на пиксель (R>>3|G>>3|B>>3): так LSB-джиттер драйверов не влияет
 * на 15-битный результат; хэширование — FNV-1a 64.
 * Отказ (GL/backend/не-окно) → 0 + RENDER_DIAG_READBACK_FAIL в счётчиках.
 *
 * @return Хэш кадра; 0 при отказе readback (NULL-результат недопустим).
 */
uint64_t Render_TestReadbackHash(void);

/**
 * Чтение фреймбуфера текущего FBO в буфер вызывающего (tests-only АПИ).
 *
 * @param dst_rgba8 Буфер ≥ 320×240×4 байт (RGBA8 построчно).
 * @param dst_size Размер буфера в байтах.
 * @return true при успехе; false при отказе (окно/GL/чтение).
 */
bool Render_TestReadbackPixels(uint8_t* dst_rgba8, uint32_t dst_size);

// ---------------------------------------------------------------------------
// Константы ядра (воедино для renderer.cpp и тестов)
// ---------------------------------------------------------------------------

/** Разрешение reference-фреймбуфера (§1.2.1 manifest: renderer). */
#define RENDER_FB_WIDTH 320u
#define RENDER_FB_HEIGHT 240u

/** Цвет очистки по умолчанию (неделя 13: smoke «окно (0,0,32)»). */
#define RENDER_CLEAR_DEFAULT_R 0u
#define RENDER_CLEAR_DEFAULT_G 0u
#define RENDER_CLEAR_DEFAULT_B 32u
