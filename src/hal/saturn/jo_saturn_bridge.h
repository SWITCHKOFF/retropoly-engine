// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/jo_saturn_bridge.h
//
// Узкий C-мост между C++ HAL Saturn и C-заголовками Jo Engine v2026.2.
//
// Причина: публичные заголовки Jo Engine не компилируются как C++ (anonymous
// typedef-поля, char* от строковых литералов). Контракт platform.h запрещает
// протаскивать SH-2/Jo Engine в C++/публичные заголовки; поэтому весь контакт
// с <jo/jo.h> сосредоточен в jo_saturn_bridge.c (C99), а C++ видит только
// стабильные примитивы ниже.
//
// Мост НЕ является публичным API движка и не входит в hal/platform.h.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Биты состояния 6-button Saturn pad (независимый от Jo Engine контракт). */
enum
{
    RPE_SATURN_PAD_UP = 1u << 0,
    RPE_SATURN_PAD_DOWN = 1u << 1,
    RPE_SATURN_PAD_LEFT = 1u << 2,
    RPE_SATURN_PAD_RIGHT = 1u << 3,
    RPE_SATURN_PAD_A = 1u << 4,
    RPE_SATURN_PAD_B = 1u << 5,
    RPE_SATURN_PAD_C = 1u << 6,
    RPE_SATURN_PAD_X = 1u << 7,
    RPE_SATURN_PAD_Y = 1u << 8,
    RPE_SATURN_PAD_Z = 1u << 9,
    RPE_SATURN_PAD_L = 1u << 10,
    RPE_SATURN_PAD_R = 1u << 11,
    RPE_SATURN_PAD_START = 1u << 12
};

/** Цвет Saturn RGB555 с установленным MSB (формат Jo Engine jo_color). */
#define RPE_SATURN_COLOR_RGB555 0x8000u

/** Инициализация Jo Engine core (VDP1/VDP2, SMPC, память, VBlank). */
void rpe_saturn_core_init(uint16_t backdrop_color);

/** Бесконечный главный цикл Jo Engine (VBlank sync + callbacks). */
void rpe_saturn_core_run(void);

/** Миллисекунды с момента старта core (VBlank-счётчик Jo Engine). */
uint32_t rpe_saturn_ticks(void);

/** Смена однотонного backdrop VDP2 (формат RGB555 + MSB). */
void rpe_saturn_set_background_color(uint16_t color);

/** Состояние pad 1 (биты RPE_SATURN_PAD_*; 0 — пад отсутствует). */
uint32_t rpe_saturn_pad_state(void);

#ifdef __cplusplus
}
#endif
