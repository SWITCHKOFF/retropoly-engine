// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/platform_ps1.cpp
//
// Реализация Platform_* HAL PS1 (PSn00bSDK v0.24, Спринты 13–14):
//   - Platform_Init      : ResetGraph(0) (GPU device) + InitGeom + pad + CD;
//   - Platform_PumpEvents: константный true (PS1 не имеет очереди событий;
//     единственный канал выхода — START-кнопка пада, читается главным циклом
//     напрямую через Input_IsPadButtonDown(0, INPUT_PAD_START));
//   - Platform_SwapBuffers: DrawSync(0) → VSync(0) → hal_ps1_backend_present
//     (flip disp-env + смена draw-буфера; DrawSync гарантирует завершение
//     OT-рисования ДО смены отображаемой области — без тиринга);
//   - Platform_GetTimeMs : виртуальное время от счётчика vsync-кадров
//     (PS1_FRAME_MS за показ; монотонность и wrap-безопасность — через
//     беззнаковую разность, как на PC/SDL_GetTicks);
//   - Platform_SleepMs   : занятое ожидание по тому же счётчику (без
//     таймер-IRQ: кадр и так ждёт vblank в SwapBuffers);
//   - Audio_*            : заглушка (SPU-PCM — post-MVP).
//
// PSn00bSDK включается ТОЛЬКО здесь (контракт hal/platform.h). Активные
// draw/disp-env живут в gpu_backend.cpp (double buffer A/B: disp[i] всегда
// показывает draw[1−i]).
#include "hal/platform.h"

#include "hal/ps1/hal_ps1_internal.h"
#include "core/log/log.h"

#include "core/log/log.h"

// SDK third-party: изоляция от -Wpedantic (§3.3).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <psxgpu.h>
#include <psxgte.h>
#include <inline_c.h>
#pragma GCC diagnostic pop

// ---------------------------------------------------------------------------
// Состояние (virtual time: счётчик кадров, НЕ системные часы)
// ---------------------------------------------------------------------------

static bool s_inited = false;
static uint32_t s_frames = 0;     /**< Число показанных кадров (vsync). */
static uint32_t s_time_ms = 0;    /**< Виртуальное время (s_frames·PS1_FRAME_MS). */

/** Единый owner-state HAL PS1 (ops-заполнение — hal_ps1_backend_init/
 *  shutdown в gpu_backend.cpp; здесь — определение объекта, контракт
 *  hal_ps1_internal.h). */
HalPs1BackendState hal_ps1_backend_state;

/** Период кадра в мс (NTSC ≈ 59.94 Гц; целочисленный тик детерминирован и не
 *  дрейфует внутри сессии — элапсы считаются беззнаковой разностью). */
#define PS1_FRAME_MS 16u

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

bool Platform_Init(void)
{
    if (s_inited) {
        return true; // Идемпотентность (повторный вызов допустим).
    }

    // GPU: device-сброс (полный reset состояния GPU + режим видео NTSC).
    ResetGraph(0);

    // GTE: per-примитивное состояние (identity/центр камеры) ставит
    // hal_ps1_adapter_reset при Render_Init; здесь — только InitGeom
    // (сброс флагов клампинга/лимитов GTE в дефолт SDK).
    InitGeom();

    // Периферия: геймпады (InitPAD/StartPAD — драйвер BIOS, буферы в
    // input_pad.cpp) и CD-ROM (CdInit — драйвер BIOS) по одной попытке.
    hal_ps1_input_init();
    hal_ps1_fs_init();

    s_frames = 0u;
    s_time_ms = 0u;
    s_inited = true;

    Log_Message(LOG_LEVEL_INFO, "ps1_hal", "platform init: GPU/GTE/CD/pad готовы");
    return true;
}

void Platform_Shutdown(void)
{
    if (!s_inited) {
        return;
    }
    // Порядок недели 9 (PC: геймпады → окно → SDL_Quit): pad-stop до GPU.
    hal_ps1_input_shutdown();
    s_frames = 0u;
    s_time_ms = 0u;
    s_inited = false;
    Log_Message(LOG_LEVEL_INFO, "ps1_hal", "platform shutdown");
}

// ---------------------------------------------------------------------------
// События / показ / время
// ---------------------------------------------------------------------------

bool Platform_PumpEvents(void)
{
    // PS1: очереди событий нет; главный цикл живёт до START (запрос выхода
    // читается напрямую Input_IsPadButtonDown в тике ввода).
    return true;
}

void Platform_SwapBuffers(void)
{
    if (!s_inited) {
        return;
    }
    // 1) Дождаться завершения GPU (OT-рисование прошлого кадра продолжается
    //    асинхронно с момента DrawOTag).
    // 2) Дождаться vblank (переключение disp-env в фазе vertical blank).
    // 3) hal_ps1_backend_present: PutDispEnv(next) + flip буфера.
    // 4) Учёт кадра: виртуальное время растёт РОВНО на период vsync.
    DrawSync(0);
    VSync(0);
    hal_ps1_backend_present();
    ++s_frames;
    s_time_ms += PS1_FRAME_MS;
}

void Platform_SleepMs(uint32_t ms)
{
    if (ms == 0u) {
        return; // Немедленный возврат.
    }
    if (!s_inited) {
        return; // До Init счётчик времени холодный (спин не определён).
    }
    const uint32_t deadline = Platform_GetTimeMs() + ms;
    while ((int32_t)(Platform_GetTimeMs() - deadline) < 0) {
        // Занятое ожидание (wrap-safe: знаковая разность uint32, как в PC).
    }
}

uint32_t Platform_GetTimeMs(void)
{
    return s_time_ms; // Монотонно растёт с каждым показанным кадром.
}

// ---------------------------------------------------------------------------
// Audio (заглушка SPU — post-MVP, контракт платформы)
// ---------------------------------------------------------------------------

bool Audio_Init(void)
{
    // PS1: PCM-микс — post-MVP (SPU). Заглушка всегда «успех»:
    // Audio_PlayBeep остаётся no-op.
    Log_Message(LOG_LEVEL_INFO, "ps1_hal", "audio init: stub (SPU post-MVP)");
    return true;
}

void Audio_Shutdown(void)
{
    // no-op (потока не было).
}

void Audio_PlayBeep(uint32_t frequency, uint32_t duration_ms)
{
    // no-op (SPU post-MVP).
    (void)frequency;
    (void)duration_ms;
}
