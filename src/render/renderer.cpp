// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/renderer.cpp
//
// Ядро рендера (Спринт 7–8). Реализует публичные Render_* из
// hal/platform.h + внутренние контракты render/renderer.h.
//
// Конвейер (§7.1): TRANSFORM → CLIP → CULL → SORT → PACK → SUBMIT.
//   - TRANSFORM: view·model (fixed 16.16, mat4_multiply/mat4_transform);
//   - CLIP: треугольник с вершиной вне [near..far] отбрасывается ЦЕЛИКОМ
//     (Visibility contract допускает «explicitly rejected»; near/far
//     выводятся из proj честной fixed-математикой: near = B/(A−1),
//     far = B/(A+1), где A = m[10], B = m[14] — обратный вывод
//     mat4_perspective);
//   - CULL: PS1-NCLIP-эмуляция — знаковая площадь fixed-экрана (320×240,
//     y вниз): A < 0 — фронт (CCW для наблюдателя), A ≥ 0 — backface;
//   - SORT: depth_key (far→near) + tie-breaker submission_index; std::sort
//     со строгим слабым порядком (детерминизм: вход детерминирован);
//   - PACK: по треугольнику в CommandBuffer (64 KiB; overflow — явная
//     ошибка кадра, никакого silent drop);
//   - SUBMIT: backend-подача по одному треугольнику в отсортированном
//     порядке → выкладка FBO на окно (nearest).
//
// Память: только статические области (command buffer + sort refs); heap
// запрещён. Lifecycle: Platform_Init → РОВНО ОДИН Render_Init до первого
// BeginFrame (Lifecycle contract §1: double-Init / EndFrame-до-Init —
// нарушение: debug assert, release — диагностический код).
#include "render/renderer.h"

#include "hal/render_backend.h"
#include "render/command_buffer.h"
#include "render/sort_key.h"
#include "render/texture.h"

#include <assert.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Состояние ядра рендера (статические области; сброс — в Init/Shutdown)
// ---------------------------------------------------------------------------

/** Состояние lifecycle-машины рендера. */
typedef enum
{
    RENDER_STATE_UNINITIALIZED = 0, /**< Shutdown/до Init. */
    RENDER_STATE_READY,             /**< Init завершён, кадра нет. */
    RENDER_STATE_IN_FRAME           /**< BeginFrame активен. */
} RenderState;

static RenderState s_state = RENDER_STATE_UNINITIALIZED;
static uint32_t s_fb_width = 0u;
static uint32_t s_fb_height = 0u;

/** Цвет очистки кадра (последний Render_Clear либо default (0,0,32)). */
static uint8_t s_clear_r = RENDER_CLEAR_DEFAULT_R;
static uint8_t s_clear_g = RENDER_CLEAR_DEFAULT_G;
static uint8_t s_clear_b = RENDER_CLEAR_DEFAULT_B;

static Mat4 s_view;  /**< View-матрица (Render_SetCamera). */
static Mat4 s_proj;  /**< Projection-матрица (Render_SetCamera). */
static bool s_camera_set = false; /**< SetCamera прошёл валидацию. */
static fixed_t s_near = 0; /**< Выведенный near-plane (fixed 16.16). */
static fixed_t s_far = 0;  /**< Выведенный far-plane (fixed 16.16). */
static uint32_t s_depth_min_units = 0u;   /**< depth_min depth_key (units). */
static uint32_t s_depth_max_units = 4095u;/**< depth_max depth_key (units). */

static uint32_t s_submission_counter = 0u; /**< Порядок вставки команд. */

/** Привязанная текстура текущих DrawMesh (Render_BindTexture). */
static TextureHandle s_bound_texture = RENDER_TEXTURE_INVALID;

/// Командный буфер кадра (64 KiB; static — НЕ на стеке).
static CommandBuffer s_command_buffer;

