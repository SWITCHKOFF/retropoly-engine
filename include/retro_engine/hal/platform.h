// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/platform.h
//
// Единый публичный API HAL (Hardware Abstraction Layer). Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md, §3 (ЭТАП 4:
//     System Design, «platform.h — единый заголовочный файл ядра»);
//   - docs/plans/sprint_05_06.md, Недели 9–12.
//
// Разделы API:
//   - Platform_*  — окно, GL-контекст, события, тайминги (SDL3 на PC);
//   - Render_*    — интерфейс рендерера (реализация — Спринты 7–8,
//     PC-backend: src/hal/pc/render_opengl.cpp; PS1-backend: Спринты 13–14;
//     Saturn skeleton: backdrop-only, VDP1 — S27–28);
//   - Input_*     — клавиатура + геймпады строго через SDL_Gamepad API
//     (SDL_GameController в SDL3 не существует — не использовать);
//     Saturn — 6-button pad через SMPC (раскладка в docs/SATURN.md);
//   - Audio_*     — заглушка (короткий beep; полный PCM — post-MVP);
//   - FileSystem_* — загрузка файла целиком / освобождение буфера
//     (Saturn: read-only ISO9660, реализация S23–24).
//
// Контракт заголовка: не включает SDL/glad/PSn00bSDK/Jo Engine — бекенды
// подключают свои зависимости только в .cpp; заголовок компилируется во
// всех бекендах без SH-2/MIPS/хост-специфичных типов. Выбор backend —
// compile-time CMake (PLATFORM_PC/PLATFORM_PS1/PLATFORM_SATURN, ADR-001/
// ADR-008); платформенных #ifdef в этом заголовке нет by design.
// Функции — свободные (без namespace): имена сохранены дословно из §3
// (норматив). Порядок значений enum'ов — контракт ABI: вставки только
// перед *_COUNT.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <retro_engine/core/math/fixed.h>
#include <retro_engine/core/math/mat4.h>

// ---------------------------------------------------------------------------
// Platform: жизненный цикл платформы (недели 9–10)
// ---------------------------------------------------------------------------

/**
 * Инициализация платформы (PC: SDL3 + GL-контекст; PS1: GPU/GTE/CD/pad;
 * Saturn: Jo Engine core — VDP1/VDP2, SMPC, VBlank, 320×224 NTSC).
 *
 * Saturn (ADR-008): ровно один вызов на процесс; повторный — идемпотентен.
 * Backdrop выставляется через VDP2; 3D/VDP1 в v0.2.0-S23–24 отсутствует
 * (VDP1-draw вернёт false до S27–28).
 *
 * Последовательность (§3.3 + sprint_05_06, неделя 9):
 *   1. SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) — БЕЗ SDL_INIT_TIMER
 *      (в SDL3 удалён);
 *   2. GL-атрибуты (Core 3.3, DoubleBuffer=1) строго ДО создания окна;
 *   3. окно «RetroEngine» 1280×960 (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
 *   4. SDL_GL_CreateContext → SDL_GL_MakeCurrent → gladLoadGL — загрузка
 *      адресов GL до любых GL-вызовов;
 *   5. подсистема ввода открывает доступные геймпады (SDL_Gamepad API).
 *
 * При отказе любой стадии выполняется полный откат (контекст/окно/SDL_Quit),
 * возвращается false; состояние после отказа — «не инициализировано».
 * Render_Init здесь НЕ вызывается (Lifecycle G9: последовательность
 * Platform_Init → Render_Init определяет вызывающий код, Спринты 7–8).
 *
 * @return true при успехе; false при любом сбое платформы.
 */
bool Platform_Init(void);

/**
 * Освобождение платформы: закрытие геймпадов, SDL_GL_DestroyContext →
 * SDL_DestroyWindow → SDL_Quit (порядок недели 9). Идемпотентна и безопасна
 * без Init. После Shutdown запросы Platform_* и Input_* возвращают значения
 * по умолчанию (без краша); повторная Platform_Init допустима.
 */
void Platform_Shutdown(void);

