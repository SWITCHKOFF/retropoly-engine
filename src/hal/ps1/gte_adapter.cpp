// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/gte_adapter.cpp
//
// Реализация GTE-адаптера HAL PS1 (Спринты 13–14). ЕДИНСТВЕННОЕ место
// конвертации engine-view-space в конвенцию GTE:
//
//   1. Pose normalizer D = diag(1,−1,−1) в fixed (y вниз, z в глубину) —
//      ТОЧНАЯ переприписка знака (без дрейфа; raw cast Mat4*→GTE запрещён).
//   2. Centroid (среднее 3 вершин, fixed) масштабируется k → TR-регистры
//      (int32; демпфы «большой дистанции» — за пределами ±8-единиц SVECTOR).
//   3. Дельты (v − Centroid) масштабируются k → SVECTOR (int16).
//   4. Rotation = identity (1.3.12): MAC = TR + V — точная арифметика
//      (4096·V >> 12 == V без потерь для всего диапазона int16).
//   5. rtpt → nclip (gate по MAC0) → stsxy3_{gt3,g3} — запись SXY в примитив.
//
// Fail-fast: любое нарушение представления (int16 IR/SZ clamp, Divider
// насыщение, GPU-окно координат) — FAIL без clamp; ядро прерывает Submission
// с RENDER_DIAG_BACKEND_FAIL. NCLIP ≥ 0 — нормальная CULL-стадия (CULLED).
//
// Ссылки на формулы GTE — psx-spx (nocash) + SDK examples/graphics/gte.
#include "hal/ps1/gte_adapter.h"

#include "core/log/log.h"

// SDK-заголовки third-party: анонимные структуры psxpad/psxapi против ISO C++
// (-Wpedantic) изолируются (§3.3: third-party не под каноническими флагами).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <psxgte.h>
#include <inline_c.h>
#pragma GCC diagnostic pop

#include <string.h>

// ---------------------------------------------------------------------------
// Состояние (shared-инварианты; extern — hal_ps1_internal.h)
// ---------------------------------------------------------------------------

HalPs1AdapterState hal_ps1_adapter_state; // Основное состояние (shared).

static bool s_camera_valid = false; /**< Камера провалидирована (draw горит). */
static int32_t s_h_pixels = 0;      /**< H (регистр C2_H; h/2 после f). */
static int32_t s_k = 0;             /**< Масштаб raw-единиц GTE (целое 1..65532). */
static int32_t s_ofx = 0;           /**< OFX = центр фреймбуфера (px). */
static int32_t s_ofy = 0;           /**< OFY (px). */

/** int16[10] identity-матрицы 4.12 для gte_SetRotMatrix (lhu читает +16..17). */
alignas(4) int16_t s_rot_identity[10];

// Границы per-draw проверок (см. gte_adapter.h):
#define ADAPTER_IR_MAX 32766        /**< |x·k|, |y·k| максимум (слак ±1 MAC). */
#define ADAPTER_SVECTOR_MAX 32767   /**< |v−c|·k максимум (int16 feed). */
#define ADAPTER_TR_MAX 524287       /**< |Centroid·k| максимум (int32 MAC). */
#define ADAPTER_SCREEN_MAX_PX 1023  /**< |sx|,|sy| максимум (GPU-окно). */

/**
 * fixed 16.16 → raw GTE int (умножение на целочисленный масштаб k).
 * Промежуточное — int64; усечение К НУЛЮ (та же семантика, что fixed.h).
 *
 * @param v Значение fixed 16.16.
 * @param scale Целочисленный масштаб k (1..65532).
 * @return ⌊v·scale⌋ с усечением к нулю (переполнение int32 невозможно:
 *         v ∈ int32, scale ≤ 65532: |v·k| ≤ 2^31·2^16 в int64, приведение
 *         результата вызывающая сторона проверяет).
 */
static inline int32_t adapter_scale_raw(fixed_t v, int32_t scale)
{
    const int64_t product = (int64_t)v * (int64_t)scale;
    if (product >= 0) {
        return (int32_t)(product >> 16);
    }
    return -(int32_t)((-product) >> 16);
}

// ---------------------------------------------------------------------------
// near/far: обратный вывод матрицы проекции (та же fixed-математика ядра)
// ---------------------------------------------------------------------------

/**
 * Вычисление near/far из проекции (копия логики derive_near_far_from_
 * projection ядра; дублирование целенаправленное: бэкенд не ходит в ядро).
 *
 * @param proj Матрица проекции (column-major, fixed).
 * @param out_near Выход: near в fixed (valid при true).
 * @param out_far Выход: far в fixed (valid при true).
 * @return true — валидная проекция (near > 0, far > near, знаменатели ≠ 0).
 */
