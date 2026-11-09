// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/visual/render_golden_test.cpp
//
// W0-конформанс рендера (docs/plans/sprint_07_08.md, неделя 16).
//
// Метод сравнения (normative): framebuffer readback + hash — НЕ «не
// крашится». Протокол (capture/check/фолбэк v2) вынесен в общий
// golden_protocol.{h,cpp} (Спринт 11–12: hello_triangle golden обязан
// считать тем же методом); здесь — только W0-сцены и кейсы.
//
// Эталоны: 8-байтные big-endian FNV-хэши в tests/visual/reference/*.bin
// (зафиксированы в репо). Регенерация эталонов ПОСЛЕ НАМЕРЕННОГО изменения
// рендера: env RENDER_GOLDEN_REBASE=1 (только осознанный rebase —
// никогда не используется для «подгонки» падающего теста).
//
// Среда: дисплей/GL-контекст (см. hal_test.cpp: headless-CI → GTEST_SKIP).
// Gate: пресет pc-debug-render (BUILD_RENDER_INTEGRATION_TESTS=ON).
#include "hal/platform.h"
#include "hal/render_backend.h" // draw-call inventory (weeks 15–16).
#include "tests/visual/golden_protocol.h" // единый golden-протокол (S11-12).
#include "render/renderer.h"
#include "render/texture.h" // unpack_rgb555_stp (палитра RGB8 для модели).
#include "tests/visual/w0_scene.h"

#include <gtest/gtest.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>

namespace {

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
    return Render_CreateTexture(64u, 64u, w0_make_checker_pixels(&tex_w, &tex_h),
                                w0_make_checker_palette());
}

/// Результат одного кадра (снимок диагностики ДО Shutdown — там zeroing).
struct FrameResult
{
    uint64_t hash;
    RenderDiagnostics diag;
    uint32_t draw_calls; ///< hal_render_backend_draw_call_count после EndFrame.
};

/// Бюджет фолбэка room12: замер 2026-11-02 (локальный драйвер vs
/// llvmpipe) = 26 px отличий, все по плюс-минус 1 LSB, точки на рёбрах
/// треугольников (rasterizer tie-breaking — вне GL-спецификации).
/// 128 = пятикратный запас против наблюдённого.
const uint32_t W0_ROOM_GOLDEN_BUDGET = 128u;

/**
 * Кадр с одной меш-подачей: BeginFrame → Clear(0,0,32) → Bind →
 * DrawMesh → EndFrame → hash; диагностика снимается до shutdown.
 */
FrameResult render_frame(const Vertex* vertices, uint32_t vertex_count, const Triangle* indices,
                         uint32_t index_count, TextureHandle texture)
{
    // Model = identity (контракт DrawMesh: NULL-аргумент — явная
    // диагностика NULL_ARGUMENT, «NULL = identity» пути нет).
    Mat4 identity;
    mat4_identity(&identity);
    FrameResult result;
    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
    Render_BindTexture(texture);
    Render_DrawMesh(vertices, vertex_count, indices, index_count, &identity);
    Render_EndFrame();
    result.hash = golden::capture_readback();
    result.diag = render_diagnostics();
    result.draw_calls = hal_render_backend_draw_call_count();
    render_shutdown();
    return result;
}

/**
 * Кадр с одной меш-подачей БЕЗ вызова Render_BindTexture (забытая
 * привязка): flat-Gouraud-путь — валидный нетекстурированный draw.
 */
FrameResult render_frame_unbound(const Vertex* vertices, uint32_t vertex_count,
                                 const Triangle* indices, uint32_t index_count)
{
    Mat4 identity;
    mat4_identity(&identity);
    FrameResult result;
    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
    Render_DrawMesh(vertices, vertex_count, indices, index_count, &identity);
    Render_EndFrame();
    result.hash = golden::capture_readback();
    result.diag = render_diagnostics();
    result.draw_calls = hal_render_backend_draw_call_count();
    render_shutdown();
    return result;
}