/**
 * Прокачка очереди событий окна/ввода (SDL_PollEvent за кадр).
 * Остальные события (SDL_EVENT_WINDOW_* и т.п.) сознательно игнорируются —
 * их обработка появляется в Спринтах 7–8+.
 *
 * PS1/Saturn: очереди событий нет — константный true; выход определяется
 * платформенным вводом (PS1 START; Saturn — START читается через Input_*).
 * Saturn дополнительно опрашивает 6-button pad ровно один раз за кадр.
 *
 * @return true — обычный кадр; false — получено SDL_EVENT_QUIT
 *         (главный цикл обязан завершиться; Shutdown — задача вызывающего).
 */
bool Platform_PumpEvents(void);

/**
 * Показ кадра на экран: SDL_GL_SwapWindow. Безопасный no-op, если
 * окно/GL-контекст не созданы. PS1: DrawSync → VSync → flip. Saturn
 * skeleton: no-op — кадром владеет jo_core_run() (VDP1/VDP2 sync).
 */
void Platform_SwapBuffers(void);

/** Ожидание @p ms миллисекунд (SDL_Delay). ms=0 — немедленный возврат. */
void Platform_SleepMs(uint32_t ms);

/**
 * Текущее время с момента SDL_Init, мс (SDL_GetTicks).
 * uint32_t-срез uint64-счётчика SDL3: обёртка через ~49,7 дней — приемлемо
 * для сессионного игрового цикла (delta time считается беззнаковой
 * разностью, корректна и через обёртку).
 *
 * PS1/Saturn: виртуальное время платформенного VBlank-счётчика
 * (без системных часов; wrap-safe беззнаковая разность).
 *
 * @return миллисекунды (monotonic, без коррекций системных часов).
 */
uint32_t Platform_GetTimeMs(void);

// ---------------------------------------------------------------------------
// Render: интерфейс рендерера (объявлен здесь — реализация Спринты 7–8)
// ---------------------------------------------------------------------------

/** Хэндл текстуры рендерера (независимый дескриптор; 0 — невалиден). */
typedef uint32_t TextureHandle;

/** Невалидный TextureHandle (создание текстур — Спринты 7–8). */
constexpr TextureHandle RENDER_TEXTURE_INVALID = 0u;

/**
 * Инициализация рендерера (Спринты 7–8: фреймбуфер 320×240, шейдеры affine +
 * quantize, Painter's algorithm). Вызывается строго РОВНО ОДИН раз после
 * Platform_Init и до первого Render_BeginFrame (Lifecycle G9 §6).
 *
 * @return true при успехе.
 */
bool Render_Init(uint32_t width, uint32_t height);

/** Освобождение рендерера. Идемпотентна; после — вызовы Render_* безопасны. */
void Render_Shutdown(void);

/** Начало кадра (Спринт 7–8: очистка / сброс command buffer). */
void Render_BeginFrame(void);

/** Конец кадра (Спринт 7–8: sort Painter's algorithm + blit на окно). */
void Render_EndFrame(void);

/** Очистка фреймбуфера цветом @p r,@p g,@p b (Спринт 7–8). */
void Render_Clear(uint8_t r, uint8_t g, uint8_t b);

/**
 * Вершина меша движка (нормативный layout §3; сверка с .mesh v1 —
 * Спринт 7–8, неделя 15). Координаты — fixed_t 16.16 (детерминизм).
 */
typedef struct Vertex
{
    fixed_t x, y, z;    /**< Позиция в engine-space (signed 16.16). */
    int8_t nx, ny, nz;  /**< Сжатая нормаль (int8 на компоненту оси). */
    uint8_t u, v;       /**< UV-координаты (0..255 → 0.0..1.0). */
    uint8_t r, g, b, a; /**< Цвет вершины (Gouraud; a — для пост-MVP). */
} Vertex;

/** Треугольник меша: три индекса в dense-массив Vertex. */
typedef struct Triangle
{
    uint16_t v0, v1, v2;
} Triangle;

/**
 * Создание текстуры (Спринт 7–8, неделя 15): пиксели CI8 (индексы 0..255) +
 * палитра RGB555+STP (256×uint16_t; биты R[4:0], G[9:5], B[14:10], STP[15]).
 *
 * @return хэндл или RENDER_TEXTURE_INVALID при сбое.
 */
