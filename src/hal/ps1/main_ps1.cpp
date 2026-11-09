// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/main_ps1.cpp
//
// PS1-EXE демо MVP rotating_cube (веха M7, Спринты 17–18): тот же общий слой
// игры, что на PC (examples/rotating_cube/scene.h + game/systems/*), поверх
// HAL PS1 — GT3/G3 через GTE+GPU и файлы с CD-ROM.
//
// Управление:
//   - Крестик — прыжок всех кубов (demo-level гравитация, неделя 34);
//   - Select — debug overlay (FPS/TRI/RAM; неделя 36);
//   - START — выход.
//   (Камера фиксированная — эталонная расстановка W1; PlayerControl в сцене
//   нет, D-pad/стики не читаются.)
//
// Пути ассетов: ISO9660 8.3 на диске (mkpsxiso переименовывает
// cube.mesh → CUBE.MSH; texture_{0..3}.tex → TEX0..3.TEX; ";1" — номер
// версии; см. cmake/ps1-disc.xml).
//
// Память долгоживущего состояния — static storage (норматив §5.4: Registry и
// арены НИКОГДА не на CPU-стеке); арена кадра сбрасывается после EndFrame.
#include "examples/rotating_cube/scene.h"
#include "game/components.h"
#include "game/systems/collision_system.h"
#include "game/systems/input_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/render_system.h"
#include "game/systems/rotation_system.h"
#include "core/log/log.h"                 // Log_Message (репортинг сессии)
#include "core/memory/memory_profiler.h"  // MemoryProfiler (Спринт 15–16)
#include "core/memory/stack_allocator.h"  // StackAllocator (арена кадра)
#include "hal/ps1/hal_ps1_internal.h"     // GPU-pool / OT / VRAM-константы
#include "render/command_buffer.h"        // RenderTriCmd / RENDER_COMMAND_BUFFER_BYTES
#include "render/renderer.h"  // RENDER_FB_WIDTH/HEIGHT, render_command_buffer_used_bytes
#include "render/sort_key.h"  // SortRef
#include "render/texture.h"  // RENDER_TEXTURE_POOL_BYTES / RENDER_TEXTURE_MAX

#include <stdio.h>  // snprintf (overlay-строки)

// --- Пути PS1-диска (ISO9660 8.3; контент — те же ассеты, что на PC) -------
static const char* const kDiscMeshPath = "\\CUBE.MSH;1";
static const char* const kDiscTexPaths[rotating_cube::kTextureCount] = {
    "\\TEX0.TEX;1", "\\TEX1.TEX;1", "\\TEX2.TEX;1", "\\TEX3.TEX;1"};

// ---------------------------------------------------------------------------
// Static storage: реестр сцены и аллокаторы (§5.4)
// ---------------------------------------------------------------------------

Registry s_registry;  // бюджет ~234 KiB (§5.6) — static, не CPU-стек

// W1-ассеты: 1 mesh + 4 текстуры 256×256 CI8 — persistent-арена 384 KiB
// (как у PC-демо; PS1 RAM-бюджет 1.5 MiB — см. profiler ниже).
alignas(16) uint8_t s_asset_arena[384u * 1024u];
LinearAllocator s_assets(s_asset_arena, sizeof(s_asset_arena));

alignas(16) uint8_t s_frame_arena[64u * 1024u];  // арена кадра (контракт §10)
StackAllocator s_frame_stack(s_frame_arena, sizeof(s_frame_arena));

const fixed_t SIM_DT = (fixed_t)(FIXED_ONE / 60);  // детерминированный тик 60 Гц

// ---------------------------------------------------------------------------
// Callback'и MemoryProfiler (RAM / VRAM)
// ---------------------------------------------------------------------------

static size_t asset_arena_used(void*)
{
    return s_assets.used();
}
static size_t frame_arena_used(void*)
{
    return s_frame_stack.used();
}
static size_t render_command_used(void*)
{
    return render_command_buffer_used_bytes();
}

