// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/gpu_backend.cpp
//
// Реализация внутреннего рендер-бэкенда HAL PS1 (Спринты 13–14): GPU
// (psxgpu), Ordering Table (Painter's far→near), единый packet-pool, VRAM
// реестр текстур (8bpp-страница + CLUT-строка), связка с GTE-адаптером
// (hal/ps1/gte_adapter).
//
// КОНТРАКТ — src/hal/render_backend.h. Ядро доставляет ровно один треугольник
// за вызов В ПОРЯДКЕ Painter's-подачи (depth_key DESC, tie submission ASC);
// бэкенд НЕ re-sort:
//   - bucket = depth_key ядра (та же fixed-математика depth_avg units −
//     depth_min, re-derived из float-вершин: обратный snap точный);
//   - вставка = per-bucket TAIL-append (порядок рисования внутри бакета =
//     порядок вставки = submission ASC tie-break);
//   - инвариант non-increasing bucket: рост = явный false.
//
// Кадровый поток:
//   begin_frame  : DrawSync(0) → ClearOTagR(1024) → reset pool/cursors →
//                  PutDrawEnv(isbg=1) (цвет очистки кадра)
//   draw_triangle: GTE-адаптер → repack POLY_GT3/G3 → OT cursor
//   end_frame    : DrawOTag(ot + OT_LEN−1): DMA-цепочка ведёт буфер 1023 → 0
//                  (дальние рисуются первыми, ближние поверх — Painter's)
//   SwapBuffers  : DrawSync(0) → VSync(0) → PutDispEnv (double buffer A/B)
//
// PSn00bSDK (psxgpu.h) включается ТОЛЬКО здесь (контракт render_backend.h).
#include "hal/render_backend.h"

#include "hal/ps1/hal_ps1_internal.h"
#include "hal/ps1/gte_adapter.h"
#include "render/renderer.h" // RENDER_FB_*, RENDER_CLEAR_DEFAULT_*, depth_key API

// Third-party SDK-заголовки: изоляция от -Wpedantic (§3.3).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <psxgpu.h>
#pragma GCC diagnostic pop

#include "core/log/log.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Память GPU-пайплайна (static storage: НЕ CPU-стек, §5.4)
// ---------------------------------------------------------------------------

/** Ordering Table (1024 бакета; chain ClearOTagR / DrawOTag(от 1023 вниз)). */
alignas(8) uint32_t s_ot[HAL_PS1_OT_LEN];

/** Единый packet-pool кадра (32 KiB; POLY_GT3 = 40 Б, POLY_G3 = 28 Б). */
alignas(8) uint8_t s_pool[HAL_PS1_GPU_POOL_SIZE];
static uint32_t s_pool_used = 0u;   /**< Использованных байт. */
static uint32_t s_pool_count = 0u;  /**< Примитивов в кадре (диагностика). */

/** Tail-cursors бакетов OT: указатель на последний открытый tag-слот. */
static P_TAG* s_tail[HAL_PS1_OT_LEN];

/** Предыдущий bucket (0xFFFFFFFF — ещё не было; non-increasing check). */
static uint32_t s_last_bucket;

/** Двух-буферная пара A/B: draw[i] @ 320·i; disp[i] @ 320·(1−i). */
static DRAWENV s_draw_env[2];
static DISPENV s_disp_env[2];
static uint32_t s_cur = 0u; /**< Активный draw-буфер (display показывает другой). */

/** Статус инициализации (Render_Init lifecycle). */
static bool s_inited = false;

/** Реестр VRAM-текстур (owner — platform_ps1.cpp: hal_ps1_backend_state). */
extern HalPs1BackendState hal_ps1_backend_state;

// ---------------------------------------------------------------------------
// Packet-pool и OT-курсор
// ---------------------------------------------------------------------------

/**
 * Выделение примитива из pool'а. Все стратегии psxgpu кратны 4 байтам
 * (40/28), база pool'а 8-выровнена → каждый примитив 4-выровнен (достаточно
 * для swc2-строк GTE и LoadImage-DMA). Откат на CULLED — pool_used -= bytes.
 *
 * @param bytes Размер в байтах (sizeof(POLY_GT3) или sizeof(POLY_G3)).
 * @return слот или NULL при переполнении (явный отказ кадра).
 */