/// Вместимость сортировки равна вместимости command buffer.
#define RENDER_SORT_MAX (RENDER_COMMAND_BUFFER_BYTES / sizeof(RenderTriCmd))
static SortRef s_sort_refs[RENDER_SORT_MAX];

// ---------------------------------------------------------------------------
// Диагностика
// ---------------------------------------------------------------------------

/// Счётчики диагностики (render_diagnostics).
static RenderDiagnostics s_diag;

/** Фиксация кода последней диагностики. */
static void render_diag(RenderDiag code) { s_diag.last_code = (uint32_t)code; }

RenderDiagnostics render_diagnostics(void) { return s_diag; }

// ---------------------------------------------------------------------------
// Depth-контракт (§1.2.6 — normative, формат 1:1)
// ---------------------------------------------------------------------------

uint32_t depth_key_from_camera_z(int32_t camera_z, uint32_t depth_min,
                                 uint32_t depth_max)
{
    if (camera_z < 0) {
        return DEPTH_REJECT; // Без modulo; normative-вектор (−1 → REJECT).
    }
    const uint32_t z = (uint32_t)camera_z;
    if (z < depth_min || z > depth_max) {
        return DEPTH_REJECT; // Normative-вектор (4096 → REJECT на [0..4095]).
    }
    return z - depth_min; // Bucket 0 — near, (max−min) — far.
}

// ---------------------------------------------------------------------------
// Конвертация fixed→GL float (граница бэкенда §4.4)
// ---------------------------------------------------------------------------

/**
 * Конвертация column-major Mat4 (fixed 16.16) в GL float[16].
 *
 * @param m Матрица фиксированного ядра (не NULL).
 * @param out_gl Выходной float-массив (16 значений; не NULL).
 */
static void render_mat4_to_gl(const Mat4* m, float* out_gl)
{
    for (uint32_t i = 0u; i < 16u; ++i) {
        out_gl[i] = fixed_to_float(m->m[i]);
    }
}

// ---------------------------------------------------------------------------
// Вывод near/far из projection (fixed-математика, детерминированно)
// ---------------------------------------------------------------------------

/**
 * Обратный вывод матрицы gluPerspective (mat4_perspective):
 *   A = m[10] = (far+near)/(near−far), B = m[14] = 2·far·near/(near−far);
 *   near = B/(A−1), far = B/(A+1) — точная fixed-математика при валидном
 *   вводе. Границы depth_key согласованы (trunc монотонен: d ≥ near ⇒
 *   trunc(d) ≥ trunc(near); d ≤ far ⇒ trunc(d) ≤ trunc(far)).
 *
 * @return false при неисправимой проекции (A±1 = 0, near ≤ 0, far ≤ near,
 *         насыщение fixed_div) — статус RENDER_DIAG_INVALID_PROJECTION.
 */
static bool derive_near_far_from_projection(void)
{
    const fixed_t a = s_proj.m[10];
    const fixed_t b = s_proj.m[14];

    const fixed_t den_near = fixed_sub(a, FIXED_ONE);
    const fixed_t den_far = fixed_add(a, FIXED_ONE);
    if (den_near == 0 || den_far == 0) {
        return false; // Вырожденный знаменатель — без fixed_div-assert.
    }

    const fixed_t near_z = fixed_div(b, den_near);
    const fixed_t far_z = fixed_div(b, den_far);

    // Валидные диапазоны: near > 0, far > near, без насыщенной границы.
    if (near_z <= 0 || far_z <= near_z) {
        return false;
    }
    if (near_z == INT32_MAX || near_z == INT32_MIN || far_z == INT32_MAX
        || far_z == INT32_MIN) {
        return false;
    }

    s_near = near_z;
    s_far = far_z;

    const int32_t near_units = fixed_to_int(s_near);
    const int32_t far_units = fixed_to_int(s_far);
    s_depth_min_units = (near_units > 0) ? (uint32_t)near_units : 0u;
    s_depth_max_units = (far_units > near_units) ? (uint32_t)far_units
                                                 : (uint32_t)near_units + 1u;
    return true;
}

