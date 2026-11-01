// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/visual/render_golden_test.cpp
//
// W0-конформанс рендера (docs/plans/sprint_07_08.md, неделя 16).
//
// Метод сравнения (normative): framebuffer readback + hash — НЕ «не
// крашится». Хэш — Render_TestReadbackHash() (FNV-1a 64 над 15-битным
// сжатием RGBA-снимка: гашение LSB-джиттера драйверов — §1.2.9).
//
// Эталоны: 8-байтные big-endian FNV-хэши в tests/visual/reference/*.bin
// (зафиксированы в репо). Регенерация эталонов ПОСЛЕ НАМЕРЕННОГО изменения
// рендера: env RENDER_GOLDEN_REBASE=1 (только осознанный rebase —
// никогда не используется для «подгонки» падающего теста).
//
// Среда: дисплей/GL-контекст (см. hal_test.cpp: headless-CI → GTEST_SKIP).
// Gate: пресет pc-debug-render (BUILD_RENDER_INTEGRATION_TESTS=ON).
#include "hal/platform.h"
#include "render/renderer.h"
#include "tests/visual/w0_scene.h"

#include <gtest/gtest.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>

// LSan-суппрессии Mesa/GL (как в hal_test.cpp — системный драйвер).
extern "C" const char* __lsan_default_suppressions(void)
{
    return "leak:libgallium\n"
           "leak:libGLX\n"
           "leak:libGL\n";
}

namespace {

bool display_available()
{
#if defined(__linux__)
    const char* x11 = getenv("DISPLAY");
    const char* wayland = getenv("WAYLAND_DISPLAY");
    return (x11 != nullptr && x11[0] != '\0') ||
           (wayland != nullptr && wayland[0] != '\0');
#else
    return true;
#endif
}

/// Полный startup рендера под фиксированную W0-камеру (320×240).
bool render_startup()
{
    if (!Platform_Init()) {
        return false;
    }
    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        Platform_Shutdown();
        return false;
    }
    Mat4 view;
    Mat4 proj;
    w0_camera(&view, &proj);
    Render_SetCamera(&view, &proj);
    return true;
}

/// Полный shutdown рендера (идемпотентен).
void render_shutdown()
{
    Render_Shutdown();
    Platform_Shutdown();
}

/// Создание CI8-чекер-текстуры через Render_CreateTexture (пул ядра GL).
TextureHandle make_checker_texture()
{
    uint32_t tex_w = 0u;
    uint32_t tex_h = 0u;
    return Render_CreateTexture(64u, 64u,
                                w0_make_checker_pixels(&tex_w, &tex_h),
                                w0_make_checker_palette());
}

/// Результат одного кадра (снимок диагностики ДО Shutdown — там zeroing).
struct FrameResult
{
    uint64_t hash;
    RenderDiagnostics diag;
};

/**
 * Кадр с одной меш-подачей: BeginFrame → Clear(0,0,32) → Bind →
 * DrawMesh → EndFrame → hash; диагностика снимается до shutdown.
 */
FrameResult render_frame(const Vertex* vertices, uint32_t vertex_count,
                         const Triangle* indices, uint32_t index_count,
                         TextureHandle texture)
{
    // Model = identity (контракт DrawMesh: NULL-аргумент — явная
    // диагностика NULL_ARGUMENT, «NULL = identity» пути нет).
    Mat4 identity;
    mat4_identity(&identity);
    FrameResult result;
    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G,
                 RENDER_CLEAR_DEFAULT_B);
    Render_BindTexture(texture);
    Render_DrawMesh(vertices, vertex_count, indices, index_count, &identity);
    Render_EndFrame();
    result.hash = Render_TestReadbackHash();
    result.diag = render_diagnostics();
    render_shutdown();
    return result;
}

/// Путь эталона @p name: <source_dir>/tests/visual/reference/<name>.bin
const char* golden_path(const char* name)
{
    static char path[640];
    const char* root = getenv("RPE_TEST_SOURCE_DIR");
    if (root == NULL || root[0] == '\0') {
        root = ".";
    }
    snprintf(path, sizeof(path), "%s/tests/visual/reference/%s", root, name);
    return path;
}

