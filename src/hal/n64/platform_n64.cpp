// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/platform_n64.cpp
//
// HAL Nintendo 64 — lifecycle/input/time/FS (S39–42, ADR-014).
// Нормативные источники:
//   - docs/plans/sprint_39_40.md, docs/plans/sprint_41_42.md;
//   - docs/adr/ADR-014-n64-backend.md;
//   - docs/N64.md.
//
// Scope S41–42: VI 320×240 16-bit (NTSC 60 Hz), ровно один joypad-опрос за
// кадр, монотонное время, read-only DFS + .mesh/.tex boot-загрузка, memory
// ledger/арены, render skeleton и bounded RDP command buffer. RSP/RDP-
// геометрия, TMEM/текстуры и engine-overlay — S43–44.
#include "hal/platform.h"

#include "core/log/log.h"
#include "hal/n64/n64_assets.h"
#include "hal/n64/n64_bridge.h"
#include "hal/n64/n64_fs.h"
#include "hal/n64/n64_input.h"
#include "hal/n64/n64_memory.h"

namespace {

bool s_platform_inited = false;
bool s_fs_available = false;
uint32_t s_prev_buttons[n64::INPUT_PORT_COUNT] = {0u, 0u, 0u, 0u};
bool s_prev_connected[n64::INPUT_PORT_COUNT] = {false, false, false, false};

const char* input_button_tag(uint32_t bit)
{
    switch (bit) {
        case RPE_N64_BTN_A:
            return "A";
        case RPE_N64_BTN_B:
            return "B";
        case RPE_N64_BTN_Z:
            return "Z";
        case RPE_N64_BTN_START:
            return "START";
        case RPE_N64_BTN_D_UP:
            return "D_UP";
        case RPE_N64_BTN_D_DOWN:
            return "D_DOWN";
        case RPE_N64_BTN_D_LEFT:
            return "D_LEFT";
        case RPE_N64_BTN_D_RIGHT:
            return "D_RIGHT";
        case RPE_N64_BTN_L:
            return "L";
        case RPE_N64_BTN_R:
            return "R";
        case RPE_N64_BTN_C_UP:
            return "C_UP";
        case RPE_N64_BTN_C_DOWN:
            return "C_DOWN";
        case RPE_N64_BTN_C_LEFT:
            return "C_LEFT";
        case RPE_N64_BTN_C_RIGHT:
            return "C_RIGHT";
        default:
            return "?";
    }
}

void log_input_edges()
{
    for (uint32_t port = 0u; port < n64::INPUT_PORT_COUNT; ++port) {
        const bool connected = n64::input_connected(port);
        if (connected != s_prev_connected[port]) {
            Log_Message(LOG_LEVEL_INFO, "n64_input", "pad%u %s", static_cast<unsigned>(port),
                        connected ? "connected" : "disconnected");
            s_prev_connected[port] = connected;
        }
        const uint32_t buttons = n64::input_buttons(port);
        const uint32_t pressed = buttons & ~s_prev_buttons[port];
        if (pressed != 0u) {
            for (uint32_t bit = 0u; bit < 14u; ++bit) {
                if ((pressed & (1u << bit)) != 0u) {
                    Log_Message(LOG_LEVEL_INFO, "n64_input", "pad%u press %s",
                                static_cast<unsigned>(port), input_button_tag(1u << bit));
                }
            }
        }
        s_prev_buttons[port] = buttons;
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

    if (!rpe_n64_core_init()) {
        return false;
    }

    // Debug-канал готов до первого сообщения (ISViewer/USB).
    Log_Init();

    if (!n64::memory_init()) {
        Log_Message(LOG_LEVEL_ERROR, "n64_hal", "memory ledger check failed");
        n64::memory_shutdown();
        Log_Shutdown();
        rpe_n64_core_shutdown();
        return false;
    }

    n64::input_init();
    n64::assets_shutdown();
    for (uint32_t i = 0u; i < n64::INPUT_PORT_COUNT; ++i) {
        s_prev_buttons[i] = 0u;
        s_prev_connected[i] = false;
    }

    s_fs_available = n64::fs_init();
    if (s_fs_available) {
        (void)n64::assets_load_boot();
    }

    s_platform_inited = true;
    Log_Message(LOG_LEVEL_INFO, "n64_hal",
                "platform init: VI 320x240 16-bit NTSC, joypad, DFS=%s, arenas=%u B",
                s_fs_available ? "ready" : "unavailable", (unsigned)n64::ENGINE_ARENAS_BYTES);
    n64::memory_log_stats();
    return true;
}

void Platform_Shutdown(void)
{
    if (!s_platform_inited) {
        return;
    }
    s_platform_inited = false;

    Log_Message(LOG_LEVEL_INFO, "n64_hal", "platform shutdown");
    n64::assets_shutdown();
    n64::fs_shutdown();
    n64::input_shutdown();
    n64::memory_shutdown();
    s_fs_available = false;

    Log_Shutdown();
    rpe_n64_core_shutdown();
}

bool Platform_PumpEvents(void)
{
    if (!s_platform_inited) {
        return true;
    }
    // Ровно один опрос контроллеров за кадр (S41–42).
    n64::input_poll();
    log_input_edges();
    return true;
}

void Platform_SwapBuffers(void)
{
    // Кадром владеет render backend (rpe_n64_frame_begin/end) или boot-demo;
    // разделение владений как на Saturn (ADR-008).
}

void Platform_SleepMs(uint32_t ms)
{
    if (ms == 0u || !s_platform_inited) {
        return;
    }
    rpe_n64_sleep_ms(ms);
}

uint32_t Platform_GetTimeMs(void)
{
    return s_platform_inited ? rpe_n64_ticks_ms() : 0u;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool Input_IsKeyDown(KeyCode key)
{
    (void)key;
    return false;  // На N64 клавиатуры нет.
}

bool Input_IsPadButtonDown(uint32_t pad_index, PadButton button)
{
    return n64::input_pad_button_down(pad_index, button);
}

int16_t Input_GetPadAxis(uint32_t pad_index, uint32_t axis)
{
    return n64::input_pad_axis(pad_index, axis);
}

bool Input_SetMouseRelativeMode(bool enable)
{
    (void)enable;
    return false;  // Мыши на N64 нет.
}

void Input_GetMouseDelta(int32_t* out_dx, int32_t* out_dy)
{
    if (out_dx != NULL) {
        *out_dx = 0;
    }
    if (out_dy != NULL) {
        *out_dy = 0;
    }
}

// ---------------------------------------------------------------------------
// Audio — post-MVP (вне v0.3.0). Безопасные no-op.
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
// FileSystem — read-only DFS (staging-арена, без heap)
// ---------------------------------------------------------------------------

FileData FileSystem_LoadFile(const char* path)
{
    return n64::fs_load_file(path, nullptr);
}

void FileSystem_FreeFile(FileData* file)
{
    (void)n64::fs_free_file(file, nullptr);
}