// ---------------------------------------------------------------------------
// Lifecycle: Init/Shutdown/BeginFrame/EndFrame/Clear
// ---------------------------------------------------------------------------

bool Render_Init(uint32_t width, uint32_t height)
{
    if (s_state != RENDER_STATE_UNINITIALIZED) {
        assert(!"Render_Init вызван повторно до Render_Shutdown "
                "(Lifecycle contract: ровно один Init — normative §1)");
        render_diag(RENDER_DIAG_DOUBLE_INIT);
        return false;
    }
    if (width == 0u || height == 0u || width > 4096u || height > 4096u) {
        render_diag(RENDER_DIAG_INVALID_SIZE);
        return false;
    }

    memset(&s_diag, 0, sizeof(s_diag));
    render_texture_pool_reset();
    s_command_buffer.reset();
    memset(&s_view, 0, sizeof(s_view));
    memset(&s_proj, 0, sizeof(s_proj));
    s_camera_set = false;
    s_near = 0;
    s_far = 0;
    s_depth_min_units = 0u;
    s_depth_max_units = 4095u;
    s_submission_counter = 0u;
    s_bound_texture = RENDER_TEXTURE_INVALID;
    s_clear_r = RENDER_CLEAR_DEFAULT_R;
    s_clear_g = RENDER_CLEAR_DEFAULT_G;
    s_clear_b = RENDER_CLEAR_DEFAULT_B;

    if (!hal_render_backend_init(width, height)) {
        // Окно/GL-контекст недоступны (после Platform_Init обязан быть).
        render_diag(RENDER_DIAG_NO_WINDOW);
        return false;
    }

    s_fb_width = width;
    s_fb_height = height;
    s_state = RENDER_STATE_READY;
    render_diag(RENDER_DIAG_OK);
    return true;
}

void Render_Shutdown(void)
{
    hal_render_backend_shutdown();
    s_state = RENDER_STATE_UNINITIALIZED;
    s_fb_width = 0u;
    s_fb_height = 0u;
    s_camera_set = false;
    s_bound_texture = RENDER_TEXTURE_INVALID;
    s_command_buffer.reset();
    memset(&s_diag, 0, sizeof(s_diag));
}

void Render_BeginFrame(void)
{
    if (s_state == RENDER_STATE_UNINITIALIZED) {
        assert(!"Render_BeginFrame до Render_Init (Lifecycle contract)");
        render_diag(RENDER_DIAG_NOT_INITIALIZED);
        return;
    }
    // Состояние кадра сбрасывается детерминированно; BeginFrame после
    // BeginFrame — просто новый чистый кадр (idempotent semantics).
    s_command_buffer.reset();
    s_submission_counter = 0u;
    render_diag(RENDER_DIAG_OK);
    s_state = RENDER_STATE_IN_FRAME;

    // Очистка reference-FBO цветом текущего кадра (default (0,0,32) или
    // последний явно установленный Render_Clear).
    hal_render_backend_begin_frame(s_clear_r, s_clear_g, s_clear_b);
}

void Render_Clear(uint8_t r, uint8_t g, uint8_t b)
{
    if (s_state == RENDER_STATE_UNINITIALIZED) {
        assert(!"Render_Clear до Render_Init (Lifecycle contract)");
        render_diag(RENDER_DIAG_NOT_INITIALIZED);
        return;
    }
    s_clear_r = r;
    s_clear_g = g;
    s_clear_b = b;
    // Немедленная очистка текущего(FBO или окна) назначения color buffer.
    hal_render_backend_clear(r, g, b);
    render_diag(RENDER_DIAG_OK);
}

