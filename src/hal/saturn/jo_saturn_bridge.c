// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/jo_saturn_bridge.c
//
// Единственный translation unit HAL Saturn, включающий <jo/jo.h> (C99).
// См. jo_saturn_bridge.h: C++ HAL не видит C-заголовки Jo Engine напрямую.
#include "hal/saturn/jo_saturn_bridge.h"

#include <jo/jo.h>

void rpe_saturn_core_init(uint16_t backdrop_color)
{
    jo_core_init((jo_color)backdrop_color);
}

void rpe_saturn_core_run(void)
{
    jo_core_run();
}

uint32_t rpe_saturn_ticks(void)
{
    return (uint32_t)jo_get_ticks();
}

void rpe_saturn_set_background_color(uint16_t color)
{
    jo_set_default_background_color((jo_color)color);
}

uint32_t rpe_saturn_pad_state(void)
{
    uint32_t state = 0u;

    if (!jo_is_pad1_available()) {
        return 0u;
    }

    if (jo_is_pad1_key_pressed(JO_KEY_UP)) state |= RPE_SATURN_PAD_UP;
    if (jo_is_pad1_key_pressed(JO_KEY_DOWN)) state |= RPE_SATURN_PAD_DOWN;
    if (jo_is_pad1_key_pressed(JO_KEY_LEFT)) state |= RPE_SATURN_PAD_LEFT;
    if (jo_is_pad1_key_pressed(JO_KEY_RIGHT)) state |= RPE_SATURN_PAD_RIGHT;
    if (jo_is_pad1_key_pressed(JO_KEY_A)) state |= RPE_SATURN_PAD_A;
    if (jo_is_pad1_key_pressed(JO_KEY_B)) state |= RPE_SATURN_PAD_B;
    if (jo_is_pad1_key_pressed(JO_KEY_C)) state |= RPE_SATURN_PAD_C;
    if (jo_is_pad1_key_pressed(JO_KEY_X)) state |= RPE_SATURN_PAD_X;
    if (jo_is_pad1_key_pressed(JO_KEY_Y)) state |= RPE_SATURN_PAD_Y;
    if (jo_is_pad1_key_pressed(JO_KEY_Z)) state |= RPE_SATURN_PAD_Z;
    if (jo_is_pad1_key_pressed(JO_KEY_L)) state |= RPE_SATURN_PAD_L;
    if (jo_is_pad1_key_pressed(JO_KEY_R)) state |= RPE_SATURN_PAD_R;
    if (jo_is_pad1_key_pressed(JO_KEY_START)) state |= RPE_SATURN_PAD_START;

    return state;
}
