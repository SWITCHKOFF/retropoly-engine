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
//     PC-backend: src/hal/pc/render_opengl.cpp; PS1-backend: Спринты 13–14);
//   - Input_*     — клавиатура + геймпады строго через SDL_Gamepad API
//     (SDL_GameController в SDL3 не существует — не использовать);
//   - Audio_*     — заглушка (короткий beep; полный PCM — post-MVP);
//   - FileSystem_* — загрузка файла целиком / освобождение буфера.
//
// Контракт заголовка: не включает SDL/glad/PSn00bSDK — бекенды подключают
// свои зависимости только в .cpp; заголовок компилируется в обоих бекендах.
// Функции — свободные (без namespace): имена сохранены дословно из §3
// (норматив). Порядок значений enum'ов — контракт ABI: вставки только
// перед *_COUNT.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/math/fixed.h"
#include "core/math/mat4.h"

// ---------------------------------------------------------------------------
// Platform: жизненный цикл платформы (недели 9–10)
// ---------------------------------------------------------------------------

/**
 * Инициализация платформы (PC: SDL3 + GL-контекст).
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
 * @return true — обычный кадр; false — получено SDL_EVENT_QUIT
 *         (главный цикл обязан завершиться; Shutdown — задача вызывающего).
 */
bool Platform_PumpEvents(void);

/**
 * Показ кадра на экран: SDL_GL_SwapWindow. Безопасный no-op, если
 * окно/GL-контекст не созданы.
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
TextureHandle Render_CreateTexture(uint32_t w, uint32_t h,
                                   const uint8_t* pixels,
                                   const uint16_t* palette);

/** Освобождение текстуры по хэндлу (Спринт 7–8). */
void Render_DestroyTexture(TextureHandle handle);

/** Привязка текстуры к текущему draw (Спринт 7–8). */
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
void Render_DrawMesh(const Vertex* vertices, uint32_t vertex_count,
                     const Triangle* indices, uint32_t index_count,
                     const Mat4* model);

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
 * @param path путь; NULL/пустая строка — отказ без краша.
 *
 * @return буфер @{data, size}; при отказе (нет файла, пустой файл,
 *         размер > UINT32_MAX) — {nullptr, 0} без краша.
 */
FileData FileSystem_LoadFile(const char* path);

/** Освобождение буфера FileData и сброс полей. NULL-указатель безопасен. */
void FileSystem_FreeFile(FileData* file);