TextureHandle Render_CreateTexture(uint32_t w, uint32_t h, const uint8_t* pixels,
                                   const uint16_t* palette);

/** Освобождение текстуры по хэндлу (Спринт 7–8). */
void Render_DestroyTexture(TextureHandle handle);

/** Привязка текстуры к текущему draw; RENDER_TEXTURE_INVALID — явный
 *  unbind: нетекстурированный режим (flat-Gouraud без семпла). */
void Render_BindTexture(TextureHandle handle);

/**
 * Установка матриц камеры (view/proj, column-major Mat4, 16.16).
 * Вызывается до DrawMesh текущего кадра; валидация указателей — реализация
 * (Спринт 7–8).
 */
void Render_SetCamera(const Mat4* view, const Mat4* proj);

/**
 * Отрисовка меша (Спринт 7–8, неделя 16): трансформация @p vertices
 * матрицей @p model, painter's-сортировка по camera-space depth
 * (depth_key_from_camera_z; вне диапазона — DEPTH_REJECT без alias).
 */
void Render_DrawMesh(const Vertex* vertices, uint32_t vertex_count, const Triangle* indices,
                     uint32_t index_count, const Mat4* model);

// ---------------------------------------------------------------------------
// Reference-разрешение и диагностика рендера (публичный SDK-контракт, ADR-011)
// ---------------------------------------------------------------------------

/** Ширина reference-фреймбуфера (§1.2.1 manifest: renderer). */
#define RENDER_FB_WIDTH 320u
/** Высота reference-фреймбуфера (§1.2.1 manifest: renderer). */
#define RENDER_FB_HEIGHT 240u

/** Цвет очистки по умолчанию (smoke «окно (0,0,32)»). */
#define RENDER_CLEAR_DEFAULT_R 0u
#define RENDER_CLEAR_DEFAULT_G 0u
#define RENDER_CLEAR_DEFAULT_B 32u

/**
 * Детерминированные диагностические коды рендера. Порядок — контракт;
 * расширение — только перед RENDER_DIAG_COUNT. Каждый «reject»-этап
 * CONVEYOR-а обязан оставлять последний код в state диагностики.
 */
typedef enum
{
    RENDER_DIAG_OK = 0,              /**< Успех. */
    RENDER_DIAG_DOUBLE_INIT,         /**< Render_Init при активном рендере. */
    RENDER_DIAG_NOT_INITIALIZED,     /**< Вызов до Init (EndFrame и пр.). */
    RENDER_DIAG_END_FRAME_BEFORE_INIT,/**< EndFrame до первого Init. */
    RENDER_DIAG_NULL_ARGUMENT,       /**< NULL-аргумент API рендера. */
    RENDER_DIAG_INVALID_SIZE,        /**< (w,h) вне [1..4096]. */
    RENDER_DIAG_NO_WINDOW,           /**< Окно/GL-контекст отсутствует. */
    RENDER_DIAG_BACKEND_FAIL,        /**< Отказ GL/backend (реализация). */
    RENDER_DIAG_INVALID_PROJECTION,  /**< Не выводятся near/far (proj). */
    RENDER_DIAG_TEXTURE_FAIL,        /**< Отказ создания текстуры (пул/арена). */
    RENDER_DIAG_INVALID_VERTEX_INDEX,/**< Индекс вне [0..vertex_count). */
    RENDER_DIAG_CLIP_REJECT,         /**< Отброс near/far-plane (счётчик). */
    RENDER_DIAG_CULL_REJECT,         /**< Отброс backface (счётчик). */
    RENDER_DIAG_DEPTH_REJECT,        /**< depth_key вне диапазона (счётчик). */
    RENDER_DIAG_COMMAND_OVERFLOW,    /**< 64 KiB буфер исчерпан (явная). */
    RENDER_DIAG_READBACK_FAIL,       /**< glReadPixels неудача. */
    RENDER_DIAG_END_FRAME_WITHOUT_BEGIN, /**< EndFrame без BeginFrame. */
    RENDER_DIAG_DRAW_OUTSIDE_FRAME,  /**< DrawMesh вне BeginFrame..EndFrame. */
    RENDER_DIAG_COUNT
} RenderDiag;