void Render_EndFrame(void)
{
    if (s_state == RENDER_STATE_UNINITIALIZED) {
        // Распознанный normative-нарушитель: EndFrame до Init.
        assert(!"Render_EndFrame до Render_Init (Lifecycle contract)");
        render_diag(RENDER_DIAG_END_FRAME_BEFORE_INIT);
        return;
    }
    if (s_state == RENDER_STATE_READY) {
        // EndFrame без активного кадра — безопасный no-op с диагностикой.
        render_diag(RENDER_DIAG_END_FRAME_WITHOUT_BEGIN);
        return;
    }

    // ---- SORT (Painter's algorithm, normative §7.2) ---------------------
    const uint32_t tri_count = s_command_buffer.command_count();
    uint32_t sorted_count = 0u;
    for (uint32_t i = 0u; i < tri_count; ++i) {
        const RenderTriCmd* cmd = s_command_buffer.command(i);
        SortRef* ref = &s_sort_refs[sorted_count];
        ref->depth_key = cmd->depth_key;
        ref->submission_index = cmd->submission_index;
        ref->cmd_index = i;
        ++sorted_count;
    }
    // Вставка-сортировка (без <algorithm>: freestanding PS1; стабильная —
    // tie-breaker submission_index нормирует порядок равных ключей).
    for (uint32_t i = 1u; i < sorted_count; ++i) {
        const SortRef key = s_sort_refs[i];
        uint32_t j = i;
        while (j > 0u && sort_ref_before(key, s_sort_refs[j - 1u])) {
            s_sort_refs[j] = s_sort_refs[j - 1u];
            --j;
        }
        s_sort_refs[j] = key;
    }

    // ---- SUBMIT (после сортировки — детерминированный порядок) ----------
    for (uint32_t i = 0u; i < sorted_count; ++i) {
        const SortRef* ref = &s_sort_refs[i];
        const RenderTriCmd* cmd = s_command_buffer.command(ref->cmd_index);

        HalRenderBackendVertex verts[3];
        for (uint32_t v = 0u; v < 3u; ++v) {
            // fixed→float (граница бэкенда §4.4); UV/RGB — u8/255 (0..1).
            verts[v].x = fixed_to_float(cmd->x[v]);
            verts[v].y = fixed_to_float(cmd->y[v]);
            verts[v].z = fixed_to_float(cmd->z[v]);
            verts[v].u = (float)cmd->attr[v * 6u + 0u] / 255.0f;
            verts[v].v = (float)cmd->attr[v * 6u + 1u] / 255.0f;
            verts[v].r = (float)cmd->attr[v * 6u + 2u] / 255.0f;
            verts[v].g = (float)cmd->attr[v * 6u + 3u] / 255.0f;
            verts[v].b = (float)cmd->attr[v * 6u + 4u] / 255.0f;
            verts[v].a = (float)cmd->attr[v * 6u + 5u] / 255.0f;
        }

        float mvp_gl[16];
        render_mat4_to_gl(&cmd->mvp, mvp_gl);

        if (!hal_render_backend_draw_triangle(verts, mvp_gl, cmd->texture)) {
            // Явная ошибка submission (не silent drop): маркер кадра.
            render_diag(RENDER_DIAG_BACKEND_FAIL);
            break;
        }
    }

    // Выкладка FBO на окно (nearest) — независимо от числа треугольников.
    hal_render_backend_end_frame();
    s_state = RENDER_STATE_READY; // Кадр закрыт; следующий — BeginFrame.
}

// ---------------------------------------------------------------------------
// Camera / конвейерные хелперы
// ---------------------------------------------------------------------------

void Render_SetCamera(const Mat4* view, const Mat4* proj)
{
    if (s_state == RENDER_STATE_UNINITIALIZED) {
        assert(!"Render_SetCamera до Render_Init (Lifecycle contract)");
        render_diag(RENDER_DIAG_NOT_INITIALIZED);
        return;
    }
    if (view == NULL || proj == NULL) {
        render_diag(RENDER_DIAG_NULL_ARGUMENT);
        return;
    }

    s_view = *view;
    s_proj = *proj;
    if (!derive_near_far_from_projection()) {
        // Камера НЕ фиксируется: DrawMesh отказывается до валидной SetCamera
        // (детерминизм — никакой рисующей «мусорной» матрицной подмеси).
        s_camera_set = false;
        memset(&s_view, 0, sizeof(s_view));
        memset(&s_proj, 0, sizeof(s_proj));
        render_diag(RENDER_DIAG_INVALID_PROJECTION);
        return;
    }
    s_camera_set = true;
    render_diag(RENDER_DIAG_OK);
}

