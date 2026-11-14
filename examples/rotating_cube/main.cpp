// SPDX-License-Identifier: MIT
// RetroPolyEngine — examples/rotating_cube
//
// Демо MVP, веха M7 «Rotating Cube» (docs/plans/sprint_17_18.md):
// W1-сцена — 4 видимых куба (48 текстурированных треугольников, CI8/256,
// Gouraud, transparency off), фиксированная камера (эталонная расстановка),
// RotationSystem (fixed 90°/сек yaw), AABB-коллизии + гравитация/прыжок
// (demo-level), главный цикл по контракту §10 (fixed accumulator 60 Гц),
// FPS-лог (bench-режим, стресс недели 36).
//
// Запуск (PC-сборки): build/<preset>/game/rotating_cube
//   - без аргументов — интерактив: Space (или Крестик геймпада) — прыжок
//     всех кубов, ESC — выход; камера фиксированная;
//   - `rotating_cube <секунды>` — bench-режим: то же, без ожидания ввода,
//     автоматический выход через N секунд, FPS-лог каждую секунду + итог
//     мин/макс/средн (протокол замера W1 — README примера).
//
// Память долгоживущего состояния — static storage (норматив §5.4: Registry
// и арены НИКОГДА не на CPU-стеке); кадровые матрицы — значения постоянного
// размера, арена кадра fine-но сбрасывается после Render_EndFrame.
#if !defined(PLATFORM_PC)
// PLATFORM_PS1: PC-цель демо (недели 33–34). PS1-порт сцены W1 — недели
// 35–36 (сверка PC/PS1 на эталонной сцене): собирается ps1-конфигурацией
// как заглушка без зависимости от HAL (примеры линкуют только retro_engine).
int main()
{
    return 0;
}
#else

#include "examples/rotating_cube/scene.h"
#include "game/components.h"
#include "game/systems/collision_system.h"
#include "game/systems/input_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/render_system.h"
#include "game/systems/rotation_system.h"
#include "core/log/log.h"                // Log_Message (репортинг сессии/замера)
#include "core/memory/memory_profiler.h" // debug-overlay: RAM usage (S17–18)
#include "hal/platform.h"                // RENDER_FB_*, RENDER_CLEAR_DEFAULT_*, diagnostics
#include "core/memory/stack_allocator.h" // StackAllocator (арена кадра)

#include <stdio.h>  // fprintf, printf, snprintf
#include <stdlib.h> // strtoul
#include <string.h> // strcmp

#include <retro_engine/version.h>

// ---------------------------------------------------------------------------
// Static storage: реестр сцены и аллокаторы (§5.4)
// ---------------------------------------------------------------------------

Registry s_registry; // бюджет ~234 KiB (§5.6) — static, не CPU-стек

alignas(16) uint8_t s_asset_arena[384u * 1024u]; // persistent: .mesh + 4 .tex
LinearAllocator s_assets(s_asset_arena, sizeof(s_asset_arena));

alignas(16) uint8_t s_frame_arena[64u * 1024u]; // арена кадра (контракт §10)
StackAllocator s_frame_stack(s_frame_arena, sizeof(s_frame_arena));

const fixed_t SIM_DT = (fixed_t)(FIXED_ONE / 60); // детерминированный тик 60 Гц

// ---------------------------------------------------------------------------
// Debug-overlay (S17–18, неделя 36): источники RAM + состояние тумблера
// ---------------------------------------------------------------------------

/// RAM-источник профилировщика: persistent-арена ассетов (used ≤ capacity).
size_t asset_arena_used_cb(void* user)
{
    return static_cast<LinearAllocator*>(user)->used();
}

/// RAM-источник профилировщика: арена кадра (used сбрасывается каждый кадр).
size_t frame_arena_used_cb(void* user)
{
    return static_cast<StackAllocator*>(user)->used();
}

