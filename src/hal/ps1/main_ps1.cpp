// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/main_ps1.cpp
//
// PS1-EXE примера hello_triangle (веха M4-port, Спринты 13–14): тот же общий
// слой игры, что на PC (examples/hello_triangle/scene.h + game/systems/*),
// но поверх HAL PS1 — GT3/G3 через GTE+GPU и файлы с CD-ROM.
//
// Расхождения с PC-рантаймом (PS1 не имеет мыши/клавиатуры/ESC):
//   - управление треугольником: D-pad — x/y, лицевые кнопки — z
//     (Крестик — к камере, Треугольник — от камеры; скорость kPadMoveSpeed
//     units/секунду, интеграция в MovementSystem — фазы §10);
//   - выход: START (прямой канал Input_IsPadButtonDown — ESC-канала нет);
//   - пути ассетов: ISO9660 8.3 на диске (mkpsxiso переименовывает
//     triangle.mesh → TRIANGLE.MSH; texture.tex → TEXTURE.TEX; ";1" —
//     номер версии; см. cmake/ps1-disc.xml).
//
// Память долгоживущего состояния — static storage (норматив §5.4: Registry и
// арены НИКОГДА не на CPU-стеке); арена кадра сбрасывается после EndFrame.
#include "examples/hello_triangle/scene.h"
#include "game/components.h"
#include "game/systems/collision_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/render_system.h"
#include "core/log/log.h"      // Log_Message (репортинг сессии)
#include "render/renderer.h"   // RENDER_FB_WIDTH/HEIGHT, RENDER_CLEAR_DEFAULT_*
#include "core/memory/stack_allocator.h" // StackAllocator (арена кадра)

// --- Пути PS1-диска (ISO9660 8.3; контент — те же ассеты, что на PC) -------
static const char* const kDiscMeshPath = "\\TRIANGLE.MSH;1";
static const char* const kDiscTexPath = "\\TEXTURE.TEX;1";

// ---------------------------------------------------------------------------
// Static storage: реестр сцены и аллокаторы (§5.4)
// ---------------------------------------------------------------------------

Registry s_registry; // бюджет ~234 KiB (§5.6) — static, не CPU-стек

alignas(16) uint8_t s_asset_arena[256u * 1024u]; // persistent-ассеты (.mesh/.tex)
LinearAllocator s_assets(s_asset_arena, sizeof(s_asset_arena));

alignas(16) uint8_t s_frame_arena[64u * 1024u]; // арена кадра (контракт §10)
StackAllocator s_frame_stack(s_frame_arena, sizeof(s_frame_arena));

const fixed_t SIM_DT = (fixed_t)(FIXED_ONE / 60); // детерминированный тик 60 Гц

/** Скорость управления треугольником от пада (units/секунду; 16.16). */
const fixed_t kPadMoveSpeed = fixed_from_int(3);

// ---------------------------------------------------------------------------
// Управление PS1-пада (замена InputSystem: мыши/клавиатуры на PS1 нет)
// ---------------------------------------------------------------------------

/// D-pad → оси x/y, лицевые кнопки → z. Потребитель ходит в Velocity
/// треугольника, позиции интегрирует MovementSystem (разделение фаз §10).
static void ps1_pad_control(Registry& registry, Entity triangle)
{
    Velocity* velocity = registry.get_component<Velocity>(triangle);
    if (velocity == nullptr) {
        return; // Треугольник без Velocity — (сцена гарантирует наличие).
    }
    fixed_t dx = 0;
    fixed_t dy = 0;
    fixed_t dz = 0;
    if (Input_IsPadButtonDown(0, INPUT_PAD_LEFT)) {
        dx = fixed_sub(dx, kPadMoveSpeed);
    }
    if (Input_IsPadButtonDown(0, INPUT_PAD_RIGHT)) {
        dx = fixed_add(dx, kPadMoveSpeed);
    }
    if (Input_IsPadButtonDown(0, INPUT_PAD_UP)) {
        dy = fixed_sub(dy, kPadMoveSpeed); // экранная ось y вниз: ↑ = −y
    }
    if (Input_IsPadButtonDown(0, INPUT_PAD_DOWN)) {
        dy = fixed_add(dy, kPadMoveSpeed);
    }
    if (Input_IsPadButtonDown(0, INPUT_PAD_CROSS)) {
        dz = fixed_sub(dz, kPadMoveSpeed); // к камере (−z мира)
    }
    if (Input_IsPadButtonDown(0, INPUT_PAD_TRIANGLE)) {
        dz = fixed_add(dz, kPadMoveSpeed); // от камеры (+z)
    }
    velocity->value = vec3_make(dx, dy, dz);
}