/** Экранные координаты SY-DOWN (PS1 origin top-left): fixed 16.16. */
typedef struct ScreenXY
{
    fixed_t sx;
    fixed_t sy;
} ScreenXY;

/**
 * Fixed-проекция view-вершины в экранные координаты (PS1-NCLIP-эквивалент):
 * через proj → деление на w → экран 320×240 (x вправо, y вниз). Клип-гейт
 * гарантирует w > 0; насыщение fixed за пределами фрустума детерминировано.
 *
 * @param view_p Вершина в view-space (w = FIXED_ONE).
 * @return Экран 0..320 / 0..240 в fixed (y ВНИЗ).
 */
static ScreenXY project_view_to_screen_sydown(Vec4 view_p)
{
    const Vec4 clip_p = mat4_transform(&s_proj, view_p);
    const fixed_t w = clip_p.w;
    // w = −z_view (у mat4_perspective m[11] = −1); w > 0 после клипа.
    const fixed_t ndc_x = fixed_div(clip_p.x, w);
    const fixed_t ndc_y = fixed_div(clip_p.y, w);

    ScreenXY result;
    // sx = (ndc_x + 1)/2·320 = ndc_x·160 + 160;
    // sy = (1 − (ndc_y + 1)/2)·240 = 120 − ndc_y·120  (y ВНИЗ).
    result.sx = fixed_add(fixed_mul(ndc_x, fixed_from_int(160)),
                          fixed_from_int(160));
    result.sy = fixed_sub(fixed_from_int(120),
                          fixed_mul(ndc_y, fixed_from_int(120)));
    return result;
}

/**
 * Знаковая площадь треугольника в экране (y вниз): A = (p1−p0)×(p2−p0).
 * A < 0 — фронт (CCW при y-вверх для наблюдателя), A ≥ 0 — backface.
 *
 * @param s0..s2 Экранные координаты (SY-DOWN).
 * @return Знаковая площадь (fixed; насыщение при экстремумах).
 */
static fixed_t screen_triangle_signed_area(ScreenXY s0, ScreenXY s1,
                                           ScreenXY s2)
{
    const fixed_t dx1 = fixed_sub(s1.sx, s0.sx);
    const fixed_t dy1 = fixed_sub(s1.sy, s0.sy);
    const fixed_t dx2 = fixed_sub(s2.sx, s0.sx);
    const fixed_t dy2 = fixed_sub(s2.sy, s0.sy);
    return fixed_sub(fixed_mul(dx1, dy2), fixed_mul(dy1, dx2));
}

// ---------------------------------------------------------------------------
// DrawMesh (TRANSFORM → CLIP → CULL → SORT-ключ → PACK)
// ---------------------------------------------------------------------------

