// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/main_ps1.cpp
//
// PS1-EXE примера hello_triangle (веха M6, Спринты 15–16): тот же общий
// слой игры, что на PC (examples/hello_triangle/scene.h + game/systems/*),
// поверх HAL PS1 — GT3/G3 через GTE+GPU и файлы с CD-ROM.
//
// Управление:
//   - D-pad / левый стик — движение камеры в плоскости yaw;
//   - правый стик — yaw/pitch;
//   - START — выход.
//
// Пути ассетов: ISO9660 8.3 на диске (mkpsxiso переименовывает
// triangle.mesh → TRIANGLE.MSH; texture.tex → TEXTURE.TEX; ";1" — номер
// версии; см. cmake/ps1-disc.xml).
//
// Память долгоживущего состояния — static storage (норматив §5.4: Registry и
// арены НИКОГДА не на CPU-стеке); арена кадра сбрасывается после EndFrame.
#include "examples/hello_triangle/scene.h"
#include "game/components.h"
#include "game/systems/collision_system.h"
#include "game/systems/input_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/render_system.h"
#include "core/log/log.h"                 // Log_Message (репортинг сессии)
#include "core/memory/memory_profiler.h"  // MemoryProfiler (Спринт 15–16)
#include "core/memory/stack_allocator.h"  // StackAllocator (арена кадра)
#include "hal/ps1/hal_ps1_internal.h"     // GPU-pool / OT / VRAM-константы
#include "render/command_buffer.h"        // RenderTriCmd / RENDER_COMMAND_BUFFER_BYTES
#include "render/renderer.h"  // RENDER_FB_WIDTH/HEIGHT, render_command_buffer_used_bytes
#include "render/sort_key.h"  // SortRef
#include "render/texture.h"  // RENDER_TEXTURE_POOL_BYTES / RENDER_TEXTURE_MAX

// --- Пути PS1-диска (ISO9660 8.3; контент — те же ассеты, что на PC) -------
static const char* const kDiscMeshPath = "\\TRIANGLE.MSH;1";
static const char* const kDiscTexPath = "\\TEXTURE.TEX;1";

// ---------------------------------------------------------------------------
// Static storage: реестр сцены и аллокаторы (§5.4)
// ---------------------------------------------------------------------------

Registry s_registry;  // бюджет ~234 KiB (§5.6) — static, не CPU-стек

alignas(16) uint8_t s_asset_arena[256u * 1024u];  // persistent-ассеты (.mesh/.tex)
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
// Главный цикл PS1
// ---------------------------------------------------------------------------

int main()
{
    if (!Platform_Init()) {
        // Журнал на PS1 — no-op sink (FILE-sink — PC-only). Без экрана: код 1.
        return 1;
    }
    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "Render_Init(320x240) провалена");
        Platform_Shutdown();
        return 1;
    }

    // --- Контент: ассеты с CD-ROM -------------------------------------------
    MeshAsset* mesh = nullptr;
    TextureAsset* texture_asset = nullptr;
    uint32_t diag_code = 0u;
    FileData mesh_file = FileSystem_LoadFile(kDiscMeshPath);
    if (mesh_file.data != nullptr) {
        if (!mesh_validate_and_load(mesh_file.data, mesh_file.size, &s_assets, &mesh, &diag_code)) {
            mesh = nullptr;
        }
        FileSystem_FreeFile(&mesh_file);  // арена владеет копией — файл можно освободить
    }
    FileData tex_file = FileSystem_LoadFile(kDiscTexPath);
    if (tex_file.data != nullptr) {
        if (!texture_validate_and_load(tex_file.data, tex_file.size, &s_assets, &texture_asset,
                                       &diag_code)) {
            texture_asset = nullptr;
        }
        FileSystem_FreeFile(&tex_file);
    }
    if (mesh == nullptr || texture_asset == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "ассеты CD недоступны (диагностика %u)",
                    (unsigned)diag_code);
        Render_Shutdown();
        Platform_Shutdown();
        return 1;
    }

    const TextureHandle texture = Render_CreateTexture(
        texture_asset->width, texture_asset->height, texture_asset->pixels, texture_asset->palette);
    if (texture == RENDER_TEXTURE_INVALID) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "Render_CreateTexture недоступен");
        Render_Shutdown();
        Platform_Shutdown();
        return 1;
    }

    const hello_triangle::SceneHandles handles = hello_triangle::ht_build_scene(s_registry);
    if (handles.camera == INVALID_ENTITY || handles.triangle == INVALID_ENTITY) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "сцена не собрана (пул/слоты)");
        Render_Shutdown();
        Platform_Shutdown();
        return 1;
    }

    // Банк текстур кадра: единственный слот примера (MeshRenderer::texture_id 0).
    game::RenderBanks banks;
    banks.meshes = mesh;
    banks.mesh_count = 1u;
    const TextureHandle texture_bank[1] = {texture};
    banks.textures = texture_bank;
    banks.texture_count = 1u;
    banks.aspect = fixed_from_float((float)RENDER_FB_WIDTH / (float)RENDER_FB_HEIGHT);

    // MemoryProfiler: регистрация источников и стартовая сверка бюджетов.
    register_memory_profiler();
    memory_profiler_print_stats();
    if (!memory_profiler_check_thresholds(90u, 90u)) {
        Log_Message(LOG_LEVEL_WARNING, "hello_triangle",
                    "MemoryProfiler: бюджеты RAM/VRAM близки к порогу на старте");
    }

    Log_Message(LOG_LEVEL_INFO, "hello_triangle",
                "PS1-сцена готова (mesh: вершин %u, записей %u; текстура %ux%u)",
                (unsigned)mesh->vertex_count, (unsigned)mesh->index_count,
                (unsigned)texture_asset->width, (unsigned)texture_asset->height);

    // --- Главный цикл §10 (PS1: кадр = vsync, тик 60 Гц по виртуальному времени)
    uint32_t last_time = Platform_GetTimeMs();
    uint64_t accumulator_60hz = 0;
    uint32_t total_frames = 0;
    bool quit = false;

    while (!quit && Platform_PumpEvents()) {  // false недостижимо на PS1
        const uint32_t now = Platform_GetTimeMs();
        const uint32_t elapsed_ms = now - last_time;  // uint32-разность: wrap-safe
        last_time = now;

        accumulator_60hz += (uint64_t)elapsed_ms * 60u;
        while (accumulator_60hz >= 1000u) {
            accumulator_60hz -= 1000u;

            // 1. Ввод (единая абстракция: геймпад на PS1, клавиатура/мышь+геймпад на PC).
            if (game::InputSystem::update(s_registry, SIM_DT)) {
                quit = true;
            }
            // 2. GameUpdate — сцена без геймплея; фаза зарезервирована §10.
            // 3. Коллизии (no-op для сцен без Collider).
            game::CollisionSystem::update(s_registry, SIM_DT);
            // 4. Движение (интеграция позиций по длине тика).
            game::MovementSystem::update(s_registry, SIM_DT);
        }

        // 5. Рендер КАЖДОГО кадра независимо от числа тиков.
        Render_BeginFrame();
        Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
        game::RenderSystem::update(s_registry, banks);
        Render_EndFrame();
        s_frame_stack.reset_frame();  // стадия 6 §10: кадровые траты — в ноль

        // Показ: DrawSync → VSync → flip (буфер A/B).
        Platform_SwapBuffers();
        ++total_frames;
    }

    Log_Message(LOG_LEVEL_INFO, "hello_triangle", "сессия завершена (кадров %u)",
                (unsigned)total_frames);

    Render_Shutdown();
    Platform_Shutdown();  // включает StopPAD.
    return 0;
}
