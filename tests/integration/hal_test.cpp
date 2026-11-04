// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/integration/hal_test.cpp
//
// HAL-интеграционные тесты — docs/plans/sprint_05_06.md,
// недели 10 (Init/Shutdown, GetTimeMs-delta, ввод) и 12 (аудио-заглушка, ФС).
//
// Среда исполнения: Platform-тесты требуют дисплей/GL-контекст (окно 1280×960
// реально создаётся и закрывается — это и есть [H]-проверка недели 9).
// На headless-CI Linux (нет DISPLAY/WAYLAND_DISPLAY) такие кейсы пропускаются
// через GTEST_SKIP; локальные прогонки и windows/macos CI (build-pc.yml)
// выполняют их полностью. Тесты Audio/FileSystem от дисплея не зависят.
// Рендер-Lifecycle (G9) — Спринты 7–8. Мышь (Sprint 11–12, неделя 23) —
// HalMouse: относительный режим + consume-дельты, включая вставку
// синтетических событий SDL через общий PumpEvents-путь.
#include "hal/platform.h"

#ifdef PLATFORM_PC
#include <SDL3/SDL.h> // вставка синтетических SDL-событий (тест PC-only)
#endif

#include <gtest/gtest.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>

// ---------------------------------------------------------------------------
// LSan-суппрессии системного GL-драйвера. Спринт 5–6 добавил создание
// GL-контекстов: Mesa (libgallium) держит внутренние контекстные реестры,
// не освобождаемые при glXDestroyContext, — это долгоживущая память ВНУТРИ
// системного драйвера, НЕ кода движка. Официальный механизм LSan — хук
// __lsan_default_suppressions (широко используемый sanitizer runtime).
// Зеркальный список для внешних прогонок: ci/lsan_suppressions.supp.
// ---------------------------------------------------------------------------
extern "C" const char* __lsan_default_suppressions(void)
{
    return "leak:libgallium\n"
           "leak:libGLX\n"
           "leak:libGL\n";
}

namespace {

/// Headless-обнаружение только для Linux канонизируется репозиторием (CI).
bool display_available()
{
#if defined(__linux__)
    const char* x11 = getenv("DISPLAY");
    const char* wayland = getenv("WAYLAND_DISPLAY");
    return (x11 != nullptr && x11[0] != '\0') || (wayland != nullptr && wayland[0] != '\0');
#else
    return true;
#endif
}

/// Полное условие готовности: дисплей есть И Platform_Init окончился успехом.
bool platform_ready()
{
    return display_available() && Platform_Init();
}

/// Гомогенный временный файл для FileSystem-кейсов (каталог — cwd CTest).
const char* TEST_FILE_NAME = "retro_hal_fs_test.tmp";

} // namespace

TEST(Hal, InitShutsDownWithoutCrash)
{
    if (!platform_ready()) {
        GTEST_SKIP() << "Дисплей недоступен — HAL Platform-тест пропущен (headless)";
    }
    Platform_Shutdown();
    Platform_Shutdown(); // Shutdown идемпотентен (без краша)

    // Повторная инициализация после честного цикла Init/Shutdown допустима.
    ASSERT_TRUE(platform_ready());
    Platform_Shutdown();
}

TEST(Hal, GetTimeMsDeltaMatchesSleepWindow)
{
    if (!platform_ready()) {
        GTEST_SKIP() << "Дисплей недоступен — HAL Platform-тест пропущен (headless)";
    }

    const uint32_t before = Platform_GetTimeMs();
    Platform_SleepMs(100);
    const uint32_t after = Platform_GetTimeMs();

    const uint32_t delta = after - before; // uint32-разность: wrap-safe
    EXPECT_GE(delta, 95u);
    EXPECT_LE(delta, 150u);

    Platform_Shutdown();
}

