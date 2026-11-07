// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/input_pad.cpp
//
// Реализация Input_* HAL PS1 (Спринты 13–14): цифровой PS1-совместимый геймпад
// через драйвер BIOS (psxapi.h: InitPAD/StartPAD/StopPAD; структуры psxpad.h).
//
// Зеркало input_sdl.cpp по контракту platform.h:
//   - буферы 34 байта × 2 пада (InitPAD), опрос в шинном IRQ BIOS — состояние
//     читается при запросе (Input_IsPadButtonDown/GetPadAxis) без блокировки;
//   - кнопки INVERTED (active low): бит btn == 0 означает «нажата»;
//   - валидные типы устройств: PAD_ID_DIGITAL (0x4), PAD_ID_ANALOG_STICK (0x5),
//     PAD_ID_ANALOG (0x7) — стики читаются ТОЛЬКО у аналоговых;
//   - оси стиков: raw − 128 (−128..127; 8-бит транспорт пада == raw SDL/256);
//     L2/R2 — квантованы по кнопкам (0 или 127): аналоговое давление DS2
//     требует config-режима — post-MVP;
//   - клавиатуры на PS1 нет: Input_IsKeyDown — всегда false; мышь отсутствует:
//     Input_GetMouseDelta / Input_SetMouseRelativeMode — no-op.
#include "hal/platform.h"

#include "hal/ps1/hal_ps1_internal.h"
#include "core/log/log.h"

// SDK third-party: анонимные структуры psxpad/psxapi против -Wpedantic (§3.3).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
// SDK-типы/функции (psxpad.h, psxapi.h) защищены namespace-шилдом: SDK объявляет
// enum PadButton — имя, совпадающее с typedef из platform.h; включение внутри
// namespace убирает конфликт, не трогая публичный контракт ядра. Анонимные
// структуры заголовков против -Wpedantic изолируются прагмами (§3.3).
namespace psx_sdk_shield {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <psxpad.h>
#include <psxapi.h>
#pragma GCC diagnostic pop
}
// Короткий алиас шилда (квалификация SDK-имён; using-namespace запрещён —
// SDK enum PadButton конфликтовал бы с движковым typedef на file-скоупе).
namespace sdk = psx_sdk_shield;

#include <string.h>

// ---------------------------------------------------------------------------
// Буферы драйвера BIOS (заполняются в шинном IRQ — volatile-семантика)
// ---------------------------------------------------------------------------

static volatile uint8_t s_pad_buf[HAL_PS1_PAD_COUNT][HAL_PS1_PAD_BUFFER_BYTES];
static bool s_pads_started = false;

/** Карта кнопки: INPUT_PAD_* → битов маски PAD_* (psxpad.h). */
static uint16_t pad_button_mask(uint32_t button)
{
    switch (button) {
    case INPUT_PAD_SELECT:   return (uint16_t)sdk::PAD_SELECT;
    case INPUT_PAD_UP:       return (uint16_t)sdk::PAD_UP;
    case INPUT_PAD_RIGHT:    return (uint16_t)sdk::PAD_RIGHT;
    case INPUT_PAD_DOWN:     return (uint16_t)sdk::PAD_DOWN;
    case INPUT_PAD_LEFT:     return (uint16_t)sdk::PAD_LEFT;
    case INPUT_PAD_CROSS:    return (uint16_t)sdk::PAD_CROSS;
    case INPUT_PAD_CIRCLE:   return (uint16_t)sdk::PAD_CIRCLE;
    case INPUT_PAD_SQUARE:   return (uint16_t)sdk::PAD_SQUARE;
    case INPUT_PAD_TRIANGLE: return (uint16_t)sdk::PAD_TRIANGLE;
    case INPUT_PAD_L1:       return (uint16_t)sdk::PAD_L1;
    case INPUT_PAD_R1:       return (uint16_t)sdk::PAD_R1;
    case INPUT_PAD_L2:       return (uint16_t)sdk::PAD_L2;
    case INPUT_PAD_R2:       return (uint16_t)sdk::PAD_R2;
    case INPUT_PAD_START:    return (uint16_t)sdk::PAD_START;
    default:                 return 0u; // INPUT_PAD_COUNT / вне диапазона.
    }
}

/** 8-бит стик пада (0..255) → ось двига (−128..127). */
static inline int16_t pad_axis_from_raw(uint8_t raw)
{
    return (int16_t)((int32_t)raw - 128);
}

/**
 * Снимок состояния пада @p pad_index.
 *
 * @param out_btn Выход: состояние кнопок (1 = нажата; средний active low
 *                уже инвертирован).
 * @param out_analog Выход: стики достоверны (тип 0x5/0x7).
 * @return true — пэд в порту и устройство валидно.
 */
