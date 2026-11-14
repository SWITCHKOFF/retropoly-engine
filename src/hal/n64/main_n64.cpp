// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/main_n64.cpp
//
// N64 boot-demo S39–42 (ADR-014): доказательство boot-контура + HAL
// S41–42 (VI/input/DFS/память/render skeleton):
//   Platform_Init (VI 320×240 16-bit + RDPQ + joypad + DFS + ассеты)
//   → idempotence self-test (init/init/shutdown/init)
//   → Render_Init (320×240 skeleton) → main loop:
//       Platform_PumpEvents → Render_BeginFrame → debug text/overlay
//       → Render_EndFrame
//   → Log_Message через ISViewer/USB.
//
// RSP/RDP-геометрия/текстуры — S43–44; boot-демо показывает clear,
// загруженные CUBE.MSH/TEX0.TEX, memory ledger и состояние контроллера.
#include "hal/platform.h"

#include "core/log/log.h"
#include "hal/n64/n64_assets.h"
#include "hal/n64/n64_bridge.h"
#include "hal/n64/n64_fs.h"
#include "hal/n64/n64_input.h"
#include "hal/n64/n64_memory.h"
#include "retro_engine/version.h"

#include <stdio.h>

namespace {

uint32_t s_frames = 0u;

[[noreturn]] void halt_forever(void)
{
    for (;;) {
        rpe_n64_sleep_ms(100u);
    }
}

void boot_platform_or_halt(void)
{
    if (!Platform_Init()) {
        // Display может быть не инициализирован: только halt.
        halt_forever();
    }
}

void draw_boot_screen(uint32_t frame)
{
    // Render skeleton: BeginFrame (RDPQ attach + clear) → debug text →
    // EndFrame (detach_show). Текст идёт через C-мост (ISViewer-независимо).
    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);

    rpe_n64_text_print(16, 20, "RETROPOLYENGINE N64");
    rpe_n64_text_print(16, 36, "BOOT OK / S41-42");
    rpe_n64_text_print(16, 52, "VI 320X240 16BPP NTSC");

    const n64::BootAssets& assets = n64::boot_assets();
    char line[96];
    snprintf(line, sizeof(line), "DFS %s MESH %u TEX %u", n64::fs_ready() ? "OK" : "NO",
             (unsigned)(assets.mesh_ok ? assets.mesh_file_bytes : 0u),
             (unsigned)(assets.texture_ok ? assets.texture_file_bytes : 0u));
    rpe_n64_text_print(16, 68, line);

    char memory[64];
    n64::memory_overlay_line(memory, sizeof(memory));
    snprintf(line, sizeof(line), "MEM %s", memory);
    rpe_n64_text_print(16, 84, line);

    const int16_t stick_x = n64::input_pad_axis(0u, INPUT_AXIS_LEFTX);
    const int16_t stick_y = n64::input_pad_axis(0u, INPUT_AXIS_LEFTY);
    snprintf(line, sizeof(line), "PAD1 %s A%u B%u Z%u ST%u X%d Y%d",
             n64::input_connected(0u) ? "ON" : "OFF",
             n64::input_pad_button_down(0u, INPUT_PAD_CROSS) ? 1u : 0u,
             n64::input_pad_button_down(0u, INPUT_PAD_CIRCLE) ? 1u : 0u,
             n64::input_pad_button_down(0u, INPUT_PAD_SQUARE) ? 1u : 0u,
             n64::input_pad_button_down(0u, INPUT_PAD_START) ? 1u : 0u, (int)stick_x, (int)stick_y);
    rpe_n64_text_print(16, 100, line);

    snprintf(line, sizeof(line), "FRAME %lu  T %lu MS", (unsigned long)frame,
             (unsigned long)Platform_GetTimeMs());
    rpe_n64_text_print(16, 124, line);

    Render_EndFrame();
}

}  // namespace

int main(void)
{
    // Self-test идемпотентности: повторный Init и Init после Shutdown
    // обязаны вернуть true и не сломать VI/debug-канал (критерий S39–40).
    boot_platform_or_halt();
    boot_platform_or_halt();  // double Init — no-op, true.
    Platform_Shutdown();
    boot_platform_or_halt();  // re-init после Shutdown: DFS/ассеты заново.

    Log_Message(LOG_LEVEL_INFO, "n64_hal", "boot: RetroPolyEngine %s target=n64 arch=mips64",
                RETRO_ENGINE_VERSION_STRING);
    Log_Message(LOG_LEVEL_INFO, "n64_hal",
                "boot: VI 320x240 16-bit NTSC, RDPQ/joypad/DFS ready, time=%u ms",
                Platform_GetTimeMs());
    Log_Message(LOG_LEVEL_INFO, "n64_hal", "boot: init/shutdown/init self-test OK");

    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_hal", "boot: Render_Init failed, halt");
        halt_forever();
    }

    for (;;) {
        (void)Platform_PumpEvents();
        draw_boot_screen(s_frames);
        ++s_frames;

        // ~5 s при 60 Гц: heartbeat и stress asset-load loop (DFS-чтение
        // без роста staging/арен) для 10-минутного ручного gate.
        if ((s_frames % 300u) == 0u) {
            const n64::BootAssets& assets = n64::boot_assets();
            FileData mesh_probe = FileSystem_LoadFile("CUBE.MSH");
            FileData tex_probe = FileSystem_LoadFile("TEX0.TEX");
            const bool reload_ok =
                mesh_probe.data != nullptr && mesh_probe.size == assets.mesh_file_bytes &&
                tex_probe.data != nullptr && tex_probe.size == assets.texture_file_bytes;
            FileSystem_FreeFile(&mesh_probe);
            FileSystem_FreeFile(&tex_probe);
            if (!reload_ok) {
                Log_Message(LOG_LEVEL_WARNING, "n64_hal", "boot: stress reload mismatch");
            }
            Log_Message(LOG_LEVEL_INFO, "n64_hal",
                        "boot: frame=%u time=%u ms dfs=%u mesh=%u tex=%u arena=%u peak=%u "
                        "reload=%u staging=%u",
                        s_frames, Platform_GetTimeMs(), n64::fs_ready() ? 1u : 0u,
                        assets.mesh_ok ? 1u : 0u, assets.texture_ok ? 1u : 0u,
                        (unsigned)n64::asset_arena().used(), (unsigned)n64::asset_arena().peak(),
                        reload_ok ? 1u : 0u, (unsigned)n64::fs_staging_used());
        }

#if RPE_N64_BOOT_FRAMES
        if (s_frames >= (uint32_t)RPE_N64_BOOT_FRAMES) {
            Log_Message(LOG_LEVEL_INFO, "n64_hal", "boot: frame limit reached, shutdown");
            Render_Shutdown();
            Platform_Shutdown();
            halt_forever();
        }
#endif
    }
}