/** Диагностический счётчик рендера (детерминированный, тест-совместимый). */
typedef struct RenderDiagnostics
{
    uint32_t submitted;        /**< Треугольников добавлено в буфер кадра. */
    uint32_t clip_rejects;     /**< Отброшено near/far (конвейер CLIP). */
    uint32_t backface_rejects; /**< Отброшено backface (конвейер CULL). */
    uint32_t depth_rejects;    /**< Отброшено depth вне диапазона (SORT gate). */
    uint32_t last_code;        /**< Последний код RenderDiag (0 — OK). */
} RenderDiagnostics;

/**
 * Текущие счётчики диагностики рендера (для игр/тестов/логов; без блокировок —
 * однопоточный драйвер). Счётчики НЕ сбрасываются автоматически (reset —
 * зона вызывающего; Render_Init обнуляет).
 */
RenderDiagnostics render_diagnostics(void);

/**
 * Занятое место в командном буфере кадра (байт).
 * Используется MemoryProfiler для учёта RAM рендера.
 */
uint32_t render_command_buffer_used_bytes(void);

/**
 * Занятое место в пуле текстур (байт). Публичный диагностический счётчик
 * для overlay/профилирования; 0 до Render_Init.
 */
uint32_t render_texture_pool_used_bytes(void);

/**
 * Debug-overlay текст (Спринт 17–18, неделя 36): растровый шрифт 3×5
 * (uppercase + цифры + пунктуация) в ПИКСЕЛЬНЫХ координатах reference-
 * фреймбуфера (x 0..319, y 0..239, y вниз). Рисуется ПОВЕРХ сцены кадра
 * (вне Painter's-сортировки и clip/cull) — flush в Render_EndFrame до
 * выкладки. Вызов вне BeginFrame..EndFrame — диагностический no-op
 * (RENDER_DIAG_DRAW_OUTSIDE_FRAME); текст сверх ёмкости буфера
 * усекается детерминированно.
 *
 * @param x Пиксельный X (левый край первого глифа).
 * @param y Пиксельный Y (верх первой строки).
 * @param r,g,b Цвет текста (0..255).
 * @param scale Целочисленный масштаб глифа (1 — 3×5 px; 2 — 6×10 px).
 * @param text NUL-терминированная ASCII-строка ('\n' — новая строка).
 */
void Render_OverlayText(int32_t x, int32_t y, uint8_t r, uint8_t g, uint8_t b, uint32_t scale,
                        const char* text);

// ---------------------------------------------------------------------------
// Input: клавиатура + геймпады (недели 9–11)
// ---------------------------------------------------------------------------

/**
 * Код клавиши клавиатуры. Порядок — контракт (таблица сканкодов в
 * input_sdl.cpp); расширение списка — только перед INPUT_KEY_COUNT.
 */
typedef enum
{
    INPUT_KEY_W = 0,
    INPUT_KEY_A,
    INPUT_KEY_S,
    INPUT_KEY_D,
    INPUT_KEY_UP,
    INPUT_KEY_DOWN,
    INPUT_KEY_LEFT,
    INPUT_KEY_RIGHT,
    INPUT_KEY_SPACE,
    INPUT_KEY_ENTER,
    INPUT_KEY_ESCAPE,
    INPUT_KEY_TAB,
    INPUT_KEY_LSHIFT,
    INPUT_KEY_RSHIFT,
    INPUT_KEY_LCTRL,
    INPUT_KEY_RCTRL,
    INPUT_KEY_J,
    INPUT_KEY_K,
    INPUT_KEY_L,
    INPUT_KEY_U,
    INPUT_KEY_I,
    INPUT_KEY_COUNT
} KeyCode;

/**
 * Кнопка цифрового PS1-совместимого геймпада (док. §3: D-pad, лицевые
 * кнопки PS-раскладки, шоулдеры, Start/Select). Порядок — контракт.
 * L1/R1 — цифровые кнопки; L2/R2 в SDL — аналоговые оси, цифровое чтение
 * по порогу (см. Input_IsPadButtonDown).
 */
