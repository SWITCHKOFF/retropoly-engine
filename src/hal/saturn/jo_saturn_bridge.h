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

#include <stdarg.h>
#include <stdbool.h>
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

/** Сырое состояние порта 1 (активные-низкие биты SMPC). */
typedef struct RpeSaturnPadState
{
    uint16_t data;    /**< jo_inputs[0].data: 0 — кнопка нажата. */
    uint8_t connected;/**< Периферия на порту 1 присутствует. */
    uint8_t regular;  /**< Тип периферии — RegularGamepad. */
} RpeSaturnPadState;

/**
 * Инициализация Jo Engine core (VDP1/VDP2, SMPC, память, VBlank, FS).
 *
 * Проверяет результат, который jo_core_init() не возвращает: разрешение
 * 320×224, NTSC TVSTAT и наличие 0.BIN на CD. При любом несоответствии
 * выполняет откат (маска прерываний) и возвращает false.
 *
 * @return true при успешной инициализации.
 */
bool rpe_saturn_core_init(uint16_t backdrop_color);

/** Остановка HAL-прерываний и VDP2-вывода (полного deinit у Jo Engine нет). */
void rpe_saturn_core_shutdown(void);

/** Бесконечный главный цикл Jo Engine (VBlank sync + callbacks). */
void rpe_saturn_core_run(void);

/** Миллисекунды с момента старта core (VBlank-счётчик Jo Engine). */
uint32_t rpe_saturn_ticks(void);

/** Смена однотонного backdrop VDP2 (формат RGB555 + MSB). */
void rpe_saturn_set_background_color(uint16_t color);

/** Сырое состояние пада порта 1 (данные + флаги типа). */
RpeSaturnPadState rpe_saturn_pad_read(void);

/**
 * Регистрация callback'а главного цикла (вызывается до jo_core_run()).
 *
 * @return true при успешной регистрации.
 */
bool rpe_saturn_add_frame_callback(void (*callback)(void));

// ---------------------------------------------------------------------------
// Master + Slave SH-2 (S25–26)
// ---------------------------------------------------------------------------

/**
 * @return true, если Saturn собран с dual-CPU и Slave реально стартовал.
 *
 * Jo Engine запускает Slave внутри jo_core_init(); без
 * JO_COMPILE_WITH_DUAL_CPU_SUPPORT функция всегда возвращает false.
 */
bool rpe_saturn_slave_available(void);

/**
 * Постановка callback'а в очередь Jo Engine, исполняемую Slave SH-2.
 *
 * Callback вызывается один раз на Slave; если Slave не стартовал,
 * возвращается false и callback НЕ выполняется на Master (в отличие от
 * jo_core_exec_on_slave()).
 *
 * @param callback Функция без аргументов (C-linkage).
 * @return true, если job поставлен в очередь.
 */
bool rpe_saturn_slave_post(void (*callback)(void));

/** Cache-through alias адреса для обмена данными между CPU (SH-2). */
void* rpe_saturn_cache_through(const void* pointer);

/** Сброс/invalidate кэша текущего SH-2 (на host — no-op). */
void rpe_saturn_cache_purge(void);

/** Показания Free Running Timer текущего CPU (1 tick = 32 такта). */
uint16_t rpe_saturn_frt_ticks(void);

/** Перевод ticks FRT в микросекунды для target 320×224 NTSC. */
uint32_t rpe_saturn_frt_ticks_to_us(uint16_t ticks);

/** Пауза busy-wait без обращения к внешней шине (деление SH-2). */
void rpe_saturn_cpu_relax(void);

/** @return true, если ISO9660-каталог доступен (0.BIN найден). */
bool rpe_saturn_fs_available(void);

/** @return true и размер файла; имя — bare 8.3 без «;1». */
bool rpe_saturn_fs_get_size(const char* name, uint32_t* out_size);

/** Точное чтение файла в @p dst (size байт). */
bool rpe_saturn_fs_read(const char* name, uint8_t* dst, uint32_t size);

/**
 * Форматирование и вывод строки лога в VDP2-текст (строка 27, NBG0).
 *
 * Форматирование выполняет Jo Engine (`jo_vsnprintf`), а не newlib: это
 * исключает malloc/sbrk из runtime-пути (ADR-007).
 *
 * @param level 0 INFO, 1 WARNING, иначе ERROR.
 * @param domain Короткая метка подсистемы.
 * @param format printf-подобный формат (NULL → "(null)").
 * @param args va_list, подготовленный вызывающим (NULL → пустая строка).
 */
void rpe_saturn_log_write_va(int level, const char* domain, const char* format, va_list* args);

#ifdef __cplusplus
}
#endif
