// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/hal_ps1_internal.h
//
// ПРИВАТНЫЙ внутренний заголовок HAL PS1 (PSn00bSDK v0.24, pinned).
// Реализация — Спринты 13–14 (docs/plans/sprint_13_14.md).
//
// Потребители: gpu_backend.cpp, gte_adapter.cpp, platform_ps1.cpp,
// input_pad.cpp, filesystem_cdrom.cpp, main_ps1.cpp. ЗАПРЕЩЁН во внешнем
// ядре (контракт render_backend.h: PSn00bSDK не включается нигде, кроме
// .cpp файлов бэкенда) и в PC-конфигурации: компилируется исключительно
// mipsel-none-elf-тулчейном пресета ps1-release.
//
// Разделы: конфигурация VRAM (размещение фреймбуферов, texture pages и
// CLUT), реестр текстур бэкенда, состояния подсистем HAL.
//
// GTE-конвенционные решения (норматив Спринтов 13–14):
//   - GTE-представление: Y и Z инвертируются относительно engine view-space
//     (pose normalizer D = diag(1,−1,−1): engine «Y вверх, камера смотрит
//     в −Z» → GTE «Y вниз, Z от камеры в глубину»). Отражение — точная
//     fixed-операция (умножение на −1 не вносит дрейфа; raw cast Mat4*→GTE
//     запрещён нормативом документации §4.4);
//   - GTE-матрица per-triangle: rotation = identity, translation =
//     D·Centroid·k (TR-регистры int32 держат большие дистанции в scale k;
//     входные SVECTOR-дельты v_i − Centroid всегда малы и не упираются в
//     предел ±8 единиц 12-битных SVECTOR'ов GTE).
#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// Константы рендера (значения согласованы с фреймбуфером ядра 320×240).
// ---------------------------------------------------------------------------

/** Число бакетов Ordering Table (static uint32_t[HAL_PS1_OT_LEN], DMA-chained). */
#define HAL_PS1_OT_LEN 1024u

/** Ширина/высота фреймбуфера (должны совпадать с RENDER_FB_W/H ядра). */
#define HAL_PS1_FB_WIDTH 320u
#define HAL_PS1_FB_HEIGHT 240u

/**
 * Порог Divider GTE: проекция H·2^17/(2·SZ3) не насыщает к 0x1FFFF при
 * SZ3 ≥ H/2 (проверенное условие nocash psx-spx; всё, что ближе, Core уже
 * отбросил near-клипом). k выбирается так, чтобы k·near ≥ H/2 c запасом
 * слака MAC-суммирования:
 */
#define HAL_PS1_DIVIDER_SAFE_MARGIN_RAW 4u

/** Верхняя граница SZ3 (16-bit unsigned;slack для округления floor-сумм). */
#define HAL_PS1_SZ3_RAW_MAX 65532u

/** Длина буфера PS1-pad'а (байт; контракт psxapi.h InitPAD). */
#define HAL_PS1_PAD_BUFFER_BYTES 34u

/** Максимум пада (PS1-паритет ядра: 2 слота Digital/DualShock). */
#define HAL_PS1_PAD_COUNT 2u

/** Размер единого packet-pool кадра в gpu_backend.cpp (байт). */
#define HAL_PS1_GPU_POOL_SIZE (32u * 1024u)

// ---------------------------------------------------------------------------
// VRAM layout (1024×512 halfwords). Дважды-проверенное размещение Спринтов
// 13–14 (пересечений нет):
//
//   x [000..320) y [000..240) : фреймбуфер A (16bpp, отображается в disp[0])
//   x [320..640) y [000..240) : фреймбуфер B (16bpp)
//   x [000..256) y [240..245) : колонка CLUT (5 строк, по одной на текстуру)
//   x [384..896) y [256..512) : texture pages, 8bpp (5 слотов по 128×256 hw)
//
//   Свободный остаток: x 0..383 y 245..511, x 0..1024 y 272..511 (запас).
//   «10 слотов» предварительного плана невозможно: полоса y 0..239 занята
//   фреймбуферами, страницы требуют ровно 256 строк (geометрия tpage) —
//   максимум 5 слотов в единственной полосе y 256..511.
//
// Двойной буфер (A/B) активен всегда: GPU рисует в одну область, дисплей
// читает вторую — без тиринга при любом тайминге кадра; PutDispEnv
// переключает области в Platform_SwapBuffers на VSync.
// ---------------------------------------------------------------------------

/** База texture page по X (hw; кратна 128 — выравнивание 8bpp-страницы). */
#define HAL_PS1_TPAGE_X_BASE 384u

/** Строка единственной полосы texture pages (hw; ровно 256 строк до 512). */
#define HAL_PS1_TPAGE_Y_BAND 256u