static void* pool_alloc(uint32_t bytes)
{
    if (s_pool_used + bytes > HAL_PS1_GPU_POOL_SIZE) {
        return NULL; // Явный отказ БЕЗ silent drop.
    }
    void* prim = &s_pool[s_pool_used];
    s_pool_used += bytes;
    ++s_pool_count;
    return prim;
}

/**
 * Tail-append примитива в Ordering Table (bucket @p bucket).
 *
 * Курсор s_tail[b] указывает на ОТКРЫТЫЙ tag-слот (изначально ot[b]; после
 * вставки — tag последнего примитива). Вставка:
 *   prim.tag.addr = курсор.addr (дальняя часть цепи не меняется);
 *   курсор.addr   = addr(prim);
 *   курсор        = prim.tag.
 * Читатель chain'а (DrawOTag от 1023 к 0): примитивы одного бакета рисуются
 * В ПОРЯДКЕ ВСТАВКИ (submission ASC tie-break), инвариант длины/len примитива
 * и слота не трогается (P_TAG.addr — 24-битное поле адреса, len сохранён).
 *
 * @param bucket Бакет (валидность проверяет вызывающая сторона).
 * @param prim   Заполненный примитив (setlen уже применён).
 */
static void otag_push(uint32_t bucket, void* prim)
{
    P_TAG* cursor = s_tail[bucket];
    setaddr(prim, getaddr(cursor));
    setaddr(cursor, (uint32_t)(uintptr_t)prim);
    s_tail[bucket] = (P_TAG*)prim;
}

// ---------------------------------------------------------------------------
// Атрибутика репака (float → u8 через fixed 16.16 — детерминированно)
// ---------------------------------------------------------------------------

/**
 * Восстановление u8-атрибута из float (ядро конвертировало u8/255):
 * (uf·255 + 0.5)>>16 — round-half-up даёт ИСХОДНОЕ u8 побитно
 * (float snappy fixed 16.16 + целочисленный round — без потерь).
 */
static inline uint8_t attr_u8_from_float01(float f)
{
    const fixed_t uf = fixed_from_float(f);
    return (uint8_t)(((int64_t)uf * 255 + 32768) >> 16);
}

/**
 * Конверсия UV-атрибута (u8 0..255 → texel-координата в 8bpp-странице).
 * texel = floor((u/255)·dim), clamp dim−1 (GL-CLAMP_TO_EDGE эквивалент;
 * GTE интерполирует UV ВНУТРИ hull'а вершин — wrap вне страницы невозможен).
 *
 * @param u_fixed_from_float01 snapped (u/255) в fixed.
 * @param dim Размер текстуры по измерению (64/128/256).
 * @return texel-координата 0..dim−1.
 */
static inline uint8_t uv_to_texel_u8(fixed_t u_fixed, uint32_t dim)
{
    // u_fixed = trunc((engine_u/255)·65536) ⇒ texel = floor(u_fixed·dim/2^16).
    const uint32_t texel = (uint32_t)(((int64_t)u_fixed * (int64_t)dim) / (int64_t)FIXED_ONE);
    return (texel >= dim) ? (uint8_t)(dim - 1u) : (uint8_t)texel;
}

// ---------------------------------------------------------------------------
// Инициализация / shutdown
// ---------------------------------------------------------------------------

