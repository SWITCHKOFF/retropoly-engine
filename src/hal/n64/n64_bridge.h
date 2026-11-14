// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_bridge.h
//
// ВНУТРЕННИЙ C-мост N64 HAL (не публичный SDK API). libdragon/newlib
// подключаются только в n64_bridge.c; остальные translation units HAL,
// логгер и host-тесты видят этот стабильный C-интерфейс. Это позволяет
// собирать `n64_hal_tests` host-компилятором с fake_n64_bridge.cpp
// (аналог Saturn C-моста, ADR-008/ADR-014).
//
// Контракты:
//   - Init/Shutdown идемпотентны; Shutdown безопасен без Init;
//   - ticks_ms — монотонные миллисекунды (get_ticks_ms, uint32-срез);
//   - логгер передаёт уже готовый printf-формат; мост форматирует в
//     фиксированный буфер, ограничивает частоту и пишет через
//     ISViewer/USB debug-канал;
//   - input_poll — ровно один joypad_poll за вызов (один вызов на кадр);
//   - FS — read-only DragonFS (mkdfs), ошибки возвращаются явно;
//   - render frame — BeginFrame/EndFrame владеют display surface и RDPQ.
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Битовая маска кнопок N64-контроллера (RpeN64PadState.buttons). */
enum
{
    RPE_N64_BTN_A = 1u << 0,
    RPE_N64_BTN_B = 1u << 1,
    RPE_N64_BTN_Z = 1u << 2,
    RPE_N64_BTN_START = 1u << 3,
    RPE_N64_BTN_D_UP = 1u << 4,
    RPE_N64_BTN_D_DOWN = 1u << 5,
    RPE_N64_BTN_D_LEFT = 1u << 6,
    RPE_N64_BTN_D_RIGHT = 1u << 7,
    RPE_N64_BTN_L = 1u << 8,
    RPE_N64_BTN_R = 1u << 9,
    RPE_N64_BTN_C_UP = 1u << 10,
    RPE_N64_BTN_C_DOWN = 1u << 11,
    RPE_N64_BTN_C_LEFT = 1u << 12,
    RPE_N64_BTN_C_RIGHT = 1u << 13
};

/** Снимок одного порта контроллера (после rpe_n64_input_poll). */
typedef struct RpeN64PadState
{
    uint32_t buttons; /**< RPE_N64_BTN_* bitmask (0 — ничего не нажато). */
    int8_t stick_x; /**< Левая ось X (−127..127). */
    int8_t stick_y; /**< Левая ось Y (−127..127). */
    int8_t cstick_x; /**< C-ось X (−127..127; эмулируется C-кнопками). */
    int8_t cstick_y; /**< C-ось Y (−127..127; эмулируется C-кнопками). */
    uint8_t analog_l; /**< Аналоговый L (0..255). */
    uint8_t analog_r; /**< Аналоговый R (0..255). */
    bool connected; /**< false — порт пуст/устройство не опрошено. */
} RpeN64PadState;

/** Инициализация libdragon: debug-канал, timer, VI 320×240 16-bit, RDPQ, joypad. */
bool rpe_n64_core_init(void);

/** Деинициализация joypad/RDPQ/VI. Идемпотентна; безопасна без Init. */
void rpe_n64_core_shutdown(void);

/** true — core init выполнен и не был закрыт. */
bool rpe_n64_core_ready(void);

/** Монотонные миллисекунды с момента загрузки (uint32-срез). */
uint32_t rpe_n64_ticks_ms(void);

/** Ожидание @p ms миллисекунд (wait_ms, с разбиением длинных пауз). */
void rpe_n64_sleep_ms(uint32_t ms);

/** Размер RDRAM, определённый libdragon: 4 MiB или 8 MiB (Expansion Pak). */
uint32_t rpe_n64_rdram_size(void);

/**
 * Вывод строки лога через ISViewer/USB debug-канал с rate limit.
 *
 * @param level    LogLevel (0 INFO, 1 WARNING, 2 ERROR).
 * @param domain   Короткая метка подсистемы (не NULL).
 * @param format   printf-формат (не NULL).
 * @param args     Аргументы формата (va_list, переданный указателем).
 */
void rpe_n64_log_write_va(int level, const char* domain, const char* format, va_list* args);

/** Ровно один joypad_poll и фиксация снимка всех портов. */
void rpe_n64_input_poll(void);

/**
 * Снимок порта @p port (0..3).
 *
 * @return true — порт содержит поддерживаемый контроллер; false — пуст/
 *         неподдерживаемое устройство (out заполняется нулями).
 */
bool rpe_n64_input_read(uint32_t port, RpeN64PadState* out);

/** Инициализация read-only DragonFS (DFS_DEFAULT_LOCATION). Идемпотентно. */
bool rpe_n64_fs_init(void);

/** Деинициализация DFS (идемпотентно, безопасно без Init). */
void rpe_n64_fs_shutdown(void);

/** true — DragonFS инициализирована и найдена в ROM. */
bool rpe_n64_fs_ready(void);

/**
 * Размер файла в DFS без открытия.
 *
 * @return размер ≥ 0; -1 — файла нет/FS недоступна.
 */
int32_t rpe_n64_fs_size(const char* name);

/**
 * Точное чтение файла @p name в @p dst размером @p size.
 *
 * @return число прочитанных байт (== size) или -1 при ошибке/обрыве.
 */
int32_t rpe_n64_fs_read(const char* name, uint8_t* dst, uint32_t size);

/** true — display/RDPQ готовы и backend может начать кадр. */
bool rpe_n64_render_available(void);

/** Начало кадра: display_get + rdpq_attach + clear. false — кадр уже активен. */
bool rpe_n64_frame_begin(uint8_t r, uint8_t g, uint8_t b);

/** Очистка активного кадра (no-op вне кадра). */
void rpe_n64_frame_clear(uint8_t r, uint8_t g, uint8_t b);

/** Конец кадра: rdpq_detach_show. no-op вне кадра. */
void rpe_n64_frame_end(void);

/** true — между rpe_n64_frame_begin и rpe_n64_frame_end. */
bool rpe_n64_frame_active(void);

/**
 * Вывод debug-текста в активный кадр (встроенный шрифт 8×8, id=1).
 *
 * @return true — текст поставлен; false — кадра нет/текст NULL.
 */
bool rpe_n64_text_print(int x, int y, const char* text);

#ifdef __cplusplus
}
#endif