// ---------------------------------------------------------------------------
// Числовой affine-конформанс: программная реплика GPU-пайплайна
// (fixed-матрицы ядра → GL float-матрица → квантование vert 1/16 →
// экранная барицентрика → CI8-семпл nearest → модуляция Gouraud'ом).
// Проверка NOT «не крашится»: readback ядра сравнивается ПИКСЕЛЬНО с
// forecast'ом; отрицательный контроль — perspective-correct прогноз UV
// обязан расходиться с affine (иначе замер бессмысленен).
// ---------------------------------------------------------------------------

/// GLSL-реплика mat·vec в double (GL column-major массив m[16]).
void model_clip_from_view(const float* m, const double view[4], double clip[4])
{
    for (int c = 0; c < 4; ++c) {
        double acc = 0.0;
        for (int j = 0; j < 4; ++j) {
            acc += (double)m[(size_t)j * 4u + (size_t)c] * view[j];
        }
        clip[c] = acc;
    }
}

/// Vert-ветка: квантование экрана 1/16 (normative floor(x*16+0.5)/16) →
/// окна-координаты (x вправо, y ВВЕРХ от низа FBO — readback row 0 =
/// bottom: glReadPixels) + clip.w (perspective-вес smooth-варяинга).
void model_quantized_window(const double clip[4], double* out_wx, double* out_wy, double* out_w)
{
    const double w = clip[3];
    const double ndc_x = clip[0] / w;
    const double ndc_y = clip[1] / w;
    double sx = (ndc_x * 0.5 + 0.5) * 320.0; // vert: виртуальный 320×240.
    double sy = (ndc_y * 0.5 + 0.5) * 240.0;
    sx = floor(sx * 16.0 + 0.5) / 16.0;
    sy = floor(sy * 16.0 + 0.5) / 16.0;
    *out_wx = sx;
    *out_wy = sy;
    *out_w = w;
}

/// Барицентрики пикселя (px,py) по quantized-вершинам окна (y-up).
void model_bary(const double qx[3], const double qy[3], double px, double py, double l[3])
{
    const double den = (qy[1] - qy[2]) * (qx[0] - qx[2]) + (qx[2] - qx[1]) * (qy[0] - qy[2]);
    l[0] = ((qy[1] - qy[2]) * (px - qx[2]) + (qx[2] - qx[1]) * (py - qy[2])) / den;
    l[1] = ((qy[2] - qy[0]) * (px - qx[2]) + (qx[0] - qx[2]) * (py - qy[2])) / den;
    l[2] = 1.0 - l[0] - l[1];
}

/// Расстояние в px от пикселя до ребра окна tri (гвард raster-edge rule).
double model_edge_distance(double ax, double ay, double bx, double by, double px, double py)
{
    const double dx = bx - ax;
    const double dy = by - ay;
    const double len = sqrt(dx * dx + dy * dy);
    if (len < 1e-9) {
        return 0.0;
    }
    return fabs(dx * (py - ay) - dy * (px - ax)) / len;
}

/**
 * Прогноз бесперспективного кадра пикселя: interp UV (affine — лямбда;
 * «perspective» — μ = λ/w-нормированные) → texel-index (vertical split
 * 64×64: x < 32 → palette[0], иначе palette[1]) → модуляция smooth
 * v_color (perspective-correct ВСЕГДА — GPU). Выход: RGB 0..255.
 */