static size_t ps1_vram_used(void*)
{
    // Два 16bpp фреймбуфера 320×240 (A/B) + CLUT-строки + занятые texture pages.
    size_t used = (size_t)HAL_PS1_FB_WIDTH * HAL_PS1_FB_HEIGHT * 2u * 2u;
    used += (size_t)HAL_PS1_CLUT_SLOTS * 256u * 2u;
    for (uint32_t i = 0u; i < HAL_PS1_TEXTURE_MAX; ++i) {
        if (hal_ps1_backend_state.textures[i].in_use != 0u) {
            used += (size_t)256u * 256u;  // 8bpp texture page
        }
    }
    return used;
}

static void register_memory_profiler(void)
{
    memory_profiler_reset();
    memory_profiler_set_budget(MEMORY_PROFILER_RAM, 1572864u);  // 1.5 MiB
    memory_profiler_set_budget(MEMORY_PROFILER_VRAM, 819200u);  // 800 KiB

    memory_profiler_register_static("registry", MEMORY_PROFILER_RAM, sizeof(Registry));
    memory_profiler_register_source("asset_arena", MEMORY_PROFILER_RAM, sizeof(s_asset_arena),
                                    asset_arena_used, nullptr);
    memory_profiler_register_source("frame_arena", MEMORY_PROFILER_RAM, sizeof(s_frame_arena),
                                    frame_arena_used, nullptr);
    memory_profiler_register_static("render_command_buffer", MEMORY_PROFILER_RAM,
                                    RENDER_COMMAND_BUFFER_BYTES);
    memory_profiler_register_source("render_command_used", MEMORY_PROFILER_RAM,
                                    RENDER_COMMAND_BUFFER_BYTES, render_command_used, nullptr);
    memory_profiler_register_static(
        "render_sort_buffer", MEMORY_PROFILER_RAM,
        ((size_t)RENDER_COMMAND_BUFFER_BYTES / sizeof(RenderTriCmd)) * sizeof(SortRef));
    memory_profiler_register_static(
        "texture_pool", MEMORY_PROFILER_RAM,
        (size_t)RENDER_TEXTURE_POOL_BYTES + (size_t)RENDER_TEXTURE_MAX * 256u * sizeof(uint16_t));
    memory_profiler_register_static("gpu_packet_pool", MEMORY_PROFILER_RAM, HAL_PS1_GPU_POOL_SIZE);
    memory_profiler_register_static("ordering_table", MEMORY_PROFILER_RAM,
                                    (size_t)HAL_PS1_OT_LEN * sizeof(uint32_t));
    memory_profiler_register_static("ps1_backend_state", MEMORY_PROFILER_RAM,
                                    sizeof(HalPs1BackendState));
    memory_profiler_register_static("ps1_adapter_state", MEMORY_PROFILER_RAM,
                                    sizeof(HalPs1AdapterState));
    memory_profiler_register_source("ps1_vram", MEMORY_PROFILER_VRAM, 819200u, ps1_vram_used,
                                    nullptr);
}

// ---------------------------------------------------------------------------
// Контент и главный цикл PS1
// ---------------------------------------------------------------------------

/// Загрузка 1 .mesh + 4 .tex с CD-ROM в persistent-арену.
static bool load_content(MeshAsset** out_mesh,
                         TextureAsset* out_textures[rotating_cube::kTextureCount],
                         uint32_t* diag_code)
{
    *out_mesh = nullptr;

    FileData mesh_file = FileSystem_LoadFile(kDiscMeshPath);
    if (mesh_file.data != nullptr) {
        if (!mesh_validate_and_load(mesh_file.data, mesh_file.size, &s_assets, out_mesh,
                                    diag_code)) {
            *out_mesh = nullptr;
        }
        FileSystem_FreeFile(&mesh_file);  // арена владеет копией — файл можно освободить
    }
    for (int i = 0; i < rotating_cube::kTextureCount; ++i) {
        out_textures[i] = nullptr;
        FileData tex_file = FileSystem_LoadFile(kDiscTexPaths[i]);
        if (tex_file.data != nullptr) {
            if (!texture_validate_and_load(tex_file.data, tex_file.size, &s_assets,
                                           &out_textures[i], diag_code)) {
                out_textures[i] = nullptr;
            }
            FileSystem_FreeFile(&tex_file);
        }
    }
    return *out_mesh != nullptr;
}