// ---------------------------------------------------------------------------
// Инициализация контента (с CD-ROM)
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
        if (!mesh_validate_and_load(mesh_file.data, mesh_file.size, &s_assets, &mesh,
                                    &diag_code)) {
            mesh = nullptr;
        }
        FileSystem_FreeFile(&mesh_file); // арена владеет копией — файл можно освободить
    }
    FileData tex_file = FileSystem_LoadFile(kDiscTexPath);
    if (tex_file.data != nullptr) {
        if (!texture_validate_and_load(tex_file.data, tex_file.size, &s_assets,
                                       &texture_asset, &diag_code)) {
            texture_asset = nullptr;
        }
        FileSystem_FreeFile(&tex_file);
    }
    if (mesh == nullptr || texture_asset == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle",
                    "ассеты CD недоступны (диагностика %u)", (unsigned)diag_code);
        Render_Shutdown();
        Platform_Shutdown();
        return 1;
    }

    const TextureHandle texture = Render_CreateTexture(
        texture_asset->width, texture_asset->height, texture_asset->pixels,
        texture_asset->palette);
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

    Log_Message(LOG_LEVEL_INFO, "hello_triangle",
                "PS1-сцена готова (mesh: вершин %u, записей %u; текстура %ux%u)",
                (unsigned)mesh->vertex_count, (unsigned)mesh->index_count,
                (unsigned)texture_asset->width, (unsigned)texture_asset->height);

    // --- Главный цикл §10 (PS1: кадр = vsync, тик 60 Гц по виртуальному времени)
    uint32_t last_time = Platform_GetTimeMs();
    uint64_t accumulator_60hz = 0;
    uint32_t total_frames = 0;
    bool quit = false;

    while (!quit && Platform_PumpEvents()) { // false недостижимо на PS1
        const uint32_t now = Platform_GetTimeMs();
        const uint32_t elapsed_ms = now - last_time; // uint32-разность: wrap-safe
        last_time = now;

        accumulator_60hz += (uint64_t)elapsed_ms * 60u;
        while (accumulator_60hz >= 1000u) {
            accumulator_60hz -= 1000u;

            // 1. Ввод PS1: START — выход; D-pad/лицевые → Velocity треугольника.
            if (Input_IsPadButtonDown(0, INPUT_PAD_START)) {
                quit = true;
            }
            ps1_pad_control(s_registry, handles.triangle);

            // 2. GameUpdate — сцена без геймплея; фаза зарезервирована §10.
            // 3. Коллизии (no-op для сцен без Collider).
            game::CollisionSystem::update(s_registry, SIM_DT);
            // 4. Движение (интеграция позиций по длине тика; камера статична).
            game::MovementSystem::update(s_registry, SIM_DT);
        }

        // 5. Рендер КАЖДОГО кадра независимо от числа тиков.
        Render_BeginFrame();
        Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G,
                     RENDER_CLEAR_DEFAULT_B);
        game::RenderSystem::update(s_registry, banks);
        Render_EndFrame();
        s_frame_stack.reset_frame(); // стадия 6 §10: кадровые траты — в ноль

        // Показ: DrawSync → VSync → flip (буфер A/B).
        Platform_SwapBuffers();
        ++total_frames;
    }

    Log_Message(LOG_LEVEL_INFO, "hello_triangle", "сессия завершена (кадров %u)",
                (unsigned)total_frames);

    Render_Shutdown();
    Platform_Shutdown(); // включает StopPAD.
    return 0;
}
