// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/pc/render_opengl.cpp
//
// HAL PC: GL-слой. Спринт 5–6 владеет только показом кадра (swap) —
// контекст создаётся в platform_pc.cpp и уже загружен gladLoadGL до любых
// GL-вызовов. Спринты 7–8 (docs/plans/sprint_07_08.md, недели 13–16)
// расширяют этот файл backend-вызовами рендерера (renderer.cpp → HAL):
// фреймбуфер 320×240, шейдеры affine+quantize, Painter's algorithm.
#include "hal/pc/hal_pc_internal.h"

bool hal_pc_swap_buffers(void)
{
    SDL_Window* window = hal_pc_window();
    if (window == nullptr) {
        return false;
    }
    SDL_GL_SwapWindow(window);
    return true;
}