/// Регистрация источников памяти демо (профилировщик — метрика overlay).
void register_memory_sources()
{
    memory_profiler_reset();
    memory_profiler_set_budget(MEMORY_PROFILER_RAM, 1536u * 1024u);
    memory_profiler_register_static("registry", MEMORY_PROFILER_RAM, sizeof(Registry));
    memory_profiler_register_source("asset_arena", MEMORY_PROFILER_RAM, sizeof(s_asset_arena),
                                    asset_arena_used_cb, &s_assets);
    memory_profiler_register_source("frame_arena", MEMORY_PROFILER_RAM, sizeof(s_frame_arena),
                                    frame_arena_used_cb, &s_frame_stack);
}

/// Итог init'а контента: указатели живы владением s_assets до Shutdown.
struct Content
{
    const MeshAsset* mesh = nullptr;
    TextureAsset* textures[rotating_cube::kTextureCount] = {nullptr, nullptr, nullptr, nullptr};
    TextureHandle texture_handles[rotating_cube::kTextureCount] = {RENDER_TEXTURE_INVALID,
                                                                   RENDER_TEXTURE_INVALID,
                                                                   RENDER_TEXTURE_INVALID,
                                                                   RENDER_TEXTURE_INVALID};
    rotating_cube::SceneHandles handles;
    game::RenderBanks banks;
};

namespace {

/// Platform_Init → Render_Init (ровно один раз, Lifecycle G9).
bool engine_init()
{
    if (!Platform_Init()) {
        // Лог ещё не поднят (журнал открывается внутри Platform_Init).
        fprintf(stderr, "rotating_cube: Platform_Init недоступен (нет дисплей-сессии?)\n");
        return false;
    }
    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        Log_Message(LOG_LEVEL_ERROR, "rotating_cube", "Render_Init(320x240) провалена");
        Platform_Shutdown();
        return false;
    }
    Log_Message(LOG_LEVEL_INFO, "rotating_cube", "рендер запущен (фреймбуфер %ux%u)",
                (unsigned)RENDER_FB_WIDTH, (unsigned)RENDER_FB_HEIGHT);
    return true;
}

/// Загрузка ассетов (1 mesh + 4 текстуры) → upload → сборка ECS-сцены.
bool content_init(Content* content)
{
    MeshAsset* mesh = nullptr;
    uint32_t diag_code = 0u;
    if (!rotating_cube::rc_load_assets(RC_ASSET_DIR, &s_assets, &mesh, content->textures,
                                       &diag_code)) {
        Log_Message(LOG_LEVEL_ERROR, "rotating_cube", "ассеты недоступны (диагностика %u)",
                    (unsigned)diag_code);
        return false;
    }
    content->mesh = mesh;

    for (int i = 0; i < rotating_cube::kTextureCount; ++i) {
        content->texture_handles[i] = Render_CreateTexture(content->textures[i]->width,
                                                           content->textures[i]->height,
                                                           content->textures[i]->pixels,
                                                           content->textures[i]->palette);
        if (content->texture_handles[i] == RENDER_TEXTURE_INVALID) {
            Log_Message(LOG_LEVEL_ERROR, "rotating_cube",
                        "Render_CreateTexture недоступен (слот %d)", i);
            return false;
        }
    }

    content->handles = rotating_cube::rc_build_scene(s_registry);
    if (content->handles.camera == INVALID_ENTITY ||
        content->handles.cubes[0] == INVALID_ENTITY ||
        content->handles.cubes[3] == INVALID_ENTITY) {
        Log_Message(LOG_LEVEL_ERROR, "rotating_cube", "сцена не собрана (пул/слоты переполнены)");
        return false;
    }

    content->banks.meshes = content->mesh;
    content->banks.mesh_count = 1u; // один .mesh на 4 куба (MeshRenderer::mesh_id 0)
    content->banks.texture_count = (uint32_t)rotating_cube::kTextureCount;
    content->banks.aspect = fixed_from_float((float)RENDER_FB_WIDTH / (float)RENDER_FB_HEIGHT);

    Log_Message(LOG_LEVEL_INFO, "rotating_cube",
                "контент готов: mesh (вершин %u, треугольников %u), текстур %u (256x256 CI8)",
                (unsigned)content->mesh->vertex_count, (unsigned)content->mesh->index_count,
                (unsigned)rotating_cube::kTextureCount);
    return true;
}

