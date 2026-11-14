// SPDX-License-Identifier: MIT
// RetroPolyEngine — hello_triangle_saturn (SDK example, S33–34).
//
// Jo Engine вызывает jo_main(); кадровый цикл идёт через публичный SDK API.
#include <retro_engine/retro_engine.h>

// C-мост к Jo Engine (jo_bridge.c): заголовки Jo Engine не C++-safe.
extern "C" void rpe_example_add_callback(void (*callback)(void));
extern "C" void rpe_example_run(void);

namespace {

void frame_tick(void)
{
    if (!Platform_PumpEvents()) {
        return;
    }
    Render_BeginFrame();
    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G,
                 RENDER_CLEAR_DEFAULT_B);
    Render_OverlayText(8, 8, 255, 255, 255, 1, "RETRO SDK SATURN");
    Render_EndFrame();
    Platform_SwapBuffers();
}

}  // namespace

extern "C" void jo_main(void)
{
    Platform_Init();
    Render_Init(320u, 224u);
    rpe_example_add_callback(frame_tick);
    rpe_example_run();
}