void Render_DrawMesh(const Vertex* vertices, uint32_t vertex_count,
                     const Triangle* indices, uint32_t index_count,
                     const Mat4* model)
{
    if (s_state != RENDER_STATE_IN_FRAME) {
        // Draw вне кадра / до Init — диагностический no-op (release).
        render_diag(RENDER_DIAG_DRAW_OUTSIDE_FRAME);
        return;
    }
    if (vertices == NULL || indices == NULL || model == NULL
        || vertex_count == 0u || index_count == 0u) {
        render_diag(RENDER_DIAG_NULL_ARGUMENT);
        return;
    }
    if (!s_camera_set) {
        // Камера не валидирована (SetCamera не выполнен / проекция мусор):
        // детерминированный отказ рисования (FAILURE, не мусорные матрицы).
        render_diag(RENDER_DIAG_INVALID_PROJECTION);
        return;
    }
    // Индексы валидируются ДО dereference (malformed reject §1.2.7).
    for (uint32_t t = 0u; t < index_count; ++t) {
        const Triangle* tri = &indices[t];
        if (tri->v0 >= vertex_count || tri->v1 >= vertex_count
            || tri->v2 >= vertex_count) {
            render_diag(RENDER_DIAG_INVALID_VERTEX_INDEX);
            return; // Целый вызов отклоняется — частичной вставки нет.
        }
    }

    // Capacity-предпроверка (worst-case: все треугольники пройдут гейты) —
    // overflow = ЯВНАЯ ошибка, никакого частичного silent-сброса.
    const uint32_t free_items =
        RENDER_SORT_MAX - s_command_buffer.command_count();
    if (free_items < index_count) {
        render_diag(RENDER_DIAG_COMMAND_OVERFLOW);
        return;
    }

    Mat4 mv;
    mat4_multiply(&mv, &s_view, model); // view·model (column-major).
    Mat4 mvp;
    mat4_multiply(&mvp, &s_proj, &mv);  // proj·view·model — PACK-константа.

    bool any_reject = false; // last_code сохраняет reject-код вызова.
    for (uint32_t t = 0u; t < index_count; ++t) {
        const Triangle* tri = &indices[t];
        const Vertex* pv[3] = {&vertices[tri->v0], &vertices[tri->v1],
                               &vertices[tri->v2]};

        // ---- TRANSFORM (view-space, fixed 16.16) -------------------------
        Vec4 view_p[3];
        for (uint32_t v = 0u; v < 3u; ++v) {
            view_p[v] = mat4_transform(&mv, Vec4{pv[v]->x, pv[v]->y, pv[v]->z,
                                                 FIXED_ONE});
        }

        // ---- CLIP (near/far-гейт; whole-triangle reject) -----------------
        bool rejected = false;
        for (uint32_t v = 0u; v < 3u; ++v) {
            const fixed_t depth = fixed_sub(0, view_p[v].z); // d = −z_view
            if (fixed_sub(depth, s_near) < 0 || fixed_sub(s_far, depth) < 0) {
                ++s_diag.clip_rejects;
                render_diag(RENDER_DIAG_CLIP_REJECT);
                any_reject = true;
                rejected = true;
                break;
            }
        }
        if (rejected) {
            continue;
        }

        // ---- CULL (PS1-NCLIP-эквивалент) ---------------------------------
        const ScreenXY s0 = project_view_to_screen_sydown(view_p[0]);
        const ScreenXY s1 = project_view_to_screen_sydown(view_p[1]);
        const ScreenXY s2 = project_view_to_screen_sydown(view_p[2]);
        if (screen_triangle_signed_area(s0, s1, s2) >= 0) {
            // Backface (A ≥ 0): порядок normative — CULL после CLIP.
            ++s_diag.backface_rejects;
            render_diag(RENDER_DIAG_CULL_REJECT);
            any_reject = true;
            continue;
        }

        // ---- SORT-ключ: средняя camera-space depth в units ---------------
        const int64_t depth_sum_i64 = fixed_sat_add_i64(
            fixed_sat_add_i64((int64_t)fixed_sub(0, view_p[0].z),
                              (int64_t)fixed_sub(0, view_p[1].z)),
            (int64_t)fixed_sub(0, view_p[2].z));
        const fixed_t depth_avg = fixed_clamp_i64(depth_sum_i64 / 3);
        const uint32_t depth_key = depth_key_from_camera_z(
            fixed_to_int(depth_avg), s_depth_min_units, s_depth_max_units);
        if (depth_key == DEPTH_REJECT) {
            // «invalid depth rejection» — normative конформанс-случай.
            ++s_diag.depth_rejects;
            render_diag(RENDER_DIAG_DEPTH_REJECT);
            any_reject = true;
            continue;
        }

        // ---- PACK (per-tri команда; порядок вставки сохраняется) ---------
        RenderTriCmd cmd;
        cmd.mvp = mvp;
        // s_bound_texture == INVALID — валидный нетекстурированный путь:
        // бэкенд рисует flat-Gouraud (u_has_texture = 0).
        cmd.texture = s_bound_texture;
        cmd.depth_key = depth_key;
        cmd.submission_index = s_submission_counter;
        for (uint32_t v = 0u; v < 3u; ++v) {
            cmd.x[v] = view_p[v].x;
            cmd.y[v] = view_p[v].y;
            cmd.z[v] = view_p[v].z;
            cmd.attr[v * 6u + 0u] = pv[v]->u;
            cmd.attr[v * 6u + 1u] = pv[v]->v;
            cmd.attr[v * 6u + 2u] = pv[v]->r;
            cmd.attr[v * 6u + 3u] = pv[v]->g;
            cmd.attr[v * 6u + 4u] = pv[v]->b;
            cmd.attr[v * 6u + 5u] = pv[v]->a;
        }
        if (!s_command_buffer.draw_triangle(&cmd)) {
            // Overflow внутри вставки — явная ошибка кадра.
            render_diag(RENDER_DIAG_COMMAND_OVERFLOW);
            return;
        }
        ++s_submission_counter;
        ++s_diag.submitted;
    }
    if (!any_reject) {
        render_diag(RENDER_DIAG_OK); // Полностью успешный DrawMesh → OK.
    }
    // При reject'ах last_code сохраняет последний reject-код вызова.
}