int main()
{
    if (!Platform_Init()) {
        // Журнал на PS1 — no-op sink (FILE-sink — PC-only). Без экрана: код 1.
        return 1;
    }
    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        Log_Message(LOG_LEVEL_ERROR, "rotating_cube", "Render_Init(320x240) провалена");
        Platform_Shutdown();
        return 1;
    }

    // --- Контент: ассеты с CD-ROM -------------------------------------------
    MeshAsset* mesh = nullptr;
    TextureAsset* texture_assets[rotating_cube::kTextureCount] = {nullptr, nullptr, nullptr,
                                                                   nullptr};
    uint32_t diag_code = 0u;
    if (!load_content(&mesh, texture_assets, &diag_code)) {
        Log_Message(LOG_LEVEL_ERROR, "rotating_cube",
                    "ассеты CD недоступны (диагностика %u)", (unsigned)diag_code);
        Render_Shutdown();
        Platform_Shutdown();
        return 1;
    }
    for (int i = 0; i < rotating_cube::kTextureCount; ++i) {
        if (texture_assets[i] == nullptr) {
            Log_Message(LOG_LEVEL_ERROR, "rotating_cube",
                        "текстура %d недоступна (диагностика %u)", i, (unsigned)diag_code);
            Render_Shutdown();
            Platform_Shutdown();
            return 1;
        }
    }

    TextureHandle texture_handles[rotating_cube::kTextureCount] = {
        RENDER_TEXTURE_INVALID, RENDER_TEXTURE_INVALID, RENDER_TEXTURE_INVALID,
        RENDER_TEXTURE_INVALID};
    for (int i = 0; i < rotating_cube::kTextureCount; ++i) {
        texture_handles[i] = Render_CreateTexture(texture_assets[i]->width,
                                                  texture_assets[i]->height,
                                                  texture_assets[i]->pixels,
                                                  texture_assets[i]->palette);
        if (texture_handles[i] == RENDER_TEXTURE_INVALID) {
            Log_Message(LOG_LEVEL_ERROR, "rotating_cube",
                        "Render_CreateTexture недоступен (слот %d)", i);
            Render_Shutdown();
            Platform_Shutdown();
            return 1;
        }
    }

    const rotating_cube::SceneHandles handles = rotating_cube::rc_build_scene(s_registry);
    if (handles.camera == INVALID_ENTITY || handles.ground == INVALID_ENTITY ||
        handles.cubes[0] == INVALID_ENTITY || handles.cubes[3] == INVALID_ENTITY) {
        Log_Message(LOG_LEVEL_ERROR, "rotating_cube", "сцена не собрана (пул/слоты)");
        Render_Shutdown();
        Platform_Shutdown();
        return 1;
    }

    // Банки кадра: 1 mesh на 4 куба + 4 текстуры + эталонный свет W1
    // (vertex lighting — тот же банк, что на PC; lit-копии в арене кадра).
    game::RenderBanks banks;
    banks.meshes = mesh;
    banks.mesh_count = 1u;
    banks.textures = texture_handles;
    banks.texture_count = (uint32_t)rotating_cube::kTextureCount;
    banks.aspect = fixed_from_float((float)RENDER_FB_WIDTH / (float)RENDER_FB_HEIGHT);
    banks.lights = &rotating_cube::rc_light_bank();
    banks.frame_scratch = &s_frame_stack;

    // MemoryProfiler: регистрация источников и стартовая сверка бюджетов.
    register_memory_profiler();
    memory_profiler_print_stats();
    if (!memory_profiler_check_thresholds(90u, 90u)) {
        Log_Message(LOG_LEVEL_WARNING, "rotating_cube",
                    "MemoryProfiler: бюджеты RAM/VRAM близки к порогу на старте");
    }

    Log_Message(LOG_LEVEL_INFO, "rotating_cube",
                "PS1 W1-сцена готова (mesh: вершин %u, записей %u; текстур %u)",
                (unsigned)mesh->vertex_count, (unsigned)mesh->index_count,
                (unsigned)rotating_cube::kTextureCount);

    // --- Главный цикл §10 (PS1: кадр = vsync, тик 60 Гц по виртуальному времени)
    uint32_t last_time = Platform_GetTimeMs();
    uint64_t accumulator_60hz = 0;
    uint32_t total_frames = 0;

    // Debug-overlay (неделя 36): Select — тумблер; FPS — окно 1 с; TRI —
    // delta кумулятивного submitted; RAM — профилировщик.
    bool overlay_on = false;
    bool overlay_toggle_prev = false;
    uint32_t submitted_prev = 0u;
    uint32_t fps_display = 0u;
    uint32_t second_frames = 0u;
    uint64_t second_ms = 0u;

    bool quit = false;
    while (!quit && Platform_PumpEvents()) {  // false недостижимо на PS1
        const uint32_t now = Platform_GetTimeMs();
        const uint32_t elapsed_ms = now - last_time;  // uint32-разность: wrap-safe
        last_time = now;

        accumulator_60hz += (uint64_t)elapsed_ms * 60u;
        while (accumulator_60hz >= 1000u) {
            accumulator_60hz -= 1000u;

            // 1. Ввод (единая абстракция: геймпад на PS1).
            if (game::InputSystem::update(s_registry, SIM_DT)) {
                quit = true;
            }
            // 2. GameUpdate демо: вращение 90°/сек + гравитация/прыжок.
            const bool jump_requested = Input_IsPadButtonDown(0u, INPUT_PAD_CROSS);
            game::RotationSystem::update(s_registry, SIM_DT);
            rotating_cube::rc_gravity_and_jump(s_registry, SIM_DT, jump_requested);
            // 3. Коллизии (AABB intersect + response; пол и куб-куб).
            game::CollisionSystem::update(s_registry, SIM_DT);
            // 4. Движение (интеграция позиций по длине тика).
            game::MovementSystem::update(s_registry, SIM_DT);
        }

        // 5. Рендер КАЖДОГО кадра независимо от числа тиков.
        const bool overlay_toggle_now = Input_IsPadButtonDown(0u, INPUT_PAD_SELECT);
        if (overlay_toggle_now && !overlay_toggle_prev) {
            overlay_on = !overlay_on;
        }
        overlay_toggle_prev = overlay_toggle_now;

        const uint32_t submitted_now = render_diagnostics().submitted;
        const uint32_t frame_triangles = submitted_now - submitted_prev;
        submitted_prev = submitted_now;

        Render_BeginFrame();
        Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
        game::RenderSystem::update(s_registry, banks);
        if (overlay_on) {
            char line[32];
            snprintf(line, sizeof(line), "FPS %u", (unsigned)fps_display);
            Render_OverlayText(4, 4, 255, 255, 255, 1, line);
            snprintf(line, sizeof(line), "TRI %u", (unsigned)frame_triangles);
            Render_OverlayText(4, 11, 180, 255, 180, 1, line);
            snprintf(line, sizeof(line), "RAM %uK",
                     (unsigned)(memory_profiler_total_used(MEMORY_PROFILER_RAM) / 1024u));
            Render_OverlayText(4, 18, 180, 200, 255, 1, line);
        }
        Render_EndFrame();
        s_frame_stack.reset_frame();  // стадия 6 §10: кадровые траты — в ноль

        // Показ: DrawSync → VSync → flip (буфер A/B).
        Platform_SwapBuffers();
        ++total_frames;

        // FPS-окно overlay (1 с; завершённое окно — в строку FPS).
        ++second_frames;
        second_ms += elapsed_ms;
        if (second_ms >= 1000u) {
            fps_display = (uint32_t)((uint64_t)second_frames * 1000u / second_ms);
            second_frames = 0u;
            second_ms -= 1000u;
        }
    }

    Log_Message(LOG_LEVEL_INFO, "rotating_cube", "сессия завершена (кадров %u)",
                (unsigned)total_frames);

    Render_Shutdown();
    Platform_Shutdown();  // включает StopPAD.
    return 0;
}