static bool adapter_derive_near_far(const Mat4* proj, fixed_t* out_near, fixed_t* out_far)
{
    const fixed_t a = proj->m[10];
    const fixed_t b = proj->m[14];
    const fixed_t den_near = fixed_sub(a, FIXED_ONE);
    const fixed_t den_far = fixed_add(a, FIXED_ONE);
    if (den_near == 0 || den_far == 0) {
        return false;
    }
    const fixed_t near_z = fixed_div(b, den_near);
    const fixed_t far_z = fixed_div(b, den_far);
    if (near_z <= 0 || far_z <= near_z) {
        return false;
    }
    *out_near = near_z;
    *out_far = far_z;
    return true;
}

// ---------------------------------------------------------------------------
// set_camera: H + near/far + селекция масштаба k
// ---------------------------------------------------------------------------

void hal_ps1_adapter_set_camera(const Mat4* view, const Mat4* proj)
{
    (void)view; // PS1-конвенция: вершины приходят view-space; view не читается.

    s_camera_valid = false;
    hal_ps1_adapter_state.inited = 0u;
    hal_ps1_adapter_state.k_scale = 0;
    hal_ps1_adapter_state.h_pixels = 0;
    hal_ps1_adapter_state.depth_min = 0u;
    hal_ps1_adapter_state.depth_max = 0u;

    if (proj == NULL) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "set_camera: proj NULL — draws отклоняются");
        return;
    }

    // ---- near/far (дублирование вывода ядра — целенаправленное) ----------
    fixed_t near_z = 0;
    fixed_t far_z = 0;
    if (!adapter_derive_near_far(proj, &near_z, &far_z)) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal",
                    "set_camera: проекция неисправима (near/far не выведены)");
        return;
    }

    // ---- H = f·(h/2), f = m[5] = cot(fov_y/2); 240/2 = 120 ----------------
    const fixed_t h_fixed = fixed_mul(proj->m[5], fixed_from_int(120));
    const int32_t h_px = fixed_to_int(h_fixed);
    if (h_px < 16 || h_px > 4095) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "set_camera: H вне [16..4095] (%d px)",
                    (int)h_px);
        return;
    }

    // ---- Селекция масштаба k (обе границы) --------------------------------
    // Нижняя: Divider GTE выдаёт точный результат только при SZ3 ≥ H/2
    // (psx-spx: DIVRES=((H·2^17/SZ3)+1)/2 насыщается к 0x1FFFF при SZ3 < H/2).
    // Минимальный SZ3 = k·near − slack MAC-сумм (≤2): k ≥ (H/2 + margin + 2).
    const int32_t half_h_margin =
        (int32_t)((h_px + 1) / 2 + (int32_t)HAL_PS1_DIVIDER_SAFE_MARGIN_RAW + 2);
    const int64_t k_min = (((int64_t)half_h_margin) * FIXED_ONE + (int64_t)near_z - 1) /
                          (int64_t)near_z; // ceil-деление (делимое/делитель > 0)

    // Верхняя (SZ3): max SZ3 = k·far (+2 slack округлений) ≤ SZ3_RAW_MAX.
    const int64_t k_max_depth =
        ((int64_t)HAL_PS1_SZ3_RAW_MAX * FIXED_ONE) / (int64_t)far_z; // floor

    // Верхняя (IR1/IR2): координата далеко в фрустуме x ≤ far·tan_h,
    // tan_h = 1/m[0] (fixed): x_max = far/m[0]; k·x_max ≤ 32766.
    int64_t k_max_coord = INT64_MAX;
    if (proj->m[0] <= 0) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "set_camera: m[0] ≤ 0 (невалидный aspect)");
        return;
    }
    const fixed_t x_max_fixed = fixed_div(far_z, proj->m[0]);
    if (x_max_fixed <= 0) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "set_camera: x_max ≤ 0 (m[0]/far конфликт)");
        return;
    }
    k_max_coord = ((int64_t)ADAPTER_IR_MAX * FIXED_ONE) / (int64_t)x_max_fixed; // floor

    int64_t upper = (k_max_depth < k_max_coord) ? k_max_depth : k_max_coord;
    if (upper < 1) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal",
                    "set_camera: верхняя граница k < 1 (экстремальная проекция)");
        return;
    }
    if (k_min > upper) {
        // Конфликт границ (аномально широкая глубина для одного k): политика
        // fail-fast — приоритет границы Divider (ближний-план всегда корректен;
        // дальний fail является per-draw ЯВНО, без saturation-мусора).
        upper = k_min;
        Log_Message(LOG_LEVEL_WARNING, "ps1_hal",
                    "set_camera: границы k конфликтуют — k = нижняя (дальний план пойдёт"
                    " в явный fail per-draw)");
    }

    s_k = (int32_t)upper;
    s_h_pixels = h_px;
    s_ofx = (int32_t)(HAL_PS1_FB_WIDTH / 2);
    s_ofy = (int32_t)(HAL_PS1_FB_HEIGHT / 2);
    s_camera_valid = true;

    hal_ps1_adapter_state.inited = 1u;
    hal_ps1_adapter_state.k_scale = s_k;
    hal_ps1_adapter_state.h_pixels = s_h_pixels;
    hal_ps1_adapter_state.depth_min = (uint32_t)fixed_to_int(near_z); // bucket-база
    hal_ps1_adapter_state.depth_max = (uint32_t)fixed_to_int(far_z);

    // Регистры GTE: H, экранное смещение, identity-вращение (константные
    // данные кадра — них не зависит от per-draw состояния).
    gte_SetGeomScreen(s_h_pixels);
    gte_SetGeomOffset(s_ofx, s_ofy);
    gte_SetRotMatrix(&s_rot_identity[0]);

    Log_Message(LOG_LEVEL_INFO, "ps1_hal",
                "set_camera: H=%d px, near=%u/{far=%u units}, k=%d",
                (int)s_h_pixels, (unsigned)hal_ps1_adapter_state.depth_min,
                (unsigned)hal_ps1_adapter_state.depth_max, (int)s_k);
}

