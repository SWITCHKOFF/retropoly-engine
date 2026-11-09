// SPDX-License-Identifier: MIT
// RetroPolyEngine — examples/hello_triangle
//
// Веха M4 «Hello Triangle PC» (docs/plans/sprint_11_12.md): меш из .mesh,
// текстура из .tex, fps-камера WASD+мышь, главный цикл по контракту §10
// (fixed accumulator 60 Гц), FPS-лог, замер W1 (bench-режим).
//
// Запуск (PC-сборки): build/<preset>/game/hello_triangle
//   - без аргументов — интерактивно: мышь захвачена (relative), WASD/стрелки
//     — движение, Space/Shift — вверх/вниз, ESC — выход;
//   - `hello_triangle <секунды>` — bench-режим: фиксированная камера (мышь
//     не захватывается), автоматический выход через N секунд, FPS-лог
//     каждую секунду через Logger (протокол замера W1 — README примера).
//
// Память долгоживущего состояния — static storage (норматив §5.4: Registry
// и арены НИКОГДА не на CPU-стеке); кадровые матрицы — значения постоянного
// размера, арена кадра fine-но сбрасывается после Render_EndFrame.
#if !defined(PLATFORM_PC)
// PLATFORM_PS1: веха M4 — PC; PS1-порт примера — post-MVP (вслед за
// PS1-бэкендом рендера). PC-цель собирается ps1-конфигурацией как заглушка
// без зависимости от HAL (примеры линкуют только retro_engine).
int main()
{
    return 0;
}
#else

#include "examples/hello_triangle/scene.h"
#include "game/components.h"
#include "game/systems/collision_system.h"
#include "game/systems/input_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/render_system.h"
#include "core/log/log.h"      // Log_Message (репортинг сессии/замера)
#include "render/renderer.h"   // RENDER_FB_WIDTH/HEIGHT, RENDER_CLEAR_DEFAULT_*
#include "core/memory/stack_allocator.h" // StackAllocator (арена кадра)

#include <stdio.h>  // fprintf
#include <stdlib.h> // strtoul

// ---------------------------------------------------------------------------
// Static storage: реестр сцены и аллокаторы (§5.4)
// ---------------------------------------------------------------------------

Registry s_registry; // бюджет ~234 KiB (§5.6) — static, не CPU-стек

alignas(16) uint8_t s_asset_arena[256u * 1024u]; // persistent-ассеты (.mesh/.tex)
LinearAllocator s_assets(s_asset_arena, sizeof(s_asset_arena));

alignas(16) uint8_t s_frame_arena[64u * 1024u]; // арена кадра (контракт §10)
StackAllocator s_frame_stack(s_frame_arena, sizeof(s_frame_arena));

const fixed_t SIM_DT = (fixed_t)(FIXED_ONE / 60); // детерминированный тик 60 Гц

/// Итог init'а контента: указатели живы владением s_assets до Shutdown.
struct Content
{
    const MeshAsset* mesh = nullptr;
    TextureHandle texture = RENDER_TEXTURE_INVALID;
    hello_triangle::SceneHandles handles;
    game::RenderBanks banks;
};

namespace {

/// Platform_Init → Render_Init (ровно один раз, Lifecycle G9).
bool engine_init()
{
    if (!Platform_Init()) {
        // Лог ещё не поднят (журнал открывается внутри Platform_Init).
        fprintf(stderr, "hello_triangle: Platform_Init недоступен (нет дисплей-сессии?)\n");
        return false;
    }
    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "Render_Init(320x240) провалена");
        Platform_Shutdown();
        return false;
    }
    Log_Message(LOG_LEVEL_INFO, "hello_triangle", "рендер запущен (фреймбуфер %ux%u)",
                (unsigned)RENDER_FB_WIDTH, (unsigned)RENDER_FB_HEIGHT);
    return true;
}

/// Загрузка ассетов → upload текстуры → сборка ECS-сцены и банков.
bool content_init(Content* content)
{
    MeshAsset* mesh = nullptr;
    TextureAsset* texture_asset = nullptr;
    uint32_t diag_code = 0u;
    if (!hello_triangle::ht_load_assets(HT_ASSET_DIR, &s_assets, &mesh, &texture_asset,
                                        &diag_code)) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "ассеты недоступны (диагностика %u)",
                    (unsigned)diag_code);
        return false;
    }

    const TextureHandle texture = Render_CreateTexture(
        texture_asset->width, texture_asset->height, texture_asset->pixels, texture_asset->palette);
    if (texture == RENDER_TEXTURE_INVALID) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "Render_CreateTexture недоступен");
        return false;
    }

    const hello_triangle::SceneHandles handles = hello_triangle::ht_build_scene(s_registry);
    if (handles.camera == INVALID_ENTITY || handles.triangle == INVALID_ENTITY) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "сцена не собрана (пул/слоты переполнены)");
        return false;
    }

    content->mesh = mesh;
    content->texture = texture;
    content->handles = handles;
    content->banks.meshes = mesh;
    content->banks.mesh_count = 1u;
    content->banks.texture_count = 1u; // массив текстур — в скопе run()
    content->banks.aspect = fixed_from_float((float)RENDER_FB_WIDTH / (float)RENDER_FB_HEIGHT);

    Log_Message(LOG_LEVEL_INFO, "hello_triangle",
                "контент готов: mesh (вершин %u, записей %u), текстура %ux%u",
                (unsigned)mesh->vertex_count, (unsigned)mesh->index_count,
                (unsigned)texture_asset->width, (unsigned)texture_asset->height);
    return true;
}

