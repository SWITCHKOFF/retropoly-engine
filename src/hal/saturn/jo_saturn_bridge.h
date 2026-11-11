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

/** Регистрация VBlank-callback (VDP2 scroll/VRAM commit). */
bool rpe_saturn_add_vblank_callback(void (*callback)(void));

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

// ---------------------------------------------------------------------------
// VDP1: полигоны, текстуры, CLUT, Gouraud (S27–28)
// ---------------------------------------------------------------------------

/** Команда VDP1 (8 слов, 32 байта; layout железа big-endian). */
typedef struct RpeVdp1Command
{
    uint16_t ctrl;
    uint16_t link;
    uint16_t pmod;
    uint16_t colr;
    uint16_t srca;
    uint16_t size;
    int16_t xa;
    int16_t ya;
    int16_t xb;
    int16_t yb;
    int16_t xc;
    int16_t yc;
    int16_t xd;
    int16_t yd;
    uint16_t grda;
    uint16_t reserved;
} RpeVdp1Command;

/**
 * Инициализация VDP1-пути RPE (после jo_core_init).
 *
 * Проверяет, что pinned Jo Engine использует ожидаемую карту VRAM; на host —
 * no-op. Идемпотентна.
 *
 * @return true при успехе.
 */
bool rpe_saturn_vdp1_init(void);

/** Сброс VDP1-состояния RPE (не трогает Jo Engine pipeline). */
void rpe_saturn_vdp1_shutdown(void);

/** Начало кадра RPE: сброс счётчика добавленных команд. */
void rpe_saturn_vdp1_begin_frame(void);

/** Цвет автоматической очистки VDP1 framebuffer (RGB555 + MSB). */
void rpe_saturn_vdp1_set_erase_color(uint16_t color);

/**
 * Загрузка CI8-текстуры в VDP1 VRAM.
 *
 * @param vram_offset Смещение от VDP1 VRAM base (кратно 32).
 * @param pixels CI8-пиксели (width × height).
 * @param width Ширина (кратна 8).
 * @param height Высота pattern (1..255).
 * @param stride Ширина строки в байтах (== width).
 * @return true при успехе.
 */
bool rpe_saturn_vdp1_upload_ci8(uint32_t vram_offset, const uint8_t* pixels, uint16_t width,
                                uint16_t height, uint16_t stride);

/**
 * Запись 256-цветной CLUT в VDP2 Color RAM.
 *
 * @param palette_id Палитра 1..7 (0 — VDP2-текст Jo Engine).
 * @param colors 256 × RGB555+STP.
 */
void rpe_saturn_vdp1_set_palette(uint8_t palette_id, const uint16_t* colors);

/** Запись Gouraud-таблицы (4 × RGB555 без MSB) в VDP1 VRAM. */
void rpe_saturn_vdp1_write_gouraud(uint16_t table_index, const uint16_t colors[4]);

/**
 * Добавление команды в текущий кадр VDP1.
 *
 * @param sort_key Ключ Painter's (больше — дальше); Jo Engine сортирует
 *                 команды по убыванию ключа.
 * @param command Команда (link игнорируется: pipeline перелинковывает).
 * @return true при успехе; false — command table переполнена.
 */
bool rpe_saturn_vdp1_add_command(uint16_t sort_key, const RpeVdp1Command* command);

/**
 * FNV-1a 32 по 320×224 словам RGB555 framebuffer A/B (MSB игнорируется).
 *
 * @param buffer 0 — A, 1 — B.
 * @param out_hash Выход: хэш.
 * @return true при успехе.
 */
bool rpe_saturn_vdp1_fb_hash(uint8_t buffer, uint32_t* out_hash);

/** @return Индекс отображаемого в данный момент framebuffer (0/1). */
uint8_t rpe_saturn_vdp1_displayed_buffer(void);

/** @return Число команд, добавленных RPE в текущем кадре. */
uint32_t rpe_saturn_vdp1_added_commands(void);

// ---------------------------------------------------------------------------
// VDP2: NBG1 8-bit bitmap background (S29–30)
// ---------------------------------------------------------------------------

/**
 * Инициализация NBG1 8bpp bitmap 320×224 (1024×256 VRAM) и порядка слоёв:
 * VDP2 text (NBG0) → VDP1 → NBG1 → RBG0 → NBG2 → NBG3.
 *
 * @param palette_id Палитра 1..7 (7 зарезервирована под фон).
 * @return true при успехе; false — bank A0 занят/нет VDP2 VRAM.
 */
bool rpe_saturn_vdp2_init(uint8_t palette_id);

/** Отключить NBG1 (идемпотентно; VDP1/VDP2 text остаются). */
void rpe_saturn_vdp2_shutdown(void);

/** Запись 256-цветной палитры фона в CRAM (RGB555+STP). */
void rpe_saturn_vdp2_set_palette(uint8_t palette_id, const uint16_t* colors);

/**
 * Заливка NBG1 bitmap тайлами @p width × @p height (кратны 8).
 *
 * @return true при успехе; false — bitmap не инициализирован/неверные размеры.
 */
bool rpe_saturn_vdp2_upload_background(const uint8_t* pixels, uint16_t width, uint16_t height);

/** Прокрутка NBG1 в пикселях (VBlank-safe вызов из vblank callback). */
void rpe_saturn_vdp2_set_scroll(int16_t x, int16_t y);

/** @return true, если NBG1 bitmap инициализирован. */
bool rpe_saturn_vdp2_available(void);



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