// ---------------------------------------------------------------------------
// reset: identity-вращение + нулевые смещения (без камеры)
// ---------------------------------------------------------------------------

void hal_ps1_adapter_reset(void)
{
    // 1.3.12 identity: R11 = R22 = R33 = 4096, прочие 0 (линейная запись m).
    static const int16_t k_identity[9] = {4096, 0, 0, 0, 4096, 0, 0, 0, 4096};
    memcpy(&s_rot_identity[0], &k_identity[0], sizeof(k_identity));

    s_ofx = (int32_t)(HAL_PS1_FB_WIDTH / 2);
    s_ofy = (int32_t)(HAL_PS1_FB_HEIGHT / 2);
    s_h_pixels = 0;
    s_k = 0;
    s_camera_valid = false;

    memset(&hal_ps1_adapter_state, 0, sizeof(hal_ps1_adapter_state));

    // GTE-регистры: смещение центра + identity (per-camera H ставится в
    // set_camera; до первой камеры draws отклоняются по состояния).
    gte_SetGeomOffset(s_ofx, s_ofy);
    gte_SetRotMatrix(&s_rot_identity[0]);
}

// ---------------------------------------------------------------------------
// transform: rtpt → nclip → stsxy
// ---------------------------------------------------------------------------

HalPs1AdapterStatus hal_ps1_adapter_transform(const float* vx, const float* vy, const float* vz,
                                              void* prim, uint32_t textured)
{
    if (!s_camera_valid || prim == NULL || vx == NULL || vy == NULL || vz == NULL) {
        return HAL_PS1_ADAPTER_FAIL; // Камера не подана / входы невалидны.
    }

    // ---- Граница бэкенда: float → fixed (§4.4; non-finite → 0 explicit) ---
    fixed_t fx[3];
    fixed_t fz[3]; // dzps = −z_view (глубина d > 0 после клип-гейта ядра)
    fixed_t fy[3]; // dy = −y_view (SY-DOWN конвенция GTE)
    for (uint32_t i = 0; i < 3u; ++i) {
        fx[i] = fixed_from_float(vx[i]);
        fy[i] = fixed_sub(0, fixed_from_float(vy[i]));  // D: y ↦ −y
        fz[i] = fixed_sub(0, fixed_from_float(vz[i]));  // D: z ↦ +d (в глубину)
    }

    // ---- Centroid (координатный центр треугольника; avg fixed 16.16) -----
    const fixed_t cx = fixed_add(fixed_add(fx[0], fx[1]), fx[2]) / 3;
    const fixed_t cy = fixed_add(fixed_add(fy[0], fy[1]), fy[2]) / 3;
    const fixed_t cz = fixed_add(fixed_add(fz[0], fz[1]), fz[2]) / 3;

    // ---- Перевод в raw-units GTE (int32 TR; int16 SVECTOR) ----------------
    const int32_t trx = adapter_scale_raw(cx, s_k);
    const int32_t tryy = adapter_scale_raw(cy, s_k);
    const int32_t trz = adapter_scale_raw(cz, s_k);
    if (trx > ADAPTER_TR_MAX || trx < -ADAPTER_TR_MAX || tryy > ADAPTER_TR_MAX ||
        tryy < -ADAPTER_TR_MAX || trz > ADAPTER_TR_MAX || trz < -ADAPTER_TR_MAX) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "TR вне int32 MAC-диапазона (sf=1)");
        return HAL_PS1_ADAPTER_FAIL;
    }

    // Дельты (v − Centroid) в SVECTOR + per-vertex полные величины
    // (TR + delta = то, что GTE суммирует в MAC — проверяем именно эту сумму).
    alignas(4) int16_t svec[3][4]; // [vx, vy, vz, pad] × 3 (gte_ldv3 stride 8 Б)
    for (uint32_t i = 0; i < 3u; ++i) {
        const int32_t dvx = adapter_scale_raw(fixed_sub(fx[i], cx), s_k);
        const int32_t dvy = adapter_scale_raw(fixed_sub(fy[i], cy), s_k);
        const int32_t dvz = adapter_scale_raw(fixed_sub(fz[i], cz), s_k);
        if (dvx > ADAPTER_SVECTOR_MAX || dvx < -ADAPTER_SVECTOR_MAX ||
            dvy > ADAPTER_SVECTOR_MAX || dvy < -ADAPTER_SVECTOR_MAX ||
            dvz > ADAPTER_SVECTOR_MAX || dvz < -ADAPTER_SVECTOR_MAX) {
            Log_Message(LOG_LEVEL_ERROR, "ps1_hal",
                        "дельта v−Centroid вне int16 SVECTOR (dv=%d,%d,%d)",
                        (int)dvx, (int)dvy, (int)dvz);
            return HAL_PS1_ADAPTER_FAIL;
        }

        // Полные величины (MAC-сумма GTE): IR1/IR2 ±32766 (насыщение ±1 slack),
        // SZ3 — unsigned 16-bit [2..65532].
        const int32_t mac_x = trx + dvx;
        const int32_t mac_y = tryy + dvy;
        const int32_t mac_z = trz + dvz;
        if (mac_x > ADAPTER_IR_MAX || mac_x < -ADAPTER_IR_MAX || mac_y > ADAPTER_IR_MAX ||
            mac_y < -ADAPTER_IR_MAX) {
            Log_Message(LOG_LEVEL_ERROR, "ps1_hal",
                        "x·k/y·k вне IR1/IR2 int16 (mac_x=%d)", (int)mac_x);
            return HAL_PS1_ADAPTER_FAIL;
        }
        if (mac_z < 2 || mac_z > (int32_t)HAL_PS1_SZ3_RAW_MAX) {
            Log_Message(LOG_LEVEL_ERROR, "ps1_hal",
                        "глубина d·k вне SZ3 [2..65532] (mac_z=%d)", (int)mac_z);
            return HAL_PS1_ADAPTER_FAIL;
        }

        // Экранный кластер: sx = OFX + H·x/d. Граница |sx| ≤ 1023 px
        // (GPU-окно; защита от wrap 12.4-halfword при |sx| ≥ 2048) даёт
        // симметричную достаточную дугу H·|x|/d ≤ 1023 − OFX (= 863 px).
        const int64_t ax = (int64_t)(mac_x < 0 ? -mac_x : mac_x);
        const int64_t ay = (int64_t)(mac_y < 0 ? -mac_y : mac_y);
        const int64_t az = (int64_t)mac_z;
        const int64_t screen_swing_px =
            (int64_t)ADAPTER_SCREEN_MAX_PX - (int64_t)s_ofx;
        if (ax * (int64_t)s_h_pixels > az * screen_swing_px ||
            ay * (int64_t)s_h_pixels > az * screen_swing_px) {
            Log_Message(LOG_LEVEL_ERROR, "ps1_hal",
                        "вершина вне GPU-окна ±1023 px (H·|x| > d·(1023−OFX))");
            return HAL_PS1_ADAPTER_FAIL;
        }

        svec[i][0] = (int16_t)dvx;
        svec[i][1] = (int16_t)dvy;
        svec[i][2] = (int16_t)dvz;
        svec[i][3] = 0;
    }

    // ---- GTE: TR → вектора → rtpt (все 3 вершины) → nclip -----------------
    gte_ldtr(trx, tryy, trz);
    gte_ldv3(&svec[0][0], &svec[1][0], &svec[2][0]);
    gte_rtpt();
    gte_nclip();
    int32_t nclip = 0;
    gte_stopz(&nclip); // MAC0: тот же крест-формула, что core signed area.
    if (nclip >= 0) {
        // NCLIP ≥ 0 ⇔ знаковая площадь ≥ 0 ⇔ backface (знак совпадает с
        // ядерным cull'ом). Нормальная стадия конвейера; примитив НЕ добавляется.
        return HAL_PS1_ADAPTER_CULLED;
    }

    // ---- Запись экранных координат в примитив (swc2 SXY0..2) --------------
    if (textured != 0u) {
        gte_stsxy3_gt3(prim); // офсеты 8/20/32 (POLY_GT3: x0y0/x1y1/x2y2)
    } else {
        gte_stsxy3_g3(prim); // офсеты 8/16/24 (POLY_G3)
    }
    return HAL_PS1_ADAPTER_OK;
}