/// Полный кадр рендера (стадия 5 §10): BeginFrame → Clear → System → EndFrame.
void render_frame(const game::RenderBanks& banks)
{
    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
    // RenderSystem сам ставит bind по MeshRenderer::texture_id каждого draw.
    game::RenderSystem::update(s_registry, banks);
    Render_EndFrame();
    Platform_SwapBuffers(); // показ кадра: SDL_GL_SwapWindow (контракт platform.h)
    s_frame_stack.reset_frame(); // стадия 6 §10: кадровые траты — в ноль
}

/// Полный откат сессии (мышь — вернуть курсор ДО разрушения окна).
void engine_shutdown(bool mouse_was_grabbed)
{
    if (mouse_was_grabbed) {
        Input_SetMouseRelativeMode(false);
    }
    Render_Shutdown();
    Platform_Shutdown(); // включает Log_Shutdown (файл журнала записан)
}

} // namespace

int main(int argc, char** argv)
{
    // Режим запуска: N секунд bench либо интерактив.
    uint32_t bench_seconds = 0u;
    if (argc > 1) {
        bench_seconds = (uint32_t)strtoul(argv[1], nullptr, 10);
        if (bench_seconds == 0u) {
            fprintf(stderr, "hello_triangle: длительность bench должна быть > 0 секунд\n");
            return 1;
        }
    }

    if (!engine_init()) {
        return 1;
    }
    if (bench_seconds != 0u) {
        Log_Message(LOG_LEVEL_INFO, "hello_triangle", "bench-режим: %u с, камера фиксирована",
                    (unsigned)bench_seconds);
    } else {
        Log_Message(LOG_LEVEL_INFO, "hello_triangle", "режим интерактивный (мышь захватывается)");
    }

    Content content;
    if (!content_init(&content)) {
        engine_shutdown(false);
        return 1;
    }

    // Банк текстур кадра: единственный слот примера (MeshRenderer::texture_id 0).
    const TextureHandle texture_bank[1] = {content.texture};
    game::RenderBanks banks = content.banks;
    banks.textures = texture_bank;

    // Мышь: относительный режим только интерактивно (bench — фиксированная
    // камера; накопитель платформы молчит вне relative, дельты всегда 0).
    const bool mouse_grabbed = (bench_seconds == 0u) && Input_SetMouseRelativeMode(true);
    if (bench_seconds == 0u && !mouse_grabbed) {
        Log_Message(LOG_LEVEL_WARNING, "hello_triangle",
                    "grab мыши недоступен — управление только клавиатурой");
    }

    // --- Главный цикл §10 --------------------------------------------------
    // Учет FPS: роллинг секундного окна стеночного времени; лог каждую
    // целую секунду в bench-режиме (и финальное среднее в конце сессии).
    uint32_t last_time = Platform_GetTimeMs();
    uint64_t accumulator_60hz = 0; // elapsed_ms в долях 1/60-секундных тиков
    uint64_t bench_elapsed_ms = 0; // bench: стеночное «проработано»

    uint64_t total_frames = 0;
    uint64_t total_ms = 0;
    uint32_t second_frames = 0;
    uint64_t second_ms = 0;
    uint32_t second_window = 0; // нумерация завершённых секундных окон (лог)

    bool quit = false;
    while (!quit && Platform_PumpEvents()) { // false — окно закрыто
        const uint32_t now = Platform_GetTimeMs();
        const uint32_t elapsed_ms = now - last_time; // uint32-разность: wrap-safe
        last_time = now;

        // Планирование тиков: wall-clock ТОЛЬКО планирует (§10), геймплей не
        // видит стеночную дельту.
        accumulator_60hz += (uint64_t)elapsed_ms * 60u;
        while (accumulator_60hz >= 1000u) {
            accumulator_60hz -= 1000u;

            // 1. Ввод (consume дельт мыши; ESC — запрос выхода).
            if (game::InputSystem::update(s_registry, SIM_DT)) {
                quit = true;
            }
            // 2. GameUpdate — сцена без геймплея; фаза зарезервирована §10.
            // 3. Коллизии (S13–14: no-op для сцен без Collider).
            game::CollisionSystem::update(s_registry, SIM_DT);
            // 4. Движение (интеграция позиций по длине тика).
            game::MovementSystem::update(s_registry, SIM_DT);
        }

        // 5. Рендер КАЖДОГО кадра независимо от числа тиков §-плана.
        render_frame(banks);
        ++total_frames;
        ++second_frames;

        // FPS-учёт: роллинг секундного окна стеночного времени.
        second_ms += elapsed_ms;
        total_ms += elapsed_ms;
        bench_elapsed_ms += elapsed_ms;
        if (second_ms >= 1000u) {
            const uint32_t fps = (uint32_t)((uint64_t)second_frames * 1000u / second_ms);
            ++second_window;
            if (bench_seconds != 0u) {
                Log_Message(LOG_LEVEL_INFO, "bench", "fps=%u (окно %u)", fps,
                            (unsigned)second_window);
            }
            second_ms -= 1000u;
            second_frames = 0u;
        }

        if (bench_seconds != 0u && bench_elapsed_ms >= (uint64_t)bench_seconds * 1000u) {
            break; // bench timeout: замер окончен
        }
    }

    // --- Финал сессии: среднее по всему окну (fps·100 для точности) --------
    const uint64_t fps_x100 = (total_ms != 0u) ? (total_frames * 100000u / total_ms) : 0u;
    Log_Message(LOG_LEVEL_INFO, "hello_triangle", "итог: %u кадров за %u мс, среднее fps=%u.%02u",
                (unsigned)total_frames, (unsigned)total_ms, (unsigned)(fps_x100 / 100u),
                (unsigned)(fps_x100 % 100u));

    engine_shutdown(mouse_grabbed);
    return 0;
}
#endif // PLATFORM_PC