static bool pad_snapshot(uint32_t pad_index, uint16_t* out_btn, bool* out_analog)
{
    *out_btn = 0u;
    *out_analog = false;
    if (pad_index >= HAL_PS1_PAD_COUNT) {
        return false;
    }
    // PADTYPE __attribute__((packed)): stat / len:4 / type:4 / btn — битовые
    // поля читаются напрямую из буфера BIOS (чтение через volatile-адрес).
    volatile const sdk::PADTYPE* pad =
        (volatile const sdk::PADTYPE*)(const void*)&s_pad_buf[pad_index][0];
    if (pad->stat != 0) {
        return false; // BIOS: нет ответа/таймаут (пада в порту нет).
    }
    if (pad->type != sdk::PAD_ID_DIGITAL && pad->type != sdk::PAD_ID_ANALOG_STICK &&
        pad->type != sdk::PAD_ID_ANALOG) {
        return false; // Мышь/ганкон/мультитап — вне MVP-множества.
    }
    *out_analog = (pad->type == sdk::PAD_ID_ANALOG_STICK) || (pad->type == sdk::PAD_ID_ANALOG);
    // Кнопки INVERTED (active low): инверсия на слое HAL.
    const uint16_t raw = (uint16_t)pad->btn;
    *out_btn = (uint16_t)~raw;
    return true;
}

// ---------------------------------------------------------------------------
// Hooks (platform_ps1.cpp)
// ---------------------------------------------------------------------------

void hal_ps1_input_init(void)
{
    if (s_pads_started) {
        return; // Идемпотентность.
    }
    memset((void*)s_pad_buf, 0, sizeof(s_pad_buf));
    sdk::InitPAD((uint8_t*)s_pad_buf[0], (int)HAL_PS1_PAD_BUFFER_BYTES,
            (uint8_t*)s_pad_buf[1], (int)HAL_PS1_PAD_BUFFER_BYTES);
    sdk::StartPAD(); // Драйвер BIOS: опрос пада в vblank-IRQ.
    s_pads_started = true;
    Log_Message(LOG_LEVEL_INFO, "ps1_hal", "input init: 2 pad-слота (34 байта)");
}

void hal_ps1_input_shutdown(void)
{
    if (!s_pads_started) {
        return;
    }
    sdk::StopPAD(); // Драйвер BIOS: отключение опроса.
    s_pads_started = false;
}

// ---------------------------------------------------------------------------
// Публичный контракт (platform.h)
// ---------------------------------------------------------------------------

bool Input_IsKeyDown(KeyCode key)
{
    // PS1 не имеет клавиатуры; безопасный false (контракт).
    (void)key;
    return false;
}

bool Input_IsPadButtonDown(uint32_t pad_index, PadButton button)
{
    uint16_t btn = 0u;
    bool analog = false;
    (void)analog;
    if (!pad_snapshot(pad_index, &btn, &analog)) {
        return false;
    }
    const uint16_t mask = pad_button_mask((uint32_t)button);
    return (btn & mask) != 0u;
}

int16_t Input_GetPadAxis(uint32_t pad_index, uint32_t axis)
{
    uint16_t btn = 0u;
    bool analog = false;
    if (!pad_snapshot(pad_index, &btn, &analog)) {
        return 0; // Нет пада/цифровое устройство: оси всегда 0 (контракт).
    }
    if (!analog) {
        return 0; // Стики отсутствуют у цифрового пада.
    }

    volatile const sdk::PADTYPE* pad =
        (volatile const sdk::PADTYPE*)(const void*)&s_pad_buf[pad_index][0];
    switch (axis) {
    case INPUT_AXIS_LEFTX:  return pad_axis_from_raw(pad->ls_x);
    case INPUT_AXIS_LEFTY:  return pad_axis_from_raw(pad->ls_y);
    case INPUT_AXIS_RIGHTX: return pad_axis_from_raw(pad->rs_x);
    case INPUT_AXIS_RIGHTY: return pad_axis_from_raw(pad->rs_y);
    case INPUT_AXIS_L2:
    case INPUT_AXIS_R2:
        // Триггеры квантованы по цифровым битам (давление DS2 — post-MVP;
        // диапазон 0..127 контракта сохранён).
        return (btn & pad_button_mask((uint32_t)axis)) ? 127 : 0;
    default:
        return 0;
    }
}

// ---------------------------------------------------------------------------
// Мышь (отсутствует на PS1 — no-op с безопасными значениями)
// ---------------------------------------------------------------------------

void Input_GetMouseDelta(int32_t* out_dx, int32_t* out_dy)
{
    if (out_dx != nullptr) {
        *out_dx = 0;
    }
    if (out_dy != nullptr) {
        *out_dy = 0;
    }
}

bool Input_SetMouseRelativeMode(bool enable)
{
    // Захвата нет (мыши нет); контракт ожидает bool — отчёт о неисправности
    // без краша.
    (void)enable;
    return false;
}