typedef enum
{
    INPUT_PAD_UP = 0,
    INPUT_PAD_DOWN,
    INPUT_PAD_LEFT,
    INPUT_PAD_RIGHT,
    INPUT_PAD_CROSS,    /**< Нижняя лицевая кнопка (PS: Крестик). */
    INPUT_PAD_CIRCLE,   /**< Правая лицевая кнопка (PS: Круг). */
    INPUT_PAD_SQUARE,   /**< Левая лицевая кнопка (PS: Квадрат). */
    INPUT_PAD_TRIANGLE, /**< Верхняя лицевая кнопка (PS: Треугольник). */
    INPUT_PAD_L1,
    INPUT_PAD_R1,
    INPUT_PAD_L2,
    INPUT_PAD_R2,
    INPUT_PAD_START,
    INPUT_PAD_SELECT,   /**< PS: Select/Share (SDL: BACK). */
    INPUT_PAD_COUNT
} PadButton;

// Saturn 6-button pad → INPUT_PAD_* (engine policy, docs/SATURN.md):
//   D-pad → UP/DOWN/LEFT/RIGHT; A→CROSS; B→CIRCLE; C→SQUARE; X→TRIANGLE;
//   Y→L1; Z→R1; L→L2; R→R2; START→START. Select на 6-button pad отсутствует.
// Enum НЕ расширяется Saturn-кнопками в S21–22: расширение — только перед
// INPUT_PAD_COUNT и только с ADR/обновлением всех бэкендов.

/**
 * Ось геймпада. Порядок зеркалит SDL_GAMEPAD_AXIS_* (контракт).
 * Стики: диапазон −128..127; триггеры L2/R2: 0..127.
 */
typedef enum
{
    INPUT_AXIS_LEFTX = 0,
    INPUT_AXIS_LEFTY,
    INPUT_AXIS_RIGHTX,
    INPUT_AXIS_RIGHTY,
    INPUT_AXIS_L2,
    INPUT_AXIS_R2,
    INPUT_AXIS_COUNT
} InputAxis;

/**
 * Состояние клавиши @p key в текущий кадр (после Platform_PumpEvents).
 *
 * @return true — клавиша зажата; false — не зажата / платформа не
 *         инициализирована / @p key вне диапазона (безопасный false).
 */
bool Input_IsKeyDown(KeyCode key);

/**
 * Состояние кнопки @p button на геймпаде @p pad_index (0..N-1;
 * PS1-паритет: максимум 2 пада).
 *
 * Pad 0 при отсутствии подключённого физического геймпада читает
 * клавиатурную эмуляцию цифрового PS1-пада (таблица маппинга в
 * input_sdl.cpp: W/A/S/D + стрелки → D-pad; SPACE → Крестик; J → Круг;
 * K → Квадрат; L → Треугольник; U → L1; I → R1; Enter → Start;
 * Tab → Select) — единая точка правды для PC-запуска без геймпада
 * (базис унификации InputSystem, неделя 31).
 *
 * L2/R2 — аналоговые оси SDL: цифровое «нажатие» = значение оси ≥ 50%
 * хода (цель: запуск с DualShock-клона через USB/адаптер).
 *
 * @return true — зажата; false — нет пада/кнопки/вне диапазона (без краша).
 */
bool Input_IsPadButtonDown(uint32_t pad_index, PadButton button);

/**
 * Значение оси @p axis геймпада @p pad_index (−128..127; триггеры 0..127).
 * Raw SDL [-32768..32767] сжимается детерминированным целочисленным
 * делением raw/256 (без float; монотонный насыщенный масштаб).
 *
 * @return значение оси; 0 при отсутствии пада/оси/вне диапазона (без краша).
 */
int16_t Input_GetPadAxis(uint32_t pad_index, uint32_t axis);

// -------------------------------------------------------------------------
// Мышь (PC; расширение Спринта 11–12, неделя 23 — «мышь-взгляд» из
// docs/plans/sprint_11_12.md). PS1-контроллер мыши отсутствует по
// нормативной модели ввода §3 — функции безопасны и PC-only.
// -------------------------------------------------------------------------