/**
 * Сравнение кадра с эталоном (или запись при RENDER_GOLDEN_REBASE=1).
 *
 * @param name Имя эталона («w0_room12.bin»).
 * @param hash Хэш актуального кадра.
 * @return true при совпадении (или при успешном rebase-записи).
 */
bool golden_check(const char* name, uint64_t hash)
{
    const char* path = golden_path(name);

    // Rebase-режим: осознанная запись эталона.
    const char* rebase = getenv("RENDER_GOLDEN_REBASE");
    if (rebase != NULL && rebase[0] == '1') {
        FILE* f = fopen(path, "wb");
        if (f == NULL) {
            printf("[golden] rebase: не открыть %s\n", path);
            return false;
        }
        // Big-endian запись хэша (стабильна против endian-хоста).
        for (int shift = 56; shift >= 0; shift -= 8) {
            const uint8_t byte = (uint8_t)((hash >> shift) & 0xFFu);
            fwrite(&byte, 1, 1, f);
        }
        fclose(f);
        printf("[golden] rebase %s <- %016llx\n", path,
               (unsigned long long)hash);
        return true;
    }

    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        printf("[golden] эталон отсутствует: %s (актуальный хэш %016llx) — "
               "запусти с RENDER_GOLDEN_REBASE=1 для записи\n",
               path, (unsigned long long)hash);
        return false;
    }
    uint64_t golden = 0u;
    for (int i = 0; i < 8; ++i) {
        const int byte = fgetc(f);
        if (byte == EOF) {
            fclose(f);
            return false;
        }
        golden = (golden << 8) | (uint64_t)(byte & 0xFFu);
    }
    fclose(f);
    if (golden != hash) {
        printf("[golden] MISMATCH %s: эталон %016llx, фактический %016llx\n",
               path, (unsigned long long)golden, (unsigned long long)hash);
    }
    return golden == hash;
}

} // namespace

// ---------------------------------------------------------------------------
// Конформанс-кейсы (детерминированные, по одному хэшу на кадр)
// ---------------------------------------------------------------------------

//! Smoke-clear: дефолтный фон (0,0,32), ни одной примитивы.
TEST(W0Golden, ClearDefaultBackground)
{
    if (!display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    FrameResult result;
    ASSERT_TRUE(render_startup());
    {
        Render_BeginFrame();
        Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G,
                     RENDER_CLEAR_DEFAULT_B);
        Render_EndFrame();
        result.hash = Render_TestReadbackHash();
        result.diag = render_diagnostics();
    }
    render_shutdown();

    EXPECT_EQ(result.diag.submitted, 0u);
    EXPECT_EQ(result.diag.clip_rejects, 0u);
    EXPECT_EQ(result.diag.backface_rejects, 0u);
    EXPECT_EQ(result.diag.depth_rejects, 0u);
    EXPECT_TRUE(golden_check("w0_clear_default.bin", result.hash));
}

