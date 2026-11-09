// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/visual/rotating_cube_golden_test.cpp
//
// Визуальная регрессия демо rotating_cube — W1-эталонный кадр (норматив:
// docs/plans/sprint_17_18.md, неделя 36 — «скриншоты + видео»; PC-скриншот
// демки генерируется из этого эталона, паттерн hello_triangle S15–16).
//
// Кадр «default»: t = 0 — yaw всех кубов 0, позиции эталонной расстановки
// (кубы стоят на полу), фиксированная камера (0, 2, 6.5) pitch −0.15;
// ассеты через rc_load_assets (реальный пайплайна: 1 .mesh + 4 .tex),
// сцена через общий examples/rotating_cube/scene.h, отрисовка через
// game::RenderSystem. Golden НЕ дублирует константы сцены.
//
// Бюджет фолбэка — 128 px (класс hello_triangle/room12: silhouette-рёбра
// растеризатора — вне GL-спецификации). Headless-CI → GTEST_SKIP.
#include "examples/rotating_cube/scene.h"
#include "game/components.h"
#include "game/systems/render_system.h"
#include "core/memory/stack_allocator.h"
#include "hal/platform.h"
#include "hal/render_backend.h" // draw-call inventory
#include "render/renderer.h"    // RENDER_FB_*, RENDER_CLEAR_DEFAULT_*, RENDER_DIAG_OK
#include "tests/visual/golden_protocol.h"

#include <gtest/gtest.h>

#include <stdint.h>

namespace {

/// Бюджет фолбэка rotating_cube: класс hello_triangle (силуэтные рёбра).
const uint32_t RC_GOLDEN_BUDGET = 128u;

/// Полный startup (идентичен render_startup W0 — фиксированный кадр 320x240).
bool rc_render_startup()
{
    if (!Platform_Init()) {
        return false;
    }
    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        Platform_Shutdown();
        return false;
    }
    return true;
}

/// Реестр сцены — static storage (§5.4: бюджет ECS никогда не на CPU-стеке).
Registry s_scene_registry;

/// Арена кадра lit-копий вершин (как в main.cpp — §10).
alignas(16) static uint8_t s_frame_arena[64u * 1024u];
static StackAllocator s_frame_stack(s_frame_arena, sizeof(s_frame_arena));

} // namespace

//! Дефолтный кадр демо: камера (0, 2, 6.5) pitch −0.15, 4 куба на эталонной
//! расстановке (yaw=0, rest y), по текстуре-варианту на куб. Эталон:
//! tests/visual/reference/rotating_cube_default.bin (+ px-партнёр).
TEST(RotatingCubeGolden, DefaultSceneView)
{
    if (!golden::display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — rotating_cube golden пропущен (headless)";
    }

    // --- Ассеты: реальный пайплайн (FileSystem → loaders) в static-арене ----
    alignas(16) static uint8_t s_asset_arena[384u * 1024u]; // 1 mesh + 4 tex
    LinearAllocator assets(s_asset_arena, sizeof(s_asset_arena));

    MeshAsset* mesh = nullptr;
    TextureAsset* textures[rotating_cube::kTextureCount] = {nullptr, nullptr, nullptr, nullptr};
    uint32_t diag_code = 0u;
    ASSERT_TRUE(
        rotating_cube::rc_load_assets(RC_ASSET_DIR, &assets, &mesh, textures, &diag_code));

    // W1-инвентарь: 24 вершины (4/грань × 6), 12 треугольников на куб.
    ASSERT_EQ(mesh->vertex_count, 24u);
    ASSERT_EQ(mesh->index_count, 12u);
    for (int i = 0; i < rotating_cube::kTextureCount; ++i) {
        ASSERT_EQ(textures[i]->width, 256u);
        ASSERT_EQ(textures[i]->height, 256u);
    }

    // --- Рендер + upload 4 текстур ------------------------------------------
    ASSERT_TRUE(rc_render_startup());
    TextureHandle texture_handles[rotating_cube::kTextureCount] = {
        RENDER_TEXTURE_INVALID, RENDER_TEXTURE_INVALID, RENDER_TEXTURE_INVALID,
        RENDER_TEXTURE_INVALID};
    for (int i = 0; i < rotating_cube::kTextureCount; ++i) {
        texture_handles[i] = Render_CreateTexture(textures[i]->width, textures[i]->height,
                                                  textures[i]->pixels, textures[i]->palette);
        ASSERT_NE(texture_handles[i], RENDER_TEXTURE_INVALID);
    }

    // --- Сцена общего header'а + полный игровой путь сборки кадра ----------
    s_scene_registry.reset();
    const rotating_cube::SceneHandles handles = rotating_cube::rc_build_scene(s_scene_registry);
    ASSERT_NE(handles.camera, INVALID_ENTITY);
    for (int i = 0; i < 4; ++i) {
        ASSERT_NE(handles.cubes[i], INVALID_ENTITY);
    }

    game::RenderBanks banks;
    banks.meshes = mesh;
    banks.mesh_count = 1u;
    banks.textures = texture_handles;
    banks.texture_count = (uint32_t)rotating_cube::kTextureCount;
    banks.aspect = fixed_from_float((float)RENDER_FB_WIDTH / (float)RENDER_FB_HEIGHT);
    banks.lights = &rotating_cube::rc_light_bank(); // эталон демки (как main.cpp)
    banks.frame_scratch = &s_frame_stack;

    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
    game::RenderSystem::update(s_scene_registry, banks);
    Render_EndFrame();

    const uint64_t hash = golden::capture_readback();
    const RenderDiagnostics frame_diag = render_diagnostics();
    const uint32_t draw_calls = hal_render_backend_draw_call_count();
    Render_Shutdown();
    Platform_Shutdown();

    // --- Инвентарь кадра: W1-сцена без clip/depth-отказов -------------------
    // Кубы-башни: половина граней backface (куб — выпуклый замкнутый корпус;
    // невидимые грани кулятся — NORMA конвейера, в отличие от открытой
    // геометрии hello_triangle). Камера ниже плоскости верхних граней
    // верхних кубов: те backface; итог — 20 видимых / 28 куллящихся.
    const uint32_t rc_submitted = 20u;
    const uint32_t rc_backface = 28u;
    EXPECT_EQ(frame_diag.submitted, rc_submitted);
    EXPECT_EQ(frame_diag.clip_rejects, 0u);
    EXPECT_EQ(frame_diag.backface_rejects, rc_backface);
    EXPECT_EQ(frame_diag.depth_rejects, 0u);
    // Последний код конвейера — CULL_REJECT (куллинг был; не ошибка).
    EXPECT_EQ(frame_diag.last_code, (uint32_t)RENDER_DIAG_CULL_REJECT);
    EXPECT_EQ(draw_calls, rc_submitted + 1u); // + present-quad

    // Эталон дефолт-кадра W1 (бюджет класса hello_triangle).
    EXPECT_TRUE(golden::check("rotating_cube_default.bin", hash, RC_GOLDEN_BUDGET));
}