// ---------------------------------------------------------------------------
// Текстуры (CreateTexture/Destroy/Bind — пул ядра + GPU-слой бэкенда)
// ---------------------------------------------------------------------------

TextureHandle Render_CreateTexture(uint32_t w, uint32_t h,
                                   const uint8_t* pixels,
                                   const uint16_t* palette)
{
    if (s_state != RENDER_STATE_READY && s_state != RENDER_STATE_IN_FRAME) {
        render_diag(RENDER_DIAG_NOT_INITIALIZED);
        return RENDER_TEXTURE_INVALID;
    }
    const TextureHandle handle = render_texture_acquire(w, h, pixels, palette);
    if (handle == RENDER_TEXTURE_INVALID) {
        render_diag(RENDER_DIAG_TEXTURE_FAIL);
        return RENDER_TEXTURE_INVALID;
    }
    // GPU-подготовка бэкендом (GL-загрузка пикселей/палитры): ядро само
    // читает данные из собственного пула и передаёт бэкенду (бэкенд не
    // ходит в пул — слоённый API). Отказ — слот освобождается, хэндл
    // не выдаётся (явный отказ без leak).
    const uint32_t pool_w = render_texture_width(handle);
    const uint32_t pool_h = render_texture_height(handle);
    const uint8_t* pool_pixels = render_texture_pixels(handle);
    const uint16_t* pool_palette = render_texture_palette(handle);
    if (pool_pixels == NULL || pool_palette == NULL
        || !hal_render_backend_create_texture(handle, pool_w, pool_h,
                                              pool_pixels, pool_palette)) {
        render_texture_release(handle);
        render_diag(RENDER_DIAG_BACKEND_FAIL);
        return RENDER_TEXTURE_INVALID;
    }
    render_diag(RENDER_DIAG_OK);
    return handle;
}

void Render_DestroyTexture(TextureHandle handle)
{
    if (s_state == RENDER_STATE_UNINITIALIZED) {
        assert(!"Render_DestroyTexture до Render_Init (Lifecycle contract)");
        render_diag(RENDER_DIAG_NOT_INITIALIZED);
        return;
    }
    hal_render_backend_destroy_texture(handle);
    render_texture_release(handle); // Идемпотентно (нет слота — no-op).
    if (s_bound_texture == handle) {
        s_bound_texture = RENDER_TEXTURE_INVALID;
    }
    render_diag(RENDER_DIAG_OK);
}

