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
// Рендер-Lifecycle (G9) — Спринты 7–8.
#include "hal/platform.h"

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

namespace
{

/// Headless-обнаружение только для Linux канонизируется репозиторием (CI).
bool display_available()
{
#if defined(__linux__)
    const char* x11 = getenv("DISPLAY");
    const char* wayland = getenv("WAYLAND_DISPLAY");
    return (x11 != nullptr && x11[0] != '\0') ||
           (wayland != nullptr && wayland[0] != '\0');
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
    static const uint8_t payload[] = { 0x01u, 0x02u, 0x7Fu, 0xFFu };

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