bool hal_render_backend_init(uint32_t width, uint32_t height)
{
    if (s_inited) {
        return true; // Идемпотентность; протокол разрешает один вызов.
    }
    // Фреймбуфер фиксирован ядром (320×240); разладка — ошибка ABI.
    if (width != RENDER_FB_WIDTH || height != RENDER_FB_HEIGHT) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "init: несовпадение фреймбуфера %ux%u ≠ %ux%u",
                    (unsigned)width, (unsigned)height, RENDER_FB_WIDTH, RENDER_FB_HEIGHT);
        return false;
    }

    // GPU/taxi device-lifecycle (ResetGraph, InitGeom) — владение
    // Platform_Init; бэкенд ставит только env-конфигурацию кадра.

    // Двойной буфер: draw A @ (0,0) показывает disp B @ (320,0) — и наоборот.
    for (uint32_t i = 0; i < 2u; ++i) {
        SetDefDrawEnv(&s_draw_env[i], (int)(RENDER_FB_WIDTH * i), 0, (int)RENDER_FB_WIDTH,
                      (int)RENDER_FB_HEIGHT);
        s_draw_env[i].isbg = 1; // Очистка области в PutDrawEnv (BeginFrame).
        s_draw_env[i].dfe = 0;
        setRGB0(&s_draw_env[i], RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G,
                RENDER_CLEAR_DEFAULT_B);

        SetDefDispEnv(&s_disp_env[i], (int)(RENDER_FB_WIDTH * (1u - i)), 0, (int)RENDER_FB_WIDTH,
                      (int)RENDER_FB_HEIGHT);
    }
    s_cur = 0u;
    PutDrawEnv(&s_draw_env[s_cur]);
    // disp_env[s_cur] указывает на draw_env[1 - s_cur]; при s_cur=0 рисуем в
    // A(0,0), показываем B(320,0). Переключение — в hal_ps1_backend_present.
    PutDispEnv(&s_disp_env[s_cur]);

    // Ordering table: «R» chain (reversed): рисование от 1023 к 0.
    for (uint32_t b = 0; b < HAL_PS1_OT_LEN; ++b) {
        s_tail[b] = (P_TAG*)&s_ot[b];
    }
    SetDispMask(1); // Показ активен (после ResetGraph).

    // GTE: identity + нулевые съезды (до set_camera draws отклонены).
    hal_ps1_adapter_reset();
    memset(&hal_ps1_backend_state, 0, sizeof(hal_ps1_backend_state));

    s_pool_used = 0u;
    s_pool_count = 0u;
    s_last_bucket = 0xFFFFFFFFu;
    s_inited = true;

    Log_Message(LOG_LEVEL_INFO, "ps1_hal",
                "backend PS1 init: OT %u бакетов, pool %u KiB, ~%u GT3 max", HAL_PS1_OT_LEN,
                HAL_PS1_GPU_POOL_SIZE / 1024u, HAL_PS1_GPU_POOL_SIZE / (uint32_t)sizeof(POLY_GT3));
    return true;
}

void hal_render_backend_shutdown(void)
{
    if (!s_inited) {
        return;
    }
    DrawSync(0);
    hal_ps1_adapter_reset();
    memset(&hal_ps1_backend_state, 0, sizeof(hal_ps1_backend_state));
    s_pool_used = 0u;
    s_pool_count = 0u;
    s_last_bucket = 0xFFFFFFFFu;
    s_cur = 0u;
    s_inited = false;
    Log_Message(LOG_LEVEL_INFO, "ps1_hal", "backend shutdown");
}

// ---------------------------------------------------------------------------
// Кадровый цикл
// ---------------------------------------------------------------------------

void hal_render_backend_begin_frame(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_inited) {
        return; // Защитный no-op (контракт: draw вне Init — диагностика вверх).
    }

    // GPU idle перед переинициализацией OT (предыдущий кадр мог дорисовываться).
    DrawSync(0);

    ClearOTagR(s_ot, HAL_PS1_OT_LEN);
    for (uint32_t b = 0; b < HAL_PS1_OT_LEN; ++b) {
        s_tail[b] = (P_TAG*)&s_ot[b];
    }
    s_pool_used = 0u;
    s_pool_count = 0u;
    s_last_bucket = 0xFFFFFFFFu;

    // Цвет очистки текущей области + PutDrawEnv (isbg = 1 — fill кадра).
    setRGB0(&s_draw_env[s_cur], r, g, b);
    PutDrawEnv(&s_draw_env[s_cur]);
}

void hal_render_backend_clear(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_inited) {
        return;
    }
    // Повторная очистка (semantics Render_Clear): PutDrawEnv с тем же isbg = 1
    // воссоздаёт fill RGB(r,g,b). Детерминированно; двойной clear = тот же пиксель.
    setRGB0(&s_draw_env[s_cur], r, g, b);
    PutDrawEnv(&s_draw_env[s_cur]);
}