void model_forecast(const double l[3], const double w[3], bool uv_persp, const double uu[3],
                    const double vv[3], const double cr[3], const double cg[3], const double cb[3],
                    uint8_t palette_u8[2][3], double out_rgb[3], int* out_index)
{
    // Perspective-correct нормировка весов (smooth v_color — GPU плавно).
    double color_r = 0.0;
    double color_g = 0.0;
    double color_b = 0.0;
    {
        double norm = 0.0;
        for (int i = 0; i < 3; ++i) {
            norm += l[i] / w[i];
        }
        for (int i = 0; i < 3; ++i) {
            const double mu = (l[i] / w[i]) / norm;
            color_r += mu * cr[i];
            color_g += mu * cg[i];
            color_b += mu * cb[i];
        }
    }

    // v_texcoord — noperspective: линейно в ОКНЕ от квантового λ.
    double u = 0.0;
    double v_coord = 0.0;
    if (uv_persp) {
        double norm = 0.0;
        for (int i = 0; i < 3; ++i) {
            norm += l[i] / w[i];
        }
        for (int i = 0; i < 3; ++i) {
            const double mu = (l[i] / w[i]) / norm;
            u += mu * uu[i];
            v_coord += mu * vv[i];
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            u += l[i] * uu[i];
            v_coord += l[i] * vv[i];
        }
    }

    int tex_x = (int)floor(u * 64.0);
    int tex_y = (int)floor(v_coord * 64.0);
    if (tex_x < 0) {
        tex_x = 0;
    }
    if (tex_x > 63) {
        tex_x = 63;
    }
    if (tex_y < 0) {
        tex_y = 0;
    }
    if (tex_y > 63) {
        tex_y = 63;
    }
    const int index = (tex_x < 32) ? 0 : 1; // вертикальный шов UV.x = 0.5.
    *out_index = index;
    const double chan[3] = {color_r, color_g, color_b};
    for (int c = 0; c < 3; ++c) {
        // RGBA8: (palette_u8/255)·(color_ed)·255 = palette_u8·color_ed —
        // ±1 LSB допуск на rounding-джиттер драйвера (золотой hash гасит
        // его 15-битным сжатием; здесь допуск явный).
        out_rgb[c] = (double)palette_u8[index][0 + c] * chan[c];
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Конформанс-кейсы (детерминированные, по одному хэшу на кадр)
// ---------------------------------------------------------------------------

//! Smoke-clear: дефолтный фон (0,0,32), ни одной примитивы.
TEST(W0Golden, ClearDefaultBackground)
{
    if (!golden::display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    FrameResult result;
    ASSERT_TRUE(render_startup());
    {
        Render_BeginFrame();
        Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
        Render_EndFrame();
        result.hash = golden::capture_readback();
        result.diag = render_diagnostics();
        result.draw_calls = hal_render_backend_draw_call_count();
    }
    render_shutdown();

    EXPECT_EQ(result.diag.submitted, 0u);
    EXPECT_EQ(result.diag.clip_rejects, 0u);
    EXPECT_EQ(result.diag.backface_rejects, 0u);
    EXPECT_EQ(result.diag.depth_rejects, 0u);
    // Draw-call inventory (weeks 15–16): пустой submit-кадр — ровно 1
    // draw-команда (fullscreen present-quad).
    EXPECT_EQ(result.draw_calls, 1u);
    EXPECT_TRUE(golden::check("w0_clear_default.bin", result.hash, 0u));
}

//! Комната W0: 12 CI8-текстурированных треугольников, Gouraud, fixed камера.
TEST(W0Golden, Room12TexturedTriangles)
{
    if (!golden::display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    ASSERT_TRUE(render_startup());

    Vertex vertices[W0_ROOM_VERTEX_COUNT];
    Triangle triangles[W0_ROOM_TRIANGLE_COUNT];
    w0_build_room(vertices, triangles);
    const TextureHandle texture = make_checker_texture();
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    const FrameResult result =
        render_frame(vertices, W0_ROOM_VERTEX_COUNT, triangles, W0_ROOM_TRIANGLE_COUNT, texture);

    // Вся сцена видима изнутри комнаты (норматив: 12 подано, 0 reject).
    EXPECT_EQ(result.diag.submitted, W0_ROOM_TRIANGLE_COUNT);
    EXPECT_EQ(result.diag.clip_rejects, 0u);
    EXPECT_EQ(result.diag.backface_rejects, 0u);
    EXPECT_EQ(result.diag.depth_rejects, 0u);
    // Draw-call inventory (weeks 15–16): Painter's-алгоритм = ровно 1
    // GL-подача на треугольник → 12 причесанных + 1 present-quad = 13.
    EXPECT_EQ(result.draw_calls, W0_ROOM_TRIANGLE_COUNT + 1u);
    // Хэш room12 НЕ bit-стабилен между растеризаторами (см. замер в
    // docblock golden_check) — фолбэк с бюджетом по px-партнёру.
    EXPECT_TRUE(golden::check("w0_room12.bin", result.hash, W0_ROOM_GOLDEN_BUDGET));
}

//! Детерминизм: повторный рендер комнаты → тот же хэш (без золотого).
TEST(W0Golden, RoomRenderIsDeterministicAcrossRuns)
{
    if (!golden::display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    ASSERT_TRUE(render_startup());
    Vertex vertices[W0_ROOM_VERTEX_COUNT];
    Triangle triangles[W0_ROOM_TRIANGLE_COUNT];
    w0_build_room(vertices, triangles);
    const TextureHandle texture1 = make_checker_texture();
    ASSERT_NE(texture1, RENDER_TEXTURE_INVALID);
    const FrameResult first =
        render_frame(vertices, W0_ROOM_VERTEX_COUNT, triangles, W0_ROOM_TRIANGLE_COUNT, texture1);

    ASSERT_TRUE(render_startup());
    const TextureHandle texture2 = make_checker_texture();
    ASSERT_NE(texture2, RENDER_TEXTURE_INVALID);
    const FrameResult second =
        render_frame(vertices, W0_ROOM_VERTEX_COUNT, triangles, W0_ROOM_TRIANGLE_COUNT, texture2);

    EXPECT_EQ(first.hash, second.hash);
}

// ---------------------------------------------------------------------------
// Одиночные нормативные кейсы + диагностические счётчики конвейера
// ---------------------------------------------------------------------------

//! Полностью видимый треугольник: подан, нарисован (цветной frame).
TEST(W0Golden, VisibleTriangleHashes)
{
    if (!golden::display_available()) {
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
    // Draw-call inventory: 1 trouverez + 1 present-quad = 2.
    EXPECT_EQ(result.draw_calls, 2u);
    EXPECT_TRUE(golden::check("w0_visible.bin", result.hash, 0u));
}

//! [H] Нетекстурированный треугольник (неделя 15): явный unbind
//! (BindTexture(INVALID) → diag OK) и «забытый bind» — ОДИН flat-Gouraud
//! путь (unbound texture is simply drawn flat): submitted = 1, пиксельный
//! результат отличается от текстурированного (ветка CI8-семпла не берётся).
TEST(W0Golden, UntexturedTriangleFlatGouraud)
{
    if (!golden::display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }
    Vertex vertices[3];
    Triangle triangle;
    w0_build_case(W0_CASE_VISIBLE, vertices, &triangle);

    // Явный unbind: привязка INVALID — валидный путь (diag OK), draw flat.
    ASSERT_TRUE(render_startup());
    const FrameResult flat = render_frame(vertices, 3u, &triangle, 1u, RENDER_TEXTURE_INVALID);
    EXPECT_EQ(flat.diag.submitted, 1u);
    EXPECT_EQ(flat.diag.clip_rejects, 0u);
    EXPECT_EQ(flat.diag.backface_rejects, 0u);
    EXPECT_EQ(flat.diag.depth_rejects, 0u);
    EXPECT_EQ(flat.diag.last_code, (uint32_t)RENDER_DIAG_OK);
    EXPECT_EQ(flat.draw_calls, 2u); // Нетекстурированная подача + present.

    // «Забытый bind» (BindTexture не вызвана вовсе) — тот же flat путь:
    // пиксельный результат идентичен явному unbind (нет тихого сбоя).
    ASSERT_TRUE(render_startup());
    const FrameResult forgot = render_frame_unbound(vertices, 3u, &triangle, 1u);
    EXPECT_EQ(forgot.diag.submitted, 1u);
    EXPECT_EQ(forgot.hash, flat.hash);

    // Тот же треугольник С привязанной CI8-текстурой — другой результат
    // (заякоривает различимость веток на readback).
    ASSERT_TRUE(render_startup());
    const TextureHandle texture = make_checker_texture();
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);
    const FrameResult textured = render_frame(vertices, 3u, &triangle, 1u, texture);
    EXPECT_EQ(textured.diag.submitted, 1u);
    EXPECT_NE(textured.hash, flat.hash);

    EXPECT_TRUE(golden::check("w0_untextured.bin", flat.hash, 0u));
}

//! Backface-кулинг: зеркальный winding → CULL, кадр = чистый фон.
TEST(W0Golden, BackfaceTriangleCulled)
{
    if (!golden::display_available()) {
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
    EXPECT_EQ(result.draw_calls, 1u); // CULLED → только present-quad.
    EXPECT_TRUE(golden::check("w0_backface.bin", result.hash, 0u));
}

//! Пересечение near plane: вершина вне [near..far] → whole-triangle reject.
TEST(W0Golden, NearPlaneCrossRejectedWholeTriangle)
{
    if (!golden::display_available()) {
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
    EXPECT_EQ(result.draw_calls, 1u); // Полный reject: только present-quad.
    EXPECT_TRUE(golden::check("w0_near_cross.bin", result.hash, 0u));
}

//! Пересечение far plane: вершина d > far → whole-triangle reject.
TEST(W0Golden, FarPlaneCrossRejectedWholeTriangle)
{
    if (!golden::display_available()) {
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
    EXPECT_EQ(result.draw_calls, 1u); // Полный reject: только present-quad.
    EXPECT_TRUE(golden::check("w0_far_cross.bin", result.hash, 0u));
}

//! All-verts behind camera: invalid clip/depth rejection (кейс 5 нормы).
TEST(W0Golden, BehindCameraInvalidClipRejection)
{
    if (!golden::display_available()) {
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
    EXPECT_EQ(result.draw_calls, 1u); // Полный reject: только present-quad.
    EXPECT_TRUE(golden::check("w0_behind_camera.bin", result.hash, 0u));
}

// ---------------------------------------------------------------------------
// Числовой affine-конформанс на наклонной плоскости (weeks 15–16): readback
// ядра сверяется ПИКСЕЛЬНО с программным прогнозом GPU-пайплайна
// (model_*-реплика шейдерной математики). Отрицательный контроль: пиксели
// «affine ≠ perspective UV» обязаны существовать (иначе замер бессмыслен)
// и следовать РОВНО affine-индексам палитрового шва.
// ---------------------------------------------------------------------------

//! Affine (noperspective) UV: наклонный floor-треугольник W0-комнаты
//! (z −5..−20 — градиент w 10→25) с двуцветной палитровой шов-текстурой.
//!
//! Проверка заменяет два ручных пункта приёмки sprint_07_08:
//!  (а) «affine подтверждено визуально (текстуры плывут на наклонных
//!      плоскостях)» — шов floor'а на readback рассчитывается до 1/16 px;
//!  (б) «RenderDoc: draw calls корректны» — инвентарь draw-команд кадра
//!      проверяется программно (hal_render_backend_draw_call_count).
TEST(W0Golden, AffineNumericConformanceOnSlantedPlane)
{
    if (!golden::display_available()) {
        GTEST_SKIP() << "Дисплей недоступен — W0-golden пропущен (headless)";
    }

    // --- Сцена: наклонный floor-tri W0 (normative w0_build_room, пол tri0) --
    Vertex vertices[3];
    Triangle triangle;
    memset(vertices, 0, sizeof(vertices));
    memset(&triangle, 0, sizeof(triangle));
    triangle.v0 = 0u;
    triangle.v1 = 1u;
    triangle.v2 = 2u;

    const fixed_t fx[3] = {fixed_from_int(-5), fixed_from_int(5), fixed_from_int(5)};
    const fixed_t fy[3] = {fixed_from_int(-5), fixed_from_int(-5), fixed_from_int(-5)};
    const fixed_t fz[3] = {fixed_from_int(-5), fixed_from_int(-5), fixed_from_int(-20)};
    const uint8_t uv_in[3][2] = {{0u, 0u}, {255u, 0u}, {255u, 255u}};
    const uint8_t col_in[3][3] = {
        {200u, 120u, 100u},
        {240u, 160u, 60u},
        {120u, 200u, 60u},
    };
    for (uint32_t i = 0u; i < 3u; ++i) {
        vertices[i].x = fx[i];
        vertices[i].y = fy[i];
        vertices[i].z = fz[i];
        vertices[i].u = uv_in[i][0];
        vertices[i].v = uv_in[i][1];
        vertices[i].r = col_in[i][0];
        vertices[i].g = col_in[i][1];
        vertices[i].b = col_in[i][2];
        vertices[i].a = 255u; // Прозрачность выключена (норматив W0).
    }

    // --- Палитра RGB8 (та же honesty-конверсия, что и backend §1.2.5) ------
    const uint16_t* palette_u16 = w0_make_checker_palette();
    uint8_t pal8[2][3];
    unpack_rgb555_stp(palette_u16[0], &pal8[0][0], &pal8[0][1], &pal8[0][2]);
    unpack_rgb555_stp(palette_u16[1], &pal8[1][0], &pal8[1][1], &pal8[1][2]);
    int palette_separation = 0; // Шов обязан реконструироваться однозначно.
    for (int c = 0; c < 3; ++c) {
        palette_separation += abs((int)pal8[1][c] - (int)pal8[0][c]);
    }
    ASSERT_GE(palette_separation, 100);

    // --- Кадр: шов-текстура + наклонный tri --------------------------------
    ASSERT_TRUE(render_startup());
    uint32_t tex_w = 0u;
    uint32_t tex_h = 0u;
    const TextureHandle texture = Render_CreateTexture(
        64u, 64u, w0_make_seam_pixels(&tex_w, &tex_h), w0_make_checker_palette());
    ASSERT_NE(texture, RENDER_TEXTURE_INVALID);

    Mat4 identity;
    mat4_identity(&identity);
    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
    Render_BindTexture(texture);
    Render_DrawMesh(vertices, 3u, &triangle, 1u, &identity);
    Render_EndFrame();
    EXPECT_EQ(render_diagnostics().submitted, 1u);
    EXPECT_EQ(render_diagnostics().clip_rejects, 0u);
    EXPECT_EQ(render_diagnostics().backface_rejects, 0u);
    EXPECT_EQ(render_diagnostics().depth_rejects, 0u);
    EXPECT_EQ(hal_render_backend_draw_call_count(), 2u);

    uint8_t pixels[320u * 240u * 4u];
    ASSERT_TRUE(Render_TestReadbackPixels(pixels, sizeof(pixels)));
    Render_Shutdown();
    Platform_Shutdown();

    // Фон: clear (0,0,32) на пикселе (0,0) ВНЕ tri (row 0 = низ framebuffer).
    EXPECT_EQ(pixels[0u], 0u);   // r
    EXPECT_EQ(pixels[1u], 0u);   // g
    EXPECT_EQ(pixels[2u], 32u);  // b = clear
    EXPECT_EQ(pixels[3u], 255u); // a

    // --- Программная реплика (фаза прогноза; model_*-хелперы) --------------
    Mat4 view;
    Mat4 proj;
    w0_camera(&view, &proj);
    Mat4 mv;
    mat4_multiply(&mv, &view, &identity);
    Mat4 mvp;
    mat4_multiply(&mvp, &proj, &mv);

    float mvp_gl[16]; // Граница бэкенда §4.4 — та же fixed_to_float.
    for (uint32_t i = 0u; i < 16u; ++i) {
        mvp_gl[i] = fixed_to_float(mvp.m[i]);
    }

    double qx[3];
    double qy[3];
    double qw[3];
    double uu[3];
    double vv[3];
    double cr[3];
    double cg[3];
    double cb[3];
    for (uint32_t v = 0u; v < 3u; ++v) {
        const Vec4 view_p = mat4_transform(&mv, Vec4{fx[v], fy[v], fz[v], FIXED_ONE});
        // float-вершины — как в Render_EndFrame → backend draw.
        const double vd[4] = {(double)fixed_to_float(view_p.x), (double)fixed_to_float(view_p.y),
                              (double)fixed_to_float(view_p.z), 1.0};
        double clip[4];
        model_clip_from_view(mvp_gl, vd, clip);
        model_quantized_window(clip, &qx[v], &qy[v], &qw[v]);
        uu[v] = (double)uv_in[v][0] / 255.0;
        vv[v] = (double)uv_in[v][1] / 255.0;
        cr[v] = (double)col_in[v][0] / 255.0;
        cg[v] = (double)col_in[v][1] / 255.0;
        cb[v] = (double)col_in[v][2] / 255.0;
    }

    // --- Пиксельная сверка --------------------------------------------------
    uint32_t guarded = 0u;     // Пикселей под guard-фильтрами.
    uint32_t mismatched = 0u;  // Промахов против affine-прогноза (≥ 3 LSB).
    uint32_t informative = 0u; // affine ≠ persp-структура (расхождение ≥ 1 texel).
    uint32_t ambiguous = 0u;   // Реконструкция неоднозначна (skip — не судим).
    uint32_t persp_hits = 0u;  // Негативный контрольследит за persp.

    for (uint32_t py = 0u; py < 240u; ++py) {
        for (uint32_t px = 0u; px < 320u; ++px) {
            const double cx = (double)px + 0.5;
            const double cy = (double)py + 0.5;
            double l[3];
            model_bary(qx, qy, cx, cy, l);
            if (l[0] < 0.0 || l[1] < 0.0 || l[2] < 0.0) {
                continue; // Вне tri — не raster-кандидат.
            }
            // Гвард от рёбер (1 px): покрытие и edge-rule зона ядра.
            const double e01 = model_edge_distance(qx[0], qy[0], qx[1], qy[1], cx, cy);
            const double e12 = model_edge_distance(qx[1], qy[1], qx[2], qy[2], cx, cy);
            const double e20 = model_edge_distance(qx[2], qy[2], qx[0], qy[0], cx, cy);
            if (e01 < 1.0 || e12 < 1.0 || e20 < 1.0) {
                continue;
            }
            // Прогноз: affine (объект) и perspective (отриц. контроль).
            double rgb_aff[3];
            int idx_aff = 0;
            model_forecast(l, qw, false, uu, vv, cr, cg, cb, pal8, rgb_aff, &idx_aff);
            double rgb_persp[3];
            int idx_persp = 0;
            model_forecast(l, qw, true, uu, vv, cr, cg, cb, pal8, rgb_persp, &idx_persp);

            // Гвард texel-бординков (шов → 32 — тоже integer-bound): 0.10.
            double frac = 0.0;
            {
                const double u_aff = l[0] * uu[0] + l[1] * uu[1] + l[2] * uu[2];
                frac = u_aff * 64.0;
                frac = fabs(frac - floor(frac + 0.5));
            }
            if (frac < 0.10) {
                continue; // Бордин floor: не судим (не может быть шум-флёра).
            }
            ++guarded;

            const uint8_t* actual = &pixels[((size_t)py * 320u + (size_t)px) * 4u];
            bool equal_to_affine = true;
            for (int c = 0; c < 3; ++c) {
                if (fabs((double)actual[c] - rgb_aff[c]) > 2.0) {
                    equal_to_affine = false;
                }
            }
            if (!equal_to_affine) {
                if (mismatched < 8u) {
                    printf(
                        "[affine] mismatch (%u,%u): read=(%u,%u,%u) "
                        "aff=(%.1f,%.1f,%.1f) persp=(%.1f,%.1f,%.1f)\n",
                        px, py, actual[0], actual[1], actual[2], rgb_aff[0], rgb_aff[1], rgb_aff[2],
                        rgb_persp[0], rgb_persp[1], rgb_persp[2]);
                }
                ++mismatched;
                continue;
            }

            // Отрицательный контроль: расхождение ≥ 1 texel структурно.
            double u_persp_hat = 0.0;
            {
                double norm = 0.0;
                for (int i = 0; i < 3; ++i) {
                    norm += l[i] / qw[i];
                }
                for (int i = 0; i < 3; ++i) {
                    u_persp_hat += (l[i] / qw[i]) / norm * uu[i];
                }
            }
            const double texel_div =
                fabs(l[0] * uu[0] + l[1] * uu[1] + l[2] * uu[2] - u_persp_hat) * 64.0;
            if (idx_aff == idx_persp || texel_div < 1.0) {
                continue;
            }
            // Реконструкция индекса по палитрам (channel — persp-weights).
            double cands[2][3];
            {
                double norm = 0.0;
                for (int i = 0; i < 3; ++i) {
                    norm += l[i] / qw[i];
                }
                double chan[3] = {0.0, 0.0, 0.0};
                for (int c = 0; c < 3; ++c) {
                    for (int i = 0; i < 3; ++i) {
                        chan[c] += (l[i] / qw[i]) / norm * ((double)col_in[i][c] / 255.0);
                    }
                    cands[0][c] = (double)pal8[0][c] * chan[c];
                    cands[1][c] = (double)pal8[1][c] * chan[c];
                }
            }
            double err[2] = {0.0, 0.0};
            for (int h = 0; h < 2; ++h) {
                for (int c = 0; c < 3; ++c) {
                    err[h] += fabs((double)actual[c] - cands[h][c]);
                }
            }
            if (fabs(err[1] - err[0]) < 25.0) {
                ++ambiguous; // Неоднозначная реконструкция — не судим.
                continue;
            }
            const int idx_est = (err[0] <= err[1]) ? 0 : 1;
            ++informative;
            if (idx_est != idx_aff) {
                ++persp_hits; // persp-паттерн НЕ должен торчать.
                if (persp_hits <= 8u) {
                    printf(
                        "[affine] PERSP-HIT (%u,%u): est=%d "
                        "aff=%d persp=%d div=%.2f texel\n",
                        px, py, idx_est, idx_aff, idx_persp, texel_div);
                }
            }
        }
    }

    printf(
        "[affine] guarded=%u mismatched=%u informative=%u ambiguous=%u "
        "persp_hits=%u\n",
        guarded, mismatched, informative, ambiguous, persp_hits);

    // Контракты конформанса (все — негативный контраст читаемости):
    ASSERT_GT(guarded, 500u);      // Сцена спелая: достаточно пикселей.
    EXPECT_EQ(mismatched, 0u);     // Readback ≡ affine-прогноз (±2 LSB).
    ASSERT_GT(informative, 100u);  // Отрицательный контраст существует.
    EXPECT_EQ(persp_hits, 0u);     // «Плывут» — ИМЕННО affine, не persp.
}

// ---------------------------------------------------------------------------
// Lifecycle-контракт (normative G9): double-Init / EndFrame до Init /
// Draw вне кадра — явные диагностики, без крашей (release) и смерть-тест
// в debug: assert-ветка Lifecycle-контракта.
// ---------------------------------------------------------------------------

//! Double-Init: debug — EXPECT_DEATH (assert); release — false + diag.
TEST(RenderLifecycle, DoubleInitIsContractViolation)
{
    if (!golden::display_available()) {
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
    if (!golden::display_available()) {
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
    if (!golden::display_available()) {
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