//! Комната W0: 12 CI8-текстурированных треугольников, Gouraud, fixed камера.
TEST(W0Golden, Room12TexturedTriangles)
{
    if (!display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    ASSERT_TRUE(render_startup());

    Vertex vertices[W0_ROOM_VERTEX_COUNT];
    Triangle triangles[W0_ROOM_TRIANGLE_COUNT];
    w0_build_room(vertices, triangles);
    const TextureHandle texture = make_checker_texture();
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    const FrameResult result =
        render_frame(vertices, W0_ROOM_VERTEX_COUNT, triangles,
                     W0_ROOM_TRIANGLE_COUNT, texture);

    // Вся сцена видима изнутри комнаты (норматив: 12 подано, 0 reject).
    EXPECT_EQ(result.diag.submitted, W0_ROOM_TRIANGLE_COUNT);
    EXPECT_EQ(result.diag.clip_rejects, 0u);
    EXPECT_EQ(result.diag.backface_rejects, 0u);
    EXPECT_EQ(result.diag.depth_rejects, 0u);
    EXPECT_TRUE(golden_check("w0_room12.bin", result.hash));
}

//! Детерминизм: повторный рендер комнаты → тот же хэш (без золотого).
TEST(W0Golden, RoomRenderIsDeterministicAcrossRuns)
{
    if (!display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    ASSERT_TRUE(render_startup());
    Vertex vertices[W0_ROOM_VERTEX_COUNT];
    Triangle triangles[W0_ROOM_TRIANGLE_COUNT];
    w0_build_room(vertices, triangles);
    const TextureHandle texture1 = make_checker_texture();
    ASSERT_NE(texture1, RENDER_TEXTURE_INVALID);
    const FrameResult first = render_frame(vertices, W0_ROOM_VERTEX_COUNT,
                                           triangles, W0_ROOM_TRIANGLE_COUNT,
                                           texture1);

    ASSERT_TRUE(render_startup());
    const TextureHandle texture2 = make_checker_texture();
    ASSERT_NE(texture2, RENDER_TEXTURE_INVALID);
    const FrameResult second = render_frame(vertices, W0_ROOM_VERTEX_COUNT,
                                            triangles, W0_ROOM_TRIANGLE_COUNT,
                                            texture2);

    EXPECT_EQ(first.hash, second.hash);
}

// ---------------------------------------------------------------------------
// Одиночные нормативные кейсы + диагностические счётчики конвейера
// ---------------------------------------------------------------------------

//! Полностью видимый треугольник: подан, нарисован (цветной frame).
TEST(W0Golden, VisibleTriangleHashes)
{
    if (!display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    ASSERT_TRUE(render_startup());
    Vertex vertices[3];
    Triangle triangle;
    w0_build_case(W0_CASE_VISIBLE, vertices, &triangle);
    const TextureHandle texture = make_checker_texture();
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    const FrameResult result = render_frame(vertices, 3u, &triangle, 1u, texture);

    EXPECT_EQ(result.diag.submitted, 1u);
    EXPECT_EQ(result.diag.clip_rejects, 0u);
    EXPECT_EQ(result.diag.backface_rejects, 0u);
    EXPECT_EQ(result.diag.depth_rejects, 0u);
    EXPECT_TRUE(golden_check("w0_visible.bin", result.hash));
}

//! Backface-кулинг: зеркальный winding → CULL, кадр = чистый фон.
TEST(W0Golden, BackfaceTriangleCulled)
{
    if (!display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    ASSERT_TRUE(render_startup());
    Vertex vertices[3];
    Triangle triangle;
    w0_build_case(W0_CASE_BACKFACE, vertices, &triangle);
    const TextureHandle texture = make_checker_texture();
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    const FrameResult result = render_frame(vertices, 3u, &triangle, 1u, texture);

    EXPECT_EQ(result.diag.backface_rejects, 1u);
    EXPECT_EQ(result.diag.submitted, 0u);
    EXPECT_EQ(result.diag.clip_rejects, 0u);
    EXPECT_EQ(result.diag.last_code, (uint32_t)RENDER_DIAG_CULL_REJECT);
    EXPECT_TRUE(golden_check("w0_backface.bin", result.hash));
}

//! Пересечение near plane: вершина вне [near..far] → whole-triangle reject.
TEST(W0Golden, NearPlaneCrossRejectedWholeTriangle)
{
    if (!display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    ASSERT_TRUE(render_startup());
    Vertex vertices[3];
    Triangle triangle;
    w0_build_case(W0_CASE_NEAR_CROSS, vertices, &triangle);
    const TextureHandle texture = make_checker_texture();
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    const FrameResult result = render_frame(vertices, 3u, &triangle, 1u, texture);

    EXPECT_EQ(result.diag.clip_rejects, 1u);
    EXPECT_EQ(result.diag.submitted, 0u);
    EXPECT_EQ(result.diag.backface_rejects, 0u);
    EXPECT_EQ(result.diag.last_code, (uint32_t)RENDER_DIAG_CLIP_REJECT);
    EXPECT_TRUE(golden_check("w0_near_cross.bin", result.hash));
}

//! Пересечение far plane: вершина d > far → whole-triangle reject.
TEST(W0Golden, FarPlaneCrossRejectedWholeTriangle)
{
    if (!display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    ASSERT_TRUE(render_startup());
    Vertex vertices[3];
    Triangle triangle;
    w0_build_case(W0_CASE_FAR_CROSS, vertices, &triangle);
    const TextureHandle texture = make_checker_texture();
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    const FrameResult result = render_frame(vertices, 3u, &triangle, 1u, texture);

    EXPECT_EQ(result.diag.clip_rejects, 1u);
    EXPECT_EQ(result.diag.submitted, 0u);
    EXPECT_EQ(result.diag.last_code, (uint32_t)RENDER_DIAG_CLIP_REJECT);
    EXPECT_TRUE(golden_check("w0_far_cross.bin", result.hash));
}

//! All-verts behind camera: invalid clip/depth rejection (кейс 5 нормы).
TEST(W0Golden, BehindCameraInvalidClipRejection)
{
    if (!display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    ASSERT_TRUE(render_startup());
    Vertex vertices[3];
    Triangle triangle;
    w0_build_case(W0_CASE_BEHIND_CAMERA, vertices, &triangle);
    const TextureHandle texture = make_checker_texture();
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    const FrameResult result = render_frame(vertices, 3u, &triangle, 1u, texture);

    EXPECT_EQ(result.diag.clip_rejects, 1u);
    EXPECT_EQ(result.diag.submitted, 0u);
    EXPECT_EQ(result.diag.last_code, (uint32_t)RENDER_DIAG_CLIP_REJECT);
    EXPECT_TRUE(golden_check("w0_behind_camera.bin", result.hash));
}

// ---------------------------------------------------------------------------
// Lifecycle-контракт (normative G9): double-Init / EndFrame до Init /
// Draw вне кадра — явные диагностики, без крашей (release) и смерть-тест
// в debug: assert-ветка Lifecycle-контракта.
// ---------------------------------------------------------------------------

//! Double-Init: debug — EXPECT_DEATH (assert); release — false + diag.
TEST(RenderLifecycle, DoubleInitIsContractViolation)
{
    if (!display_available()) {
        GTEST_SKIP();
    }
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT));

#if !defined(NDEBUG)
    // Debug: ровно один Init per process — нарушитель умирает по assert.
    EXPECT_DEATH(Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT), ".*");
#else
    // Release: диагностический false (без краша).
    EXPECT_FALSE(Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT));
    const RenderDiagnostics diag = render_diagnostics();
    EXPECT_EQ(diag.last_code, (uint32_t)RENDER_DIAG_DOUBLE_INIT);
#endif

    Render_Shutdown();
    Platform_Shutdown();
}

//! EndFrame до Init: debug — death (assert); release — diag-код.
TEST(RenderLifecycle, EndFrameBeforeInitIsDiagnosed)
{
    if (!display_available()) {
        GTEST_SKIP();
    }
    ASSERT_TRUE(Platform_Init()); // Рендер — NOT initialized (после
                                  // прошлого теста Shutdown).
#if !defined(NDEBUG)
    EXPECT_DEATH(Render_EndFrame(), ".*");
#else
    Render_EndFrame();
    const RenderDiagnostics diag = render_diagnostics();
    EXPECT_EQ(diag.last_code, (uint32_t)RENDER_DIAG_END_FRAME_BEFORE_INIT);
#endif
    Platform_Shutdown();
}

//! DrawMesh вне BeginFrame..EndFrame — диагностический no-op.
TEST(RenderLifecycle, DrawOutsideFrameIsDiagnosed)
{
    if (!display_available()) {
        GTEST_SKIP();
    }
    ASSERT_TRUE(Platform_Init());
    ASSERT_TRUE(Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT));

    Vertex vertices[3];
    Triangle triangle;
    w0_build_case(W0_CASE_VISIBLE, vertices, &triangle);
    const TextureHandle texture = make_checker_texture();
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    // Draw БЕЗ BeginFrame — вне кадра (диагностика; release no-op).
    // Model = identity: NULL-аргумент дал бы NULL_ARGUMENT вместо
    // DRAW_OUTSIDE_FRAME (ожидание теста).
    Mat4 identity;
    mat4_identity(&identity);
    Render_DrawMesh(vertices, 3u, &triangle, 1u, &identity);
    const RenderDiagnostics diag = render_diagnostics();
    EXPECT_EQ(diag.last_code, (uint32_t)RENDER_DIAG_DRAW_OUTSIDE_FRAME);
    EXPECT_EQ(diag.submitted, 0u);

    Render_Shutdown();
    Platform_Shutdown();
}
