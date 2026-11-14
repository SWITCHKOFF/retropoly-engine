// SPDX-License-Identifier: MIT
// RetroPolyEngine — hello_triangle_ps1 (SDK example, S33–34).
//
// Минимальный PS-EXE через публичный SDK: очистка экрана, выход по START.
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

    while (Platform_PumpEvents() && !Input_IsPadButtonDown(0u, INPUT_PAD_START)) {
        Render_BeginFrame();
        Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G,
                     RENDER_CLEAR_DEFAULT_B);
        Render_OverlayText(8, 8, 255, 255, 255, 1, "RETRO SDK PS1");
        Render_EndFrame();
        Platform_SwapBuffers();
    }

    Render_Shutdown();
    Platform_Shutdown();
    return 0;
}