/// Debug-overlay кадра (S17–18): FPS / треугольники / RAM в пиксельных
/// координатах фреймбуфера (шрифт 3×5, масштаб 2 → 6×10 px, y вниз).
void draw_debug_overlay(uint32_t fps, uint32_t triangles, uint32_t ram_kib)
{
    char line[32];
    snprintf(line, sizeof(line), "FPS %u", (unsigned)fps);
    Render_OverlayText(4, 4, 255, 255, 255, 2, line);
    snprintf(line, sizeof(line), "TRI %u", (unsigned)triangles);
    Render_OverlayText(4, 22, 180, 255, 180, 2, line);
    snprintf(line, sizeof(line), "RAM %uK", (unsigned)ram_kib);
    Render_OverlayText(4, 40, 180, 200, 255, 2, line);
}

/// Полный кадр рендера (стадия 5 §10): BeginFrame → Clear → System → EndFrame.
void render_frame(const game::RenderBanks& banks, bool overlay_on, uint32_t overlay_fps,
                  uint32_t overlay_triangles, uint32_t overlay_ram_kib)
{
    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
    // RenderSystem сам ставит bind по MeshRenderer::texture_id каждого draw.
    game::RenderSystem::update(s_registry, banks);
    if (overlay_on) {
        draw_debug_overlay(overlay_fps, overlay_triangles, overlay_ram_kib);
    }
    Render_EndFrame();
    Platform_SwapBuffers(); // показ кадра: SDL_GL_SwapWindow (контракт platform.h)
    s_frame_stack.reset_frame(); // стадия 6 §10: кадровые траты — в ноль
}

/// Полный откат сессии (мышь не захватывается — камера фиксированная).
void engine_shutdown()
{
    Render_Shutdown();
    Platform_Shutdown(); // включает Log_Shutdown (файл журнала записан)
}

} // namespace

