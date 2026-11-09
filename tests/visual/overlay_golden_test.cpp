// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/visual/overlay_golden_test.cpp
//
// Визуальная регрессия debug-overlay (docs/plans/sprint_17_18.md, неделя 36:
// «Debug overlay: FPS, RAM usage, triangle count»): шрифт 3×5, сборка
// экранных треугольников, flush поверх сцены — на фиксированных строках
// (значения не зависят от времени: эталон детерминирован).
//
// Кадр: чистое поле очистки + 3 строки overlay (FPS/TRI/RAM) масштабом 2 —
// hash-эталон `overlay_text_default.bin` (+ px-партнёр). Headless → GTEST_SKIP.
#include "hal/platform.h"
#include "hal/render_backend.h"
#include "render/renderer.h"
#include "tests/visual/golden_protocol.h"

#include <gtest/gtest.h>

#include <stdint.h>

namespace {

/// Startup (идентичен render_startup W0 — фиксированный кадр 320x240).
bool ov_render_startup()
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

} // namespace

//! Overlay-кадр: строки как у демо (FPS/TRI/RAM), фиксированные значения.
TEST(OverlayGolden, DebugTextLines)
{
    if (!golden::display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — overlay golden пропущен (headless)";
    }

    ASSERT_TRUE(ov_render_startup());

    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
    // Те же строки/цвета/позиции, что draw_debug_overlay демо (проверка
    // реального шрифта и триангл-пути, а не «тестовой заглушки текста»).
    Render_OverlayText(4, 4, 255, 255, 255, 2, "FPS 60");
    Render_OverlayText(4, 22, 180, 255, 180, 2, "TRI 48");
    Render_OverlayText(4, 40, 180, 200, 255, 2, "RAM 512K");
    Render_EndFrame();

    const uint64_t hash = golden::capture_readback();
    const RenderDiagnostics diag = render_diagnostics();
    Render_Shutdown();
    Platform_Shutdown();

    // Сцена пуста: конвейер не подаёт scene-треугольников; overlay идёт
    // отдельным путём (draw_overlay), последний код — OK.
    EXPECT_EQ(diag.submitted, 0u);
    EXPECT_EQ(diag.last_code, (uint32_t)RENDER_DIAG_OK);

    // Эталон: строгий hash (overlay-геометрия целочисленная — rasterizer
    // tie-breaking не задействован на краях 1-px quad'ов при целых
    // экранных координатах; при рассинхроне — нулевой бюджет).
    EXPECT_TRUE(golden::check("overlay_text_default.bin", hash, 0u));
}
