// SPDX-License-Identifier: MIT
// RetroPolyEngine — template project (S33–34).
//
// Минимальный кадровый цикл через публичный SDK API:
//   Platform_Init → Render_Init → BeginFrame/Clear/EndFrame → SwapBuffers.
#include <retro_engine/retro_engine.h>

#include <cstdio>

int main()
{
    if (!Platform_Init()) {
        std::fprintf(stderr, "retro_project: Platform_Init failed\n");
        return 1;
    }
    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        std::fprintf(stderr, "retro_project: Render_Init failed\n");
        Platform_Shutdown();
        return 1;
    }

    std::printf("RetroPolyEngine %s (%s)\n", RETRO_ENGINE_VERSION_STRING,
                RETRO_ENGINE_PLATFORM_NAME);

    while (Platform_PumpEvents()) {
        Render_BeginFrame();
        Render_Clear(8, 16, 32);
        Render_EndFrame();
        Platform_SwapBuffers();
    }

    Render_Shutdown();
    Platform_Shutdown();
    return 0;
}