void Render_BindTexture(TextureHandle handle)
{
    if (s_state == RENDER_STATE_UNINITIALIZED) {
        assert(!"Render_BindTexture до Render_Init (Lifecycle contract)");
        render_diag(RENDER_DIAG_NOT_INITIALIZED);
        return;
    }
    // ЯВНЫЙ unbind (RENDER_TEXTURE_INVALID): перевод в нетекстурированный
    // режим — DrawMesh рисует Gouraud по вершинам без семплирования
    // (норматив недели 15: «polygon без texture рисуется flat»). Это
    // валидный путь (diag OK), а не отказ.
    if (handle == RENDER_TEXTURE_INVALID) {
        s_bound_texture = RENDER_TEXTURE_INVALID;
        render_diag(RENDER_DIAG_OK);
        return;
    }
    // Привязка = state ядра, проверяемая на PACK (данные должны существовать).
    if (render_texture_pixels(handle) == NULL) {
        render_diag(RENDER_DIAG_TEXTURE_FAIL);
        return;
    }
    s_bound_texture = handle;
    render_diag(RENDER_DIAG_OK);
}

// ---------------------------------------------------------------------------
// W0-конформанс: framebuffer readback + FNV-1a hash
// (PC-specialized: буфер 307 KB запрещён на PS1-стеке/рамке; golden PS1 —
// Спринт 15–16 перенесёт readback в PS1 backend.)
// ---------------------------------------------------------------------------

#if defined(PLATFORM_PC)

/** Размер RGBA-снимка reference-фреймбуфера (байт): 320×240×4. */
#define RENDER_READBACK_BYTES (RENDER_FB_WIDTH * RENDER_FB_HEIGHT * 4u)

/// Полный RGBA-снимок reference-фреймбуфера (static: тест-only на PC).
static uint8_t s_readback[RENDER_READBACK_BYTES];

bool Render_TestReadbackPixels(uint8_t* dst_rgba8, uint32_t dst_size)
{
    if (s_state == RENDER_STATE_UNINITIALIZED || dst_rgba8 == NULL
        || dst_size < RENDER_READBACK_BYTES) {
        render_diag(RENDER_DIAG_READBACK_FAIL);
        return false;
    }
    if (!hal_render_backend_read_pixels(dst_rgba8, dst_size)) {
        render_diag(RENDER_DIAG_READBACK_FAIL);
        return false;
    }
    return true;
}

uint64_t Render_TestReadbackHash(void)
{
    if (s_state == RENDER_STATE_UNINITIALIZED
        || !Render_TestReadbackPixels(s_readback, RENDER_READBACK_BYTES)) {
        return 0u;
    }

    // FNV-1a 64 над PS1-родными 15-битными пикселями (R>>3 | G>>3<<5 |
    // B>>3<<10; alpha игнорируется — MVP-Transparency off). Сжатие к 15 бит
    // гасит LSB-джиттер драйверов; хэш детерминирован.
    const uint64_t fnv_prime = UINT64_C(1099511628211);
    uint64_t hash = UINT64_C(14695981039346656037); // offset basis.
    const uint32_t pixel_count = RENDER_FB_WIDTH * RENDER_FB_HEIGHT;
    for (uint32_t i = 0u; i < pixel_count; ++i) {
        const uint8_t* px = &s_readback[i * 4u];
        const uint16_t packed =
            (uint16_t)(((uint16_t)(px[0] >> 3))
                       | ((uint16_t)(px[1] >> 3) << 5)
                       | ((uint16_t)(px[2] >> 3) << 10));
        hash ^= packed;
        hash *= fnv_prime;
    }
    return hash;
}

#else // PLATFORM_PS1: readback отсутствует до Спринта 15–16.

uint64_t Render_TestReadbackHash(void)
{
    render_diag(RENDER_DIAG_READBACK_FAIL);
    return 0u;
}

#endif // PLATFORM_PC