TEST(Hal, InputQueriesReturnValuesWithoutCrash)
{
    if (!platform_ready()) {
        GTEST_SKIP() << "Дисплей недоступен — HAL Platform-тест пропущен (headless)";
    }

    // Все объявленные клавиши опрашиваются; в чистой консоли без ввода они
    // почти наверняка не зажаты, но главное — функция возвращает bool.
    bool pressed_seen = false;
    for (int key = 0; key < (int)INPUT_KEY_COUNT; ++key) {
        pressed_seen = Input_IsKeyDown((KeyCode)key) || pressed_seen;
    }
    (void)pressed_seen;

    // Вне диапазона: безопасные false/0 (контракт недель 10–11).
    EXPECT_FALSE(Input_IsKeyDown((KeyCode)(INPUT_KEY_COUNT + 7)));
    EXPECT_FALSE(Input_IsKeyDown((KeyCode)(-1)));
    EXPECT_FALSE(Input_IsPadButtonDown(0, INPUT_PAD_COUNT));
    EXPECT_FALSE(Input_IsPadButtonDown(999u, INPUT_PAD_CROSS));
    EXPECT_EQ(Input_GetPadAxis(999u, INPUT_AXIS_LEFTX), 0);
    EXPECT_EQ(Input_GetPadAxis(0, (uint32_t)INPUT_AXIS_COUNT), 0);

    Platform_Shutdown();
}

namespace {

#ifdef PLATFORM_PC
/// Вставка синтетического motion-события мыши в SDL-очередь: до движка дельты
/// доходят ровно по тому же пути, что и реальный ввод (Platform_PumpEvents →
/// hal_input_pump_event), поэтому тест проверяет и точку накопления.
/// SDL3: motion.xrel/yrel — float; в тесте — целые значения, так что хук
/// копит их в int32 без потерь.
void push_mouse_motion(int32_t xrel, int32_t yrel)
{
    SDL_Event motion;
    SDL_zero(motion); // «неиспользуемые» поля события без мусора
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.xrel = (float)xrel;
    motion.motion.yrel = (float)yrel;
    ASSERT_EQ(SDL_PushEvent(&motion), true);
}
#endif

} // namespace

// no-op контракт ДО Init (валиден и headless — окно не требуется).
TEST(HalMouse, PreInitQueriesAreSafeNoOps)
{
    // Предыдущие кейсы завершились честным Platform_Shutdown.
    int32_t dx = -99;
    int32_t dy = -99;
    Input_GetMouseDelta(&dx, &dy);
    EXPECT_EQ(dx, 0);
    EXPECT_EQ(dy, 0);

    Input_GetMouseDelta(nullptr, nullptr); // NULL-каналы — без краша

    // Относительный режим требует окна: платформа не инициализирована → false.
    EXPECT_FALSE(Input_SetMouseRelativeMode(true));
    EXPECT_FALSE(Input_SetMouseRelativeMode(false));
}

TEST(HalMouse, RelativeModeLifecycleAndDeltasThroughPumpEvents)
{
    if (!platform_ready()) {
        GTEST_SKIP() << "Дисплей недоступен — HAL Platform-тест пропущен (headless)";
    }

    int32_t dx = 0;
    int32_t dy = 0;

#ifdef PLATFORM_PC
    // Свежая сессия: накопители пусты.
    Input_GetMouseDelta(&dx, &dy);
    EXPECT_EQ(dx, 0);
    EXPECT_EQ(dy, 0);

    // Включение режима требует окна: с окном — true; идемпотентно.
    EXPECT_TRUE(Input_SetMouseRelativeMode(true));
    EXPECT_TRUE(Input_SetMouseRelativeMode(true));

    // Первый grab может «подтолкнуть» реальный motion (warp курсора):
    // осушаем очередь и сбрасываем накопитель — база ровно 0.
    ASSERT_TRUE(Platform_PumpEvents());
    Input_GetMouseDelta(nullptr, nullptr);

    // Три известных дельты + шум-событие НЕ-motion: (+5,-3)+(-2,+7)+0 = (+3,+4).
    push_mouse_motion(5, -3);
    push_mouse_motion(-2, 7);
    push_mouse_motion(0, 0);
    SDL_Event noise;
    SDL_zero(noise);
    noise.type = SDL_EVENT_MOUSE_BUTTON_UP; // копилку не трогает
    ASSERT_EQ(SDL_PushEvent(&noise), true);
    ASSERT_TRUE(Platform_PumpEvents());

    Input_GetMouseDelta(&dx, &dy);
    EXPECT_EQ(dx, 3);
    EXPECT_EQ(dy, 4);

    // Consume-семантика: повторное чтение — нулевое.
    Input_GetMouseDelta(&dx, &dy);
    EXPECT_EQ(dx, 0);
    EXPECT_EQ(dy, 0);

    // Частичное чтение (NULL-канал) тоже опорожняет ОБА накопителя.
    push_mouse_motion(-4, -9);
    ASSERT_TRUE(Platform_PumpEvents());
    Input_GetMouseDelta(nullptr, &dy);
    EXPECT_EQ(dy, -9);
    Input_GetMouseDelta(&dx, &dy);
    EXPECT_EQ(dx, 0);
    EXPECT_EQ(dy, 0);

    // Выключение режима: дальнейшие motion не копятся даже после pump.
    EXPECT_TRUE(Input_SetMouseRelativeMode(false));
    push_mouse_motion(11, 13);
    ASSERT_TRUE(Platform_PumpEvents());
    Input_GetMouseDelta(&dx, &dy);
    EXPECT_EQ(dx, 0);
    EXPECT_EQ(dy, 0);

    // Идемпотентность выключения.
    EXPECT_TRUE(Input_SetMouseRelativeMode(false));
#endif

    Platform_Shutdown();

#ifdef PLATFORM_PC
    // После Shutdown — тот же no-op контракт, что и до Init.
    EXPECT_FALSE(Input_SetMouseRelativeMode(true));
    dx = -1;
    dy = -1;
    Input_GetMouseDelta(&dx, &dy);
    EXPECT_EQ(dx, 0);
    EXPECT_EQ(dy, 0);
#endif
}