void hal_render_backend_end_frame(void)
{
    if (!s_inited) {
        return;
    }
    // DMA-цепочка: начинается на(bucket) 1023 и спускается к 0 — Painter's.
    // Пустой кадр = детерминированная холостая цепочка (без ветвлений).
    DrawOTag(s_ot + (HAL_PS1_OT_LEN - 1u));
}

void hal_render_backend_set_camera(const Mat4* view, const Mat4* proj)
{
    hal_ps1_adapter_set_camera(view, proj);
}

// ---------------------------------------------------------------------------
// Present (flip) — вызывается Platform_SwapBuffers после DrawSync(0)+VSync(0)
// ---------------------------------------------------------------------------

void hal_ps1_backend_present(void)
{
    if (!s_inited) {
        return;
    }
    // disp[i] адресует область draw[1−i]: показ задним числом построенного
    // кадра (текущий s_cur) + смена активного draw-буфера. PutDispEnv
    // статусов не возвращает (env-запись — одно GPU-слово, без DMA-цепи).
    PutDispEnv(&s_disp_env[1u - s_cur]);
    s_cur ^= 1u;
}

// ---------------------------------------------------------------------------
// Текстуры (VRAM: 8bpp-страница + CLUT-строка)
// ---------------------------------------------------------------------------

bool hal_render_backend_create_texture(TextureHandle handle, uint32_t w, uint32_t h,
                                       const uint8_t* pixels_ci8, const uint16_t* palette)
{
    if (!s_inited || pixels_ci8 == NULL || palette == NULL || handle == RENDER_TEXTURE_INVALID) {
        return false;
    }

    // PS1-контракт: размеры — сет 64/128/256 (Страница 256×256 нужна
    // CLAMP-семантике; пиксели пишутся в её верхний левый угол).
    if ((w != 64u && w != 128u && w != 256u) || (h != 64u && h != 128u && h != 256u)) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "create_texture: размер %ux%u вне {64,128,256}",
                    (unsigned)w, (unsigned)h);
        return false;
    }

    const uint32_t handle_raw = handle - 1u;
    if (handle_raw < HAL_PS1_TEXTURE_MAX &&
        hal_ps1_backend_state.textures[handle_raw].in_use != 0u) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "create_texture: хэндл %u уже создан",
                    (unsigned)handle);
        return false;
    }

    uint32_t slot = HAL_PS1_TEXTURE_MAX; // first-free — детерминировано.
    for (uint32_t i = 0; i < HAL_PS1_TEXTURE_MAX; ++i) {
        if (hal_ps1_backend_state.textures[i].in_use == 0u) {
            slot = i;
            break;
        }
    }
    if (slot == HAL_PS1_TEXTURE_MAX) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "create_texture: VRAM слоты переполнены (%u)",
                    HAL_PS1_TEXTURE_MAX);
        return false; // Явный отказ — диагностическая цепочка fail-fast.
    }

    const uint32_t page_x = HAL_PS1_TPAGE_X_BASE + HAL_PS1_TPAGE_W_HW * slot;
    const uint32_t page_y = HAL_PS1_TPAGE_Y_BAND;

    // 8bpp-страница: rect W = w/2 halfword (2 px на hw), H = h строк.
    // Данные 4-выровнены (арена ассетов, alignment 8 по умолчанию).
    // Байт-слой CI8 == halfword-grid: левый пиксель — младший байт.
    RECT page_rect = {(int16_t)page_x, (int16_t)page_y, (int16_t)(w / 2u), (int16_t)h};
    // LoadImage: 0 — перенос запущен сразу, >0 — позиция в очереди, -1 — отказ.
    if (LoadImage(&page_rect, (const uint32_t*)pixels_ci8) < 0) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "create_texture: LoadImage отказ (page)");
        return false;
    }

    // CLUT: палитра 256 halfwords в строке slot.
    const uint32_t clut_y = HAL_PS1_CLUT_Y_BASE + slot;
    RECT clut_rect = {(int16_t)HAL_PS1_CLUT_X, (int16_t)clut_y, 256, 1};
    if (LoadImage(&clut_rect, (const uint32_t*)palette) < 0) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "create_texture: LoadImage отказ (CLUT)");
        return false;
    }

    HalPs1TextureEntry* entry = &hal_ps1_backend_state.textures[handle_raw];
    entry->in_use = 1u;
    entry->page_x = page_x;
    entry->page_y = page_y;
    entry->width = w;
    entry->height = h;
    entry->tpage = (uint16_t)getTPage(1, 0, page_x, page_y); // mode 1 = 8bpp.
    entry->clut = (uint16_t)getClut(HAL_PS1_CLUT_X, clut_y);

    Log_Message(LOG_LEVEL_INFO, "ps1_hal",
                "create_texture: хэндл %u → слот %u (page x=%u y=%u, clut y=%u), %ux%u",
                (unsigned)handle, (unsigned)slot, (unsigned)page_x, (unsigned)page_y,
                (unsigned)clut_y, (unsigned)w, (unsigned)h);
    return true;
}