int main(int argc, char** argv)
{
    // Headless-проверка SDK/consumer smoke: не требует дисплея.
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("RetroPolyEngine %s (%s) rotating_cube\n", RETRO_ENGINE_VERSION_STRING,
               RETRO_ENGINE_PLATFORM_NAME);
        return 0;
    }

    // Режим запуска: N секунд bench либо интерактив.
    uint32_t bench_seconds = 0u;
    if (argc > 1) {
        bench_seconds = (uint32_t)strtoul(argv[1], nullptr, 10);
        if (bench_seconds == 0u) {
            fprintf(stderr, "rotating_cube: длительность bench должна быть > 0 секунд\n");
            return 1;
        }
    }

    if (!engine_init()) {
        return 1;
    }
    if (bench_seconds != 0u) {
        Log_Message(LOG_LEVEL_INFO, "rotating_cube", "bench-режим: %u с", (unsigned)bench_seconds);
    } else {
        Log_Message(LOG_LEVEL_INFO, "rotating_cube",
                    "режим интерактивный (Space/Крестик — прыжок, ESC — выход)");
    }

    Content content;
    if (!content_init(&content)) {
        engine_shutdown();
        return 1;
    }

    // Банк текстур кадра: 4 слота (MeshRenderer::texture_id 0..3 — по кубу);
    // свет W1 (неделя 35) — эталонный банк сцены, lit-копии в арене кадра.
    game::RenderBanks banks = content.banks;
    banks.textures = content.texture_handles;
    banks.lights = &rotating_cube::rc_light_bank();
    banks.frame_scratch = &s_frame_stack;

    // Профилировщик памяти демо → строка RAM debug-overlay (неделя 36).
    register_memory_sources();

    // --- Главный цикл §10 --------------------------------------------------
    // Учет FPS: роллинг секундного окна стеночного времени; лог каждую
    // целую секунду в bench-режиме; итог — мин/макс/средн (стресс недели 36).
    uint32_t last_time = Platform_GetTimeMs();
    uint64_t accumulator_60hz = 0; // elapsed_ms в долях 1/60-секундных тиков
    uint64_t bench_elapsed_ms = 0; // bench: стеночное «проработано»

    uint64_t total_frames = 0;
    uint64_t total_ms = 0;
    uint32_t second_frames = 0;
    uint64_t second_ms = 0;
    uint32_t second_window = 0; // нумерация завершённых секундных окон (лог)
    uint32_t fps_min = 0xFFFFFFFFu;
    uint32_t fps_max = 0u;
    uint32_t fps_display = 0u; // последнее завершённое секундное окно (overlay)

    // Debug-overlay: тумблер TAB (ПК) / SELECT (геймпад), старт — выключен
    // (эталонные кадры/bench — без overlay; golden-тест его не рисует).
    bool overlay_on = false;
    bool overlay_toggle_prev = false;
    uint32_t submitted_prev = 0u; // треугольников сцены в предыдущем кадре

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

            // 1. Ввод: ESC/START — запрос выхода (камера фиксированная —
            //    PlayerControl в сцене нет, движение камеры не читается).
            if (game::InputSystem::update(s_registry, SIM_DT)) {
                quit = true;
            }
            // 2. GameUpdate демо: вращение 90°/сек + гравитация/прыжок.
            //    Прыжок — удержание Space (ПК) / Крестика (геймпад pad 0).
            const bool jump_requested = Input_IsKeyDown(INPUT_KEY_SPACE) ||
                                        Input_IsPadButtonDown(0u, INPUT_PAD_CROSS);
            game::RotationSystem::update(s_registry, SIM_DT);
            rotating_cube::rc_gravity_and_jump(s_registry, SIM_DT, jump_requested);
            // 3. Коллизии: AABB intersect + response (выталкивание; пол —
            //    статика против кубов, куб-куб — первый в dense-порядке).
            game::CollisionSystem::update(s_registry, SIM_DT);
            // 4. Движение (интеграция позиций по длине тика).
            game::MovementSystem::update(s_registry, SIM_DT);
        }

        // 5. Рендер КАЖДОГО кадра независимо от числа тиков §-плана.
        // Debug-overlay (неделя 36): треугольники сцены — delta кумулятивного
        // счётчика submitted (сброс — только Render_Init); RAM — профилировщик;
        // FPS — последнее завершённое окно.
        const bool overlay_toggle_now = Input_IsKeyDown(INPUT_KEY_TAB) ||
                                        Input_IsPadButtonDown(0u, INPUT_PAD_SELECT);
        if (overlay_toggle_now && !overlay_toggle_prev) {
            overlay_on = !overlay_on;
        }
        overlay_toggle_prev = overlay_toggle_now;

        const uint32_t submitted_now = render_diagnostics().submitted;
        const uint32_t frame_triangles = submitted_now - submitted_prev;
        submitted_prev = submitted_now;
        const uint32_t ram_kib =
            (uint32_t)(memory_profiler_total_used(MEMORY_PROFILER_RAM) / 1024u);
        render_frame(banks, overlay_on, fps_display, frame_triangles, ram_kib);
        ++total_frames;
        ++second_frames;

        // FPS-учёт: роллинг секундного окна стеночного времени.
        second_ms += elapsed_ms;
        total_ms += elapsed_ms;
        bench_elapsed_ms += elapsed_ms;
        if (second_ms >= 1000u) {
            const uint32_t fps = (uint32_t)((uint64_t)second_frames * 1000u / second_ms);
            fps_display = fps;
            ++second_window;
            if (fps < fps_min) {
                fps_min = fps;
            }
            if (fps > fps_max) {
                fps_max = fps;
            }
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

    // --- Финал сессии: среднее (fps·100) + минимум/максимум окон -----------
    const uint64_t fps_x100 = (total_ms != 0u) ? (total_frames * 100000u / total_ms) : 0u;
    Log_Message(LOG_LEVEL_INFO, "rotating_cube",
                "итог: %u кадров за %u мс, среднее fps=%u.%02u, минимум %u, максимум %u",
                (unsigned)total_frames, (unsigned)total_ms, (unsigned)(fps_x100 / 100u),
                (unsigned)(fps_x100 % 100u),
                (fps_min == 0xFFFFFFFFu) ? 0u : (unsigned)fps_min, (unsigned)fps_max);

    engine_shutdown();
    return 0;
}
#endif // PLATFORM_PC