TEST(Hal, AudioStubLifecycleAndBeep)
{
    // Аудио не требует дисплея; отсутствие девайса на CI — легитимный false.
    if (!Audio_Init()) {
        GTEST_SKIP() << "Аудио-девайс недоступен — beep-сервис отключён";
    }
    EXPECT_TRUE(Audio_Init()); // идемпотентность

    Audio_PlayBeep(440, 30);   // честный beep (неключевой краш-фактор)
    Audio_PlayBeep(0, 30);     // invalid частота — no-op
    Audio_PlayBeep(99999, 30); // вне диапазона — no-op
    Audio_PlayBeep(440, 0);    // нулевая длительность — no-op

    Audio_Shutdown();
    Audio_Shutdown(); // идемпотентность
}

TEST(HalFileSystem, LoadsValidFileRoundtrip)
{
    static const uint8_t payload[] = {0x01u, 0x02u, 0x7Fu, 0xFFu};

    FILE* file = fopen(TEST_FILE_NAME, "wb");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(fwrite(payload, 1u, sizeof(payload), file), sizeof(payload));
    ASSERT_EQ(fclose(file), 0);

    FileData loaded = FileSystem_LoadFile(TEST_FILE_NAME);
    ASSERT_NE(loaded.data, nullptr);
    EXPECT_EQ(loaded.size, sizeof(payload));
    EXPECT_EQ(memcmp(loaded.data, payload, sizeof(payload)), 0);

    FileSystem_FreeFile(&loaded);
    EXPECT_EQ(loaded.data, nullptr);
    EXPECT_EQ(loaded.size, 0u);

    EXPECT_EQ(remove(TEST_FILE_NAME), 0);
}

TEST(HalFileSystem, MissingFileIsSafeFailure)
{
    FileData missing = FileSystem_LoadFile("retro_hal_missing_file_xyz.tmp");
    EXPECT_EQ(missing.data, nullptr);
    EXPECT_EQ(missing.size, 0u);

    FileSystem_FreeFile(&missing); // NULL-буфер — no-op
    EXPECT_EQ(missing.data, nullptr);

    // Мусорные пути: без краша, {nullptr, 0}.
    FileData empty_path = FileSystem_LoadFile("");
    EXPECT_EQ(empty_path.data, nullptr);
    FileData null_path = FileSystem_LoadFile(nullptr);
    EXPECT_EQ(null_path.data, nullptr);
}

TEST(HalFileSystem, EmptyFileIsSafeFailure)
{
    FILE* file = fopen(TEST_FILE_NAME, "wb");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(fclose(file), 0);

    FileData empty = FileSystem_LoadFile(TEST_FILE_NAME);
    EXPECT_EQ(empty.data, nullptr); // пустой файл → контрактный отказ
    EXPECT_EQ(empty.size, 0u);
    FileSystem_FreeFile(&empty);

    EXPECT_EQ(remove(TEST_FILE_NAME), 0);
}

TEST(HalFileSystem, FreeFileAcceptsNull)
{
    FileSystem_FreeFile(nullptr); // без краша
}