void hal_render_backend_destroy_texture(TextureHandle handle)
{
    if (!s_inited || handle == RENDER_TEXTURE_INVALID) {
        return;
    }
    const uint32_t handle_raw = handle - 1u;
    if (handle_raw >= HAL_PS1_TEXTURE_MAX) {
        return;
    }
    if (hal_ps1_backend_state.textures[handle_raw].in_use == 0u) {
        return; // Идемпотентность; невалидные — no-op.
    }
    memset(&hal_ps1_backend_state.textures[handle_raw], 0, sizeof(HalPs1TextureEntry));
    Log_Message(LOG_LEVEL_INFO, "ps1_hal", "destroy_texture: хэндл %u освобождён",
                (unsigned)handle);
}

// ---------------------------------------------------------------------------
// Draw (GTE-адаптер + repack POLY_GT3/G3 + OT)
// ---------------------------------------------------------------------------

/**
 * Откат выделения при CULLED (слоты пула не «пачкаются» брошенными
 * примитивами; безопасно: между alloc и rollback других выделений нет).
 */
static void pool_rollback(uint32_t bytes)
{
    s_pool_used -= bytes;
    --s_pool_count;
}

bool hal_render_backend_draw_triangle(const HalRenderBackendVertex vertices[3], const float* mvp_gl,
                                      TextureHandle texture)
{
    (void)mvp_gl; // PS1-конвенция: проекция в GTE-адаптере (mvp — только PC).

    if (!s_inited || vertices == NULL) {
        return false;
    }

    // ---- Bucket: та же fixed-математика ядра (depth_avg units) -------------
    // Входные float = точные fixed 16.16 (обратный snap — gte_adapter.h);
    // для глубин ≤ 255 units битовая точность 24-битного mantissa полная.
    const fixed_t z0 = fixed_from_float(vertices[0].z);
    const fixed_t z1 = fixed_from_float(vertices[1].z);
    const fixed_t z2 = fixed_from_float(vertices[2].z);
    const int64_t depth_sum =
        fixed_sat_add_i64(fixed_sat_add_i64((int64_t)fixed_sub(0, z0), (int64_t)fixed_sub(0, z1)),
                          (int64_t)fixed_sub(0, z2));
    const fixed_t depth_avg = fixed_clamp_i64(depth_sum / 3);
    const uint32_t bucket = depth_key_from_camera_z(
        fixed_to_int(depth_avg), hal_ps1_adapter_state.depth_min, hal_ps1_adapter_state.depth_max);
    if (bucket == DEPTH_REJECT || bucket >= HAL_PS1_OT_LEN) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "draw_triangle: bucket вне OT (depth invalid)");
        return false; // Явный отказ — диагностическая цепочка fail-fast.
    }

    // ---- Painter's non-increasing bucket check ------------------------------
    if (s_last_bucket != 0xFFFFFFFFu && bucket > s_last_bucket) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal",
                    "draw_triangle: bucket растёт %u → %u (Painter's порядок нарушен)",
                    (unsigned)s_last_bucket, (unsigned)bucket);
        return false;
    }

    if (texture == RENDER_TEXTURE_INVALID) {
        // ---- POLY_G3 (нетекстурированный Gouraud; 7 слов = 28 Б) —--------
        POLY_G3* p = (POLY_G3*)pool_alloc(sizeof(POLY_G3));
        if (p == NULL) {
            Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "pool переполнен (%u примитивов)",
                        (unsigned)s_pool_count);
            return false;
        }
        setPolyG3(p); // setlen(6) + code 0x30.

        const HalPs1AdapterStatus st = hal_ps1_adapter_transform(vertices, p, 0u);
        if (st == HAL_PS1_ADAPTER_CULLED) {
            pool_rollback(sizeof(POLY_G3));
            return true; // Нормальная CULL-стадия; БЕЗ диагностики вниз.
        }
        if (st != HAL_PS1_ADAPTER_OK) {
            return false; // fail-fast: RENDER_DIAG_BACKEND_FAIL вверх.
        }

        setRGB0(p, attr_u8_from_float01(vertices[0].r), attr_u8_from_float01(vertices[0].g),
                attr_u8_from_float01(vertices[0].b));
        p->r1 = attr_u8_from_float01(vertices[1].r);
        p->g1 = attr_u8_from_float01(vertices[1].g);
        p->b1 = attr_u8_from_float01(vertices[1].b);
        p->r2 = attr_u8_from_float01(vertices[2].r);
        p->g2 = attr_u8_from_float01(vertices[2].g);
        p->b2 = attr_u8_from_float01(vertices[2].b);

        otag_push(bucket, p);
        return true;
    }

    // ---- POLY_GT3 (textured Gouraud; 10 слов = 40 Б) —-----------------------
    if (texture - 1u >= HAL_PS1_TEXTURE_MAX ||
        hal_ps1_backend_state.textures[texture - 1u].in_use == 0u) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "draw_triangle: текстура %u не существует",
                    (unsigned)texture);
        return false;
    }
    const HalPs1TextureEntry* tex = &hal_ps1_backend_state.textures[texture - 1u];

    POLY_GT3* p = (POLY_GT3*)pool_alloc(sizeof(POLY_GT3));
    if (p == NULL) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "pool переполнен (%u примитивов)",
                    (unsigned)s_pool_count);
        return false;
    }
    setPolyGT3(p); // setlen(9) + code 0x34.

    const HalPs1AdapterStatus st = hal_ps1_adapter_transform(vertices, p, 1u);
    if (st == HAL_PS1_ADAPTER_CULLED) {
        pool_rollback(sizeof(POLY_GT3));
        return true;
    }
    if (st != HAL_PS1_ADAPTER_OK) {
        return false;
    }

    // Gouraud-цвет по вершинам (u8→float→u8 побитно без потерь).
    setRGB0(p, attr_u8_from_float01(vertices[0].r), attr_u8_from_float01(vertices[0].g),
            attr_u8_from_float01(vertices[0].b));
    p->r1 = attr_u8_from_float01(vertices[1].r);
    p->g1 = attr_u8_from_float01(vertices[1].g);
    p->b1 = attr_u8_from_float01(vertices[1].b);
    p->r2 = attr_u8_from_float01(vertices[2].r);
    p->g2 = attr_u8_from_float01(vertices[2].g);
    p->b2 = attr_u8_from_float01(vertices[2].b);

    // UV: engine-координата [0..255] → texel floor((u/255)·dim), clamp dim−1.
    const fixed_t u0 = fixed_from_float(vertices[0].u);
    const fixed_t v0 = fixed_from_float(vertices[0].v);
    const fixed_t u1 = fixed_from_float(vertices[1].u);
    const fixed_t v1 = fixed_from_float(vertices[1].v);
    const fixed_t u2 = fixed_from_float(vertices[2].u);
    const fixed_t v2 = fixed_from_float(vertices[2].v);
    setUV3(p, uv_to_texel_u8(u0, tex->width), uv_to_texel_u8(v0, tex->height),
           uv_to_texel_u8(u1, tex->width), uv_to_texel_u8(v1, tex->height),
           uv_to_texel_u8(u2, tex->width), uv_to_texel_u8(v2, tex->height));

    // Диспозиция texture page / CLUT из реестра.
    p->clut = tex->clut;
    p->tpage = tex->tpage;

    otag_push(bucket, p);
    return true;
}
