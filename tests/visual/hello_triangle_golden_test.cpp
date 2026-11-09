// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/visual/hello_triangle_golden_test.cpp
//
// Визуальная регрессия примера hello_triangle (docs/plans/sprint_11_12.md,
// неделя 24 — «скриншот-эталон в tests/visual/ + unit визуальной регрессии»).
//
// Отличие от W0-кейсов: кадр строится ИГРОВЫМ СТЕКОМ целиком — ассеты через
// ht_load_assets (реальные .mesh/.tex пайплайна), сцена через общий
// examples/hello_triangle/scene.h, отрисовка через game::RenderSystem
// (Registry → Render_SetCamera/DrawMesh). Таким образом эталон кадра
// «default» гарантирует, что golden НЕ дублирует константы сцены (те же
// позы, что у main.cpp).
//
// Бюджет фолбэка — 128 px (как у room12): silhouette-рёбра треугольника
// (rasterizer tie-breaking — вне GL-спецификации), замер 2026-11-02.
// Среда: дисплей/GL-контекст; headless-CI → GTEST_SKIP.
#include "examples/hello_triangle/scene.h"
#include "game/components.h"
#include "game/systems/render_system.h"
#include "hal/platform.h"
#include "hal/render_backend.h" // draw-call inventory
#include "render/renderer.h"    // RENDER_FB_*, RENDER_CLEAR_DEFAULT_*, RENDER_DIAG_OK
#include "tests/visual/golden_protocol.h"

#include <gtest/gtest.h>

#include <stdio.h>

#include <stdint.h>

namespace {

/// Бюджет фолбэка hello_triangle: тот же класс рассинхрона, что у
/// room12 (силуэтные рёбра; tie-breaking растеризатора не специфицирован).
const uint32_t HT_GOLDEN_BUDGET = 128u;

/// Полный startup (идентичен render_startup W0 — фиксированный кадр 320x240).
bool ht_render_startup()
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

/// Полный shutdown (идемпотентен; камеру ставит RenderSystem в кадре).
void ht_render_shutdown()
{
    Render_Shutdown();
    Platform_Shutdown();
}

/// Реестр сцены — static storage (§5.4: бюджет ECS никогда не на CPU-стеке).
Registry s_scene_registry;

} // namespace

//! Дефолтный кадр примера: камера (0,0,5) yaw=pitch=0, треугольник в origin,
//! текстура пайплайна целиком. Эталон: tests/visual/reference/
//! hello_triangle_default.bin (+ px-партнёр для фолбэка).
TEST(HelloTriangleGolden, DefaultSceneView)
{
    if (!golden::display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — hello_triangle golden пропущен (headless)";
    }

    // --- Ассеты: реальный пайплайн (FileSystem → loaders) в static-арене ----
    alignas(16) static uint8_t s_asset_arena[256u * 1024u];
    LinearAllocator assets(s_asset_arena, sizeof(s_asset_arena));

    MeshAsset* mesh = nullptr;
    TextureAsset* texture_asset = nullptr;
    uint32_t diag_code = 0u;
    ASSERT_TRUE(
        hello_triangle::ht_load_assets(HT_ASSET_DIR, &assets, &mesh, &texture_asset, &diag_code));

    // Состав кейса недели 21: 3 общие вершины + 3 идентичные записи
    // (§8.3 норматива index_count % 3 == 0; решение — sprint_11_12.md).
    ASSERT_EQ(mesh->vertex_count, 3u);
    ASSERT_EQ(mesh->index_count, 3u);
    ASSERT_EQ(texture_asset->width, 256u);
    ASSERT_EQ(texture_asset->height, 256u);

    // --- Рендер + upload текстуры ------------------------------------------
    ASSERT_TRUE(ht_render_startup());
    const TextureHandle texture = Render_CreateTexture(
        texture_asset->width, texture_asset->height, texture_asset->pixels, texture_asset->palette);
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    // --- Сцена общего header'а + полный игровой путь сборки кадра ----------
    s_scene_registry.reset();
    const hello_triangle::SceneHandles handles = hello_triangle::ht_build_scene(s_scene_registry);
    ASSERT_NE(handles.camera, INVALID_ENTITY);
    ASSERT_NE(handles.triangle, INVALID_ENTITY);

    const TextureHandle texture_bank[1] = {texture};
    game::RenderBanks banks;
    banks.meshes = mesh;
    banks.mesh_count = 1u;
    banks.textures = texture_bank;
    banks.texture_count = 1u;
    banks.aspect = fixed_from_float((float)RENDER_FB_WIDTH / (float)RENDER_FB_HEIGHT);

    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
    game::RenderSystem::update(s_scene_registry, banks);
    Render_EndFrame();

    const uint64_t hash = golden::capture_readback();
    const RenderDiagnostics frame_diag = render_diagnostics();
    const uint32_t draw_calls = hal_render_backend_draw_call_count();
    ht_render_shutdown();

    // --- Инвентарь кадра: 3 записи поданы (без reject) + 1 present-quad -----
    EXPECT_EQ(frame_diag.submitted, mesh->index_count);
    EXPECT_EQ(frame_diag.clip_rejects, 0u);
    EXPECT_EQ(frame_diag.backface_rejects, 0u);
    EXPECT_EQ(frame_diag.depth_rejects, 0u);
    EXPECT_EQ(frame_diag.last_code, (uint32_t)RENDER_DIAG_OK);
    EXPECT_EQ(draw_calls, mesh->index_count + 1u);

    // Эталон дефолт-кадра (бюджет класса room12).
    EXPECT_TRUE(golden::check("hello_triangle_default.bin", hash, HT_GOLDEN_BUDGET));
}
