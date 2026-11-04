// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/pc/hal_pc_internal.h
//
// ВНУТРЕННИЙ интерфейс PC-HAL (НЕ является публичным API, в примеры/тесты
// не включается). Связывает трансляционные единицы HAL:
//   - platform_pc.cpp    — владение окном/GL-контекстом (SDL);
//   - render_opengl.cpp  — GL-пакет (swap; расширение Спринтами 7–8);
//   - input_sdl.cpp      — открытие/закрытие геймпадов, состояние ввода
//                          (мышь/накопление дельт — Спринт 11–12).
//
// Нормативные источники: docs/plans/sprint_05_06.md (недели 9–12),
// docs/RetroEngine_Engineering_Documentation_v1.43.md §3;
// мышь — Спринт 11–12 (неделя 23, docs/plans/sprint_11_12.md).
#pragma once

#include "hal/platform.h"

#include <SDL3/SDL.h>

#include <stdbool.h>

/// Максимум геймпадов HAL (PS1-паритет: PS1 обслуживает 2 контроллера).
#define HAL_PC_MAX_PADS 2u

/**
 * Хэндл окна, созданного Platform_Init (или NULL до Init/после Shutdown).
 * Определён в platform_pc.cpp; читается GL-слоем (render_opengl.cpp).
 */
SDL_Window* hal_pc_window(void);

/**
 * Показ буфера отрисовки на окно (SDL_GL_SwapWindow).
 *
 * @return true — окно есть, swap отправлен; false — окна нет (безопасно).
 */
bool hal_pc_swap_buffers(void);

/**
 * Открытие доступных геймпадов (SDL_GetGamepads → SDL_OpenGamepad по
 * SDL_Gamepad API). Вызывается из Platform_Init после SDL_Init; отсутствие
 * геймпадов НЕ является отказом (ввод остаётся клавиатурным).
 *
 * @return true (ввод недоступен лишь диагностически, не как сбой).
 */
bool hal_input_init(void);

/**
 * Закрытие открытых геймпадов (SDL_CloseGamepad; SDL_free списков SDL).
 * Вызывается из Platform_Shutdown до SDL_Quit. Идемпотентна.
 */
void hal_input_shutdown(void);

/**
 * Прокачка одного SDL-события через подсистему ввода (накопление
 * относительных мышиных дельт). Вызывается из Platform_PumpEvents для
 * каждого события. Отдельно от SDL_EVENT_QUIT (тот отдаёт сам PumpEvents).
 *
 * @param event Событие SDL (не NULL).
 */
void hal_input_pump_event(const SDL_Event* event);

/**
 * Включение/выключение относительного режима мыши (SDL3
 * SDL_SetWindowRelativeMouseMode на окне HAL). Идемпотентна.
 *
 * @param window Окно SDL (NULL — no-fail-out false).
 * @param enable true — включить; false — выключить (и сбросить счётчики).
 * @return true при успехе.
 */
bool hal_input_set_mouse_relative_mode(SDL_Window* window, bool enable);
