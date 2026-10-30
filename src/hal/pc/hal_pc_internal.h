// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/pc/hal_pc_internal.h
//
// ВНУТРЕННИЙ интерфейс PC-HAL (НЕ является публичным API, в примеры/тесты
// не включается). Связывает трансляционные единицы HAL:
//   - platform_pc.cpp    — владение окном/GL-контекстом (SDL);
//   - render_opengl.cpp  — GL-пакет (swap; расширение Спринтами 7–8);
//   - input_sdl.cpp      — открытие/закрытие геймпадов, состояние ввода.
//
// Нормативные источники: docs/plans/sprint_05_06.md (недели 9–12),
// docs/RetroEngine_Engineering_Documentation_v1.43.md §3.
#pragma once

#include "hal/platform.h"

#include <SDL3/SDL.h>

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
