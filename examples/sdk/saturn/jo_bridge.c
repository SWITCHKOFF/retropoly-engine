/* SPDX-License-Identifier: MIT
 * RetroPolyEngine — hello_triangle_saturn C bridge (S33–34).
 *
 * Jo Engine headers — C-only (restrict/plan9 extensions); C++ пример
 * использует только публичный SDK, а этот мост даёт два символа.
 */
#include <jo/jo.h>

void rpe_example_add_callback(void (*callback)(void))
{
    jo_core_add_callback(callback);
}

void rpe_example_run(void)
{
    jo_core_run();
}