/**
 * Включение/выключение относительного режима мыши (SDL3:
 * SDL_SetWindowRelativeMouseMode — курсор скрыт и не покидает окно,
 * события MOUSE_MOTION дают дельты без привязки к краям экрана).
 * Идемпотентна; безопасна до Init/после Shutdown (no-op).
 *
 * Накопление дельт идёт ТОЛЬКО в относительном режиме; платформа
 * аккумулирует их между вызовами Input_GetMouseDelta (consume).
 *
 * @param enable true — включить relative-режим; false — выключить.
 * @return true — режим установлен (или уже соответствовал запросу);
 *         false — платформа не инициализирована/окно отсутствует.
 */
bool Input_SetMouseRelativeMode(bool enable);

/**
 * Относительное смещение мыши с последнего вызова (consume-семантика:
 * накопленные обоих счётчиков обнуляются при чтении). Значения — целые
 * пиксели motion-дельт SDL; до Init/после Shutdown — no-op {0, 0}.
 * Экранный Y направлен ВНИЗ: dy > 0 — мышь двигали вниз.
 *
 * @param out_dx Выход: накопленный dx (NULL — запись dx пропускается).
 * @param out_dy Выход: накопленный dy (NULL — запись dy пропускается).
 */
void Input_GetMouseDelta(int32_t* out_dx, int32_t* out_dy);

// ---------------------------------------------------------------------------
// Audio: заглушка (недели 9–12; полный PCM — post-MVP)
// ---------------------------------------------------------------------------

/**
 * Инициализация аудио-сервиса (PC: SDL3 device-stream, mono 22.05 кГц, U8).
 * Идемпотентна. false на системах без аудио-устройства — НЕ крах:
 * Audio_PlayBeep становится no-op.
 *
 * @return true — поток открыт (или уже был); false — аудио недоступно.
 */
bool Audio_Init(void);

/** Освобождение аудио-потока. Идемпотентна и безопасна без Init. */
void Audio_Shutdown(void);

/**
 * Диагностический beep частотой @p frequency Гц длительностью @p duration_ms
 * (квадратная волна умеренной амплитуды; генерируется в audio-треде,
 * управление — через атомики, главный поток не блокируется).
 * Частота вне [1..20000] Гц — no-op; до Audio_Init или при отказе Init —
 * no-op (без краша). Полный PCM-микс — post-MVP.
 *
 * @param frequency   частота волны в Гц.
 * @param duration_ms длительность в мс; 0 — беззвучный no-op.
 */
void Audio_PlayBeep(uint32_t frequency, uint32_t duration_ms);

// ---------------------------------------------------------------------------
// FileSystem: загрузка файла целиком (недели 9–12)
// ---------------------------------------------------------------------------

/** Результат загрузки файла (буфер в SDL-аллокаторе). */
typedef struct FileData
{
    uint8_t* data;
    uint32_t size;
} FileData;

/**
 * Загрузка файла @p path целиком.
 *
 * Контракт владения: память буфера выделяется SDL-аллокатором
 * (SDL_LoadFile) и освобождается ТОЛЬКО через FileSystem_FreeFile;
 * ручное free()/delete этого буфера запрещено.
 *
 * Saturn (S23–24): read-only ISO9660; HAL принимает bare 8.3-имена без
 * `;1` (`CUBE.MSH`), нормализует строчные в upper-case и отвергает
 * host-пути/`..`/`;`/wildcard'ы. Буфер выделяется из WRAM-H asset heap и
 * освобождается только через FileSystem_FreeFile; ошибка чтения/валидации
 * возвращает {nullptr, 0} без утечки persistent-памяти.
 *
 * @param path путь; NULL/пустая строка — отказ без краша.
 *
 * @return буфер @{data, size}; при отказе (нет файла, пустой файл,
 *         размер > UINT32_MAX) — {nullptr, 0} без краша.
 */
FileData FileSystem_LoadFile(const char* path);

/** Освобождение буфера FileData и сброс полей. NULL-указатель безопасен. */
void FileSystem_FreeFile(FileData* file);
