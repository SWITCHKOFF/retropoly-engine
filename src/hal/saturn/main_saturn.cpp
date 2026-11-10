// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/main_saturn.cpp
//
// Sega Saturn boot skeleton (Спринт 21–22, веха v0.2.0).
//
// Jo Engine вызывает jo_main() из собственного main() после инициализации
// стартового кода (start.S). HAL Platform_Init() инициализирует VDP1/VDP2
// через Jo Engine и выставляет однотонный backdrop 320x224; jo_core_run()
// обеспечивает VBlank-синхронизацию. 3D/VDP1 — S27–28, ввод/CD — S23–24.
//
// Символ jo_main имеет C-linkage: его вызывает C-ядро Jo Engine.
#include "hal/platform.h"
#include "hal/saturn/jo_saturn_bridge.h"

extern "C" void jo_main(void)
{
    if (!Platform_Init()) {
        // На Saturn нет stdout/host-ФС; при отказе ядра не запускаем цикл.
        for (;;) {
        }
    }
    if (!Render_Init(320u, 224u)) {
        // Target profile ADR-008; Render_Init сам диагностирует неверный размер.
        for (;;) {
        }
    }

    rpe_saturn_core_run();  // не возвращается: VBlank sync + callbacks
    for (;;) {
    }
}