/** Ширина texture page 8bpp (hw): 256 px = 128 halfwords. */
#define HAL_PS1_TPAGE_W_HW 128u

/** Высота texture page (hw; геометрия GPU tpage: всегда 256). */
#define HAL_PS1_TPAGE_H_HW 256u

/** Слотов texture page (X-шаг 128 hw в полосе [384..896)). */
#define HAL_PS1_TPAGE_SLOTS 5u

/** База CLUT-колонки по X (hw; 0 — левое поле, не пересекает страницы). */
#define HAL_PS1_CLUT_X 0u

/** База CLUT-строк по Y (hw 240..244 — щель под фреймбуферами). */
#define HAL_PS1_CLUT_Y_BASE 240u

/** Слотов CLUT (по одной на активную текстуру: ровно область). */
#define HAL_PS1_CLUT_SLOTS 5u

/** Максимум одновременных VRAM-текстур бэкенда (= слоты страниц). */
#define HAL_PS1_TEXTURE_MAX HAL_PS1_TPAGE_SLOTS

// ---------------------------------------------------------------------------
// Реестр VRAM-текстур gpu_backend (создаётся из пары CI8-пиксели + палитра)
// ---------------------------------------------------------------------------

/**
 * Запись VRAM-текстуры бэкенда PS1 (слот индексируется TextureHandle − 1;
 * 8bpp-текстуры позиционируются СТРОГО на базу texture page: UV-атрибуты
 * engine 0..255 масштабируются в texel-координаты (dim−1)/255 без выхода
 * за границы страницы — полная совместимость с GL-CLAMP_TO_EDGE на PC).
 */
typedef struct HalPs1TextureEntry
{
    uint32_t in_use;  /**< Слот занят (1) или свободен (0). */
    uint32_t page_x;  /**< База texture page по X (hw; кратна 64). */
    uint32_t page_y;  /**< База texture page по Y (hw; полосy y=256). */
    uint32_t width;   /**< Ширина текстуры px (64/128/256) — UV-conversion. */
    uint32_t height;  /**< Высота текстуры px (64/128/256). */
    uint16_t tpage;   /**< Значение getTPage(8bpp, abr 0, page_x, page_y). */
    uint16_t clut;    /**< Значение getClut(0, 240 + слот CLUT). */
} HalPs1TextureEntry;

/** Состояние(VRAM) бэкенда — объявлено gpu_backend.cpp'ом и раздано
 *  подсистемам через platform_ps1.cpp (единый owner state). */
typedef struct HalPs1BackendState
{
    /** Реестр VRAM-текстур (слот = TextureHandle − 1). */
    HalPs1TextureEntry textures[HAL_PS1_TEXTURE_MAX];
} HalPs1BackendState;

/** Общий state-объект HAL PS1 (определён в platform_ps1.cpp). */
extern HalPs1BackendState hal_ps1_backend_state;

/** Рабочее состояние GTE-адаптера (камера подана — set_camera). */
typedef struct HalPs1AdapterState
{
    uint32_t inited;    /**< set_camera выполнен (валидная проекция). */
    int32_t k_scale;    /**< GTE-масштаб k: engine units → raw GTE. */
    int32_t h_pixels;   /**< H = f·(высота/2) в пикселях (из proj). */
    uint32_t depth_min; /**< int(near) — базис бакетов OT (units). */
    uint32_t depth_max; /**< int(far) — граница диапазона (units). */
} HalPs1AdapterState;

/** Общий state GTE-адаптера (определён в gte_adapter.cpp). */
extern HalPs1AdapterState hal_ps1_adapter_state;

// ---------------------------------------------------------------------------
// Привязки подсистем (hooks): определяются рядом со «своими» подсистемами —
// present/backing flip — gpu_backend.cpp, pad-буферы — input_pad.cpp,
// CdInit — filesystem_cdrom.cpp; вызываются из platform_ps1.cpp (владелец
// device-lifecycle: ResetGraph/InitGeom/StartPAD/CD-host-инициализация).
// ---------------------------------------------------------------------------

/**
 * Показ выстроенного кадра: PutDispEnv(next disp-env) + flip активного
 * draw-буфера (вызывается ПОСЛЕ DrawSync(0)+VSync(0) — блокировки в
 * Platform_SwapBuffers, чтобыClear/OT-draw успели завершиться до смены).
 */
void hal_ps1_backend_present(void);

/** InitPAD + StartPAD (pad-буферы живут в input_pad.cpp). */
void hal_ps1_input_init(void);

/** StopPAD (symmetric shutdown; безопасен без Init). */
void hal_ps1_input_shutdown(void);

/** CdInit + guard повторного вызова (CRD-драйвер BIOS). */
void hal_ps1_fs_init(void);
