// SPDX-License-Identifier: MIT
// RetroPolyEngine — examples/sdk/n64/main.cpp (SDK example, S45–46).
//
// Минимальный N64-потребитель установленного SDK: публичный API
// (Platform/Render/Input), без внутренних заголовков и без DFS-ассетов.
// Собирается через <sdk>/cmake/n64-toolchain.cmake; ROM — n64tool.
#include <retro_engine/retro_engine.h>

int main()
{
    if (!Platform_Init()) {
        return 1;
    }
    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        Platform_Shutdown();
        return 1;
    }

    for (;;) {
        (void)Platform_PumpEvents();
        if (Input_IsPadButtonDown(0u, INPUT_PAD_START)) {
            break;
        }
        Render_BeginFrame();
        Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G, RENDER_CLEAR_DEFAULT_B);
        Render_OverlayText(16, 16, 255, 255, 255, 1, "RETRO SDK N64");
        Render_OverlayText(16, 32, 200, 220, 255, 1, "HELLO TRIANGLE");
        Render_EndFrame();
        Platform_SwapBuffers();
    }

    Render_Shutdown();
    Platform_Shutdown();
    return 0;
}
