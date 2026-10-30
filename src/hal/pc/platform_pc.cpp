// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/pc/platform_pc.cpp
//
// HAL PC: окно, GL-контекст, события, тайминги (SDL3 + glad).
// Реализация §3.3 (engineering doc v1.43) и sprint_05_06.md, недели 9–10:
// SDL_Init(VIDEO|GAMEPAD) без SDL_INIT_TIMER (удалён в SDL3) → GL-атрибуты
// ДО создания окна → окно 1280×960 → CreateContext → MakeCurrent →
// gladLoadGL ДО любых GL-вызовов; Shutdown: DestroyContext → DestroyWindow
// → SDL_Quit. Любое частичное событие отказа в Init — полный откат.
#include "hal/pc/hal_pc_internal.h"

#include <SDL3/SDL.h>

#include <glad/gl.h>

/// Окно приложения (создаётся Init; NULL — платформа не инициализирована).
static SDL_Window* s_window = nullptr;

/// OpenGL-контекст окна (SDL_GLContext; NULL — контекст не создан).
static SDL_GLContext s_gl_context = nullptr;

/// Полный откат Init от стадии: дестроит всё, что успело создаться.
static void hal_platform_teardown(void)
{
    if (s_gl_context != nullptr) {
        SDL_GL_DestroyContext(s_gl_context);
        s_gl_context = nullptr;
    }
    if (s_window != nullptr) {
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
    }
    SDL_Quit();
}

bool Platform_Init(void)
{
    // SDL_INIT_TIMER не включаем: подсистема удалена в SDL3 (sprint_05_06).
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return false;
    }

    // GL-атрибуты устанавливаются строго ДО SDL_CreateWindow (неделя 9).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    s_window = SDL_CreateWindow("RetroEngine", 1280, 960,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (s_window == nullptr) {
        hal_platform_teardown();
        return false;
    }

    s_gl_context = SDL_GL_CreateContext(s_window);
    if (s_gl_context == nullptr) {
        hal_platform_teardown();
        return false;
    }
    if (!SDL_GL_MakeCurrent(s_window, s_gl_context)) {
        hal_platform_teardown();
        return false;
    }

    // Адреса GL загружаются до любых GL-вызовов (renderer — Спринты 7–8).
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        hal_platform_teardown();
        return false;
    }

    // Геймпады: отсутствие устройств — не сбой (ввод остаётся клавиатурным).
    hal_input_init();
    return true;
}

void Platform_Shutdown(void)
{
    hal_input_shutdown();

    if (s_gl_context != nullptr) {
        SDL_GL_DestroyContext(s_gl_context);
        s_gl_context = nullptr;
    }
    if (s_window != nullptr) {
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
    }
    SDL_Quit();
}

bool Platform_PumpEvents(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
    }
    return true;
}

void Platform_SwapBuffers(void)
{
    (void)hal_pc_swap_buffers();
}

void Platform_SleepMs(uint32_t ms)
{
    SDL_Delay(ms);
}

uint32_t Platform_GetTimeMs(void)
{
    return (uint32_t)SDL_GetTicks();
}

// --- Внутренние хэндлы (hal_pc_internal.h) -----------------------------------

SDL_Window* hal_pc_window(void)
{
    return s_window;
}
