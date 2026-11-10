// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/platform_saturn.cpp
//
// HAL Saturn (Sega Saturn, Master SH-2) — lifecycle/input/time (S23–24)
// + Slave worker init/shutdown (S25–26).
// Нормативные источники:
//   - docs/plans/sprint_23_24.md (недели 45–48);
//   - docs/adr/ADR-007 (no runtime heap), ADR-008 (Saturn backend);
//   - docs/SATURN.md.
//
// Jo Engine включается ТОЛЬКО через C-мост jo_saturn_bridge.c — SH-2/Jo
// Engine не попадают в hal/platform.h и остальные C++ translation units.
// CD-ROM ФС вынесена в filesystem_saturn.cpp, ввод — saturn_input.cpp,
// арены — saturn_memory.cpp.
#include "hal/platform.h"

#include "core/log/log.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/saturn_hal_internal.h"
#include "hal/saturn/saturn_input.h"
#include "hal/saturn/saturn_memory.h"

namespace {

// JO_COLOR_DarkBlue (0xA000): RGB555 + MSB; backdrop виден на VDP2 сразу.
constexpr uint16_t kDefaultBackdrop = static_cast<uint16_t>(RPE_SATURN_COLOR_RGB555 | (8u << 10));

bool s_platform_inited = false;

saturn::RawPadState saturn_pad_source(void)
{
    const RpeSaturnPadState raw = rpe_saturn_pad_read();
    saturn::RawPadState state;
    state.data = raw.data;
    state.connected = raw.connected;
    state.regular = raw.regular;
    return state;
}

}  // namespace

namespace saturn {

bool platform_ready(void)
{
    return s_platform_inited;
}

}  // namespace saturn

// ---------------------------------------------------------------------------
// Platform lifecycle
// ---------------------------------------------------------------------------

bool Platform_Init(void)
{
    if (s_platform_inited) {
        return true;
    }

    // Jo Engine core: VDP1/VDP2, 320x224 NTSC, SMPC/пады, VBlank, FS, heap.
    // Мост проверяет наблюдаемые инварианты (release не возвращает статус).
    if (!rpe_saturn_core_init(kDefaultBackdrop)) {
        // VDP2-лог ещё недоступен: core не инициализирован.
        return false;
    }

    Log_Init();  // Saturn sink всегда доступен (VDP2 text row 27).

    if (!saturn::memory_init()) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "platform init failed: memory arenas");
        Log_Shutdown();
        rpe_saturn_core_shutdown();
        return false;
    }

    saturn::input_init(&saturn_pad_source);

    if (!saturn::filesystem_init()) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "platform init failed: CD-ROM filesystem");
        saturn::input_shutdown();
        saturn::memory_shutdown();
        Log_Shutdown();
        rpe_saturn_core_shutdown();
        return false;
    }

    s_platform_inited = true;
    Log_Message(LOG_LEVEL_INFO, "saturn_hal", "platform init: 320x224 NTSC, pad + ISO9660 ready");
    return true;
}

void Platform_Shutdown(void)
{
    if (!s_platform_inited) {
        return;
    }
    s_platform_inited = false;

    Log_Message(LOG_LEVEL_INFO, "saturn_hal", "platform shutdown");
    saturn::filesystem_shutdown();
    saturn::input_shutdown();
    saturn::memory_shutdown();
    Log_Shutdown();
    rpe_saturn_core_shutdown();
}

bool Platform_PumpEvents(void)
{
    if (!s_platform_inited) {
        return true;
    }
    // Ровно один опрос пада за кадр; HAL-запросы читают защёлку.
    saturn::input_poll();
    return true;
}

void Platform_SwapBuffers(void)
{
    // Владение кадром/переключением VDP1-VDP2 — jo_core_run() (__jo_core_synch).
    // HAL не дублирует sync; VDP1-рендер появится в S27–28.
}

void Platform_SleepMs(uint32_t ms)
{
    if (ms == 0u || !s_platform_inited) {
        return;
    }
    const uint32_t start = rpe_saturn_ticks();
    while (static_cast<uint32_t>(rpe_saturn_ticks() - start) < ms) {
        // busy-wait: отдельного таймерного IRQ-контракта нет.
    }
}

uint32_t Platform_GetTimeMs(void)
{
    return s_platform_inited ? rpe_saturn_ticks() : 0u;
}

// ---------------------------------------------------------------------------
// Input (6-button pad; защёлка обновляется в Platform_PumpEvents)
// ---------------------------------------------------------------------------

bool Input_IsKeyDown(KeyCode key)
{
    (void)key;
    return false;  // На Saturn клавиатуры нет (keyboard-модуль не включён).
}

bool Input_IsPadButtonDown(uint32_t pad_index, PadButton button)
{
    return saturn::input_pad_button_down(pad_index, button);
}

int16_t Input_GetPadAxis(uint32_t pad_index, uint32_t axis)
{
    (void)pad_index;
    (void)axis;
    return 0;  // 6-button pad цифровой; 3D pad — вне scope v0.2.0.
}

bool Input_SetMouseRelativeMode(bool enable)
{
    (void)enable;
    return false;  // Saturn mouse — вне scope v0.2.0.
}

void Input_GetMouseDelta(int32_t* out_dx, int32_t* out_dy)
{
    if (out_dx != nullptr) {
        *out_dx = 0;
    }
    if (out_dy != nullptr) {
        *out_dy = 0;
    }
}

// ---------------------------------------------------------------------------
// Audio (SCSP — post-MVP; безопасные no-op)
// ---------------------------------------------------------------------------

bool Audio_Init(void)
{
    return false;
}

void Audio_Shutdown(void)
{
}

void Audio_PlayBeep(uint32_t frequency, uint32_t duration_ms)
{
    (void)frequency;
    (void)duration_ms;
}

// ---------------------------------------------------------------------------
// Compile-time контракты target profile
// ---------------------------------------------------------------------------

static_assert(saturn::WORK_RAM_BYTES == 2u * 1024u * 1024u, "Saturn HAL requires 2 MiB Work RAM");
