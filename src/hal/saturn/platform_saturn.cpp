// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/platform_saturn.cpp
//
// HAL Saturn (Sega Saturn, Master SH-2) — skeleton Спринта 21–22.
// Нормативные источники:
//   - docs/plans/sprint_21_22.md (недели 41–44);
//   - docs/adr/ADR-008-saturn-backend.md;
//   - docs/SATURN.md.
//
// Границы v0.2.0 (S21–22): lifecycle + однотонный VDP2 backdrop. VDP1,
// 3D, ввод, CD-ROM и SCSP реализуются в S23–32. Jo Engine включается ТОЛЬКО
// через C-мост jo_saturn_bridge.c — SH-2/Jo Engine не попадают в
// hal/platform.h и остальные C++ translation units.
//
// Память: только static storage; new/delete/malloc/free в runtime запрещены
// (ADR-007). Бюджет и static_asserts — saturn_memory.h.
#include "hal/platform.h"

#include "core/log/log.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/saturn_memory.h"

namespace {

// JO_COLOR_DarkBlue (0xA000): RGB555 + MSB; backdrop виден на VDP2 сразу.
constexpr uint16_t kDefaultBackdrop = static_cast<uint16_t>(RPE_SATURN_COLOR_RGB555 | (8u << 10));

bool s_platform_inited = false;

uint32_t pad_button_bit(PadButton button)
{
    switch (button) {
        case INPUT_PAD_UP:
            return RPE_SATURN_PAD_UP;
        case INPUT_PAD_DOWN:
            return RPE_SATURN_PAD_DOWN;
        case INPUT_PAD_LEFT:
            return RPE_SATURN_PAD_LEFT;
        case INPUT_PAD_RIGHT:
            return RPE_SATURN_PAD_RIGHT;
        case INPUT_PAD_CROSS:
            return RPE_SATURN_PAD_A;
        case INPUT_PAD_CIRCLE:
            return RPE_SATURN_PAD_B;
        case INPUT_PAD_SQUARE:
            return RPE_SATURN_PAD_C;
        case INPUT_PAD_TRIANGLE:
            return RPE_SATURN_PAD_X;
        case INPUT_PAD_L1:
            return RPE_SATURN_PAD_Y;
        case INPUT_PAD_R1:
            return RPE_SATURN_PAD_Z;
        case INPUT_PAD_L2:
            return RPE_SATURN_PAD_L;
        case INPUT_PAD_R2:
            return RPE_SATURN_PAD_R;
        case INPUT_PAD_START:
            return RPE_SATURN_PAD_START;
        case INPUT_PAD_SELECT:  // 6-button Saturn pad не имеет Select.
        case INPUT_PAD_COUNT:
        default:
            return 0u;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Platform lifecycle
// ---------------------------------------------------------------------------

bool Platform_Init(void)
{
    if (s_platform_inited) {
        return true;
    }

    // Jo Engine core: сброс VDP1/VDP2, 320x224 NTSC, SMPC/пады, VBlank,
    // jo_malloc-арена. Ошибок возврата нет: при сбое Jo Engine показывает
    // собственный error screen (JO_DEBUG=0 в release — без текста).
    rpe_saturn_core_init(kDefaultBackdrop);

    s_platform_inited = true;
    Log_Message(LOG_LEVEL_INFO, "saturn_hal", "platform init: VDP2 backdrop 320x224x60");
    return true;
}

void Platform_Shutdown(void)
{
    if (!s_platform_inited) {
        return;
    }
    s_platform_inited = false;
    Log_Shutdown();
}

bool Platform_PumpEvents(void)
{
    // jo_core_run() сам опрашивает SMPC/VBlank; отдельной очереди событий нет.
    return true;
}

void Platform_SwapBuffers(void)
{
    // Владение кадром/переключением VDP1-VDP2 — jo_core_run() (__jo_core_synch).
    // До S23–24 HAL не имеет собственного main loop и не дублирует sync.
}

void Platform_SleepMs(uint32_t ms)
{
    if (ms == 0u) {
        return;
    }
    const uint32_t start = rpe_saturn_ticks();
    while (static_cast<uint32_t>(rpe_saturn_ticks() - start) < ms) {
        // busy-wait: таймерного IRQ-контракта в skeleton нет.
    }
}

uint32_t Platform_GetTimeMs(void)
{
    return rpe_saturn_ticks();
}

// ---------------------------------------------------------------------------
// Input (6-button pad; state обновляет Jo Engine в VBlank)
// ---------------------------------------------------------------------------

bool Input_IsKeyDown(KeyCode key)
{
    (void)key;
    return false;  // На Saturn клавиатуры нет (keyboard-модуль не включён).
}

bool Input_IsPadButtonDown(uint32_t pad_index, PadButton button)
{
    if (pad_index != 0u) {
        return false;
    }
    const uint32_t bit = pad_button_bit(button);
    return bit != 0u && (rpe_saturn_pad_state() & bit) != 0u;
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
    return false;  // Saturn mouse — вне scope S21–22.
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
// FileSystem (ISO9660 read-only — S23–24)
// ---------------------------------------------------------------------------

FileData FileSystem_LoadFile(const char* path)
{
    (void)path;
    FileData empty = {nullptr, 0u};
    return empty;
}

void FileSystem_FreeFile(FileData* file)
{
    if (file != nullptr) {
        file->data = nullptr;
        file->size = 0u;
    }
}

// ---------------------------------------------------------------------------
// Compile-time контракты target profile
// ---------------------------------------------------------------------------

static_assert(saturn::WORK_RAM_BYTES == 2u * 1024u * 1024u, "Saturn HAL requires 2 MiB Work RAM");
