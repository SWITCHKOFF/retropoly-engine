// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/log/log_test.cpp
//
// Тесты логгера ядра (src/core/log) — Спринт 7–8 (логирование ошибок
// компиляции шейдеров), закрывающий долг Спринтов 3–4 (STYLE_GUIDE §3).
// План тестирования §10.1: «Logger: init/log/shutdown, файл создан, формат».
//
// Файл журнала — LOG_FILE_NAME в cwd CTest (бинарный каталог tests/) —
// безопасно перезаписывается и удаляется самим тестом.
#include "core/log/log.h"

#include <gtest/gtest.h>

#include <stdio.h>
#include <string.h>

namespace
{

/// Чтение журнала целиком (валидные размеры — файл ограничен тестом).
bool read_log_file(char* out, unsigned long out_size)
{
    FILE* file = fopen(LOG_FILE_NAME, "rb");
    if (file == nullptr) {
        return false;
    }
    const size_t n = fread(out, 1, out_size - 1, file);
    out[n] = '\0';
    fclose(file);
    return true;
}

/// Удаление файла журнала (начало каждого кейса — чистое состояние).
void remove_log_file()
{
    remove(LOG_FILE_NAME);
}

} // namespace

TEST(Log, InitCreatesFileAndShutdownClosesIt)
{
    remove_log_file();
    ASSERT_TRUE(Log_Init());

    // Файл существует и открывается на чтение (создан Init-ом).
    FILE* file = fopen(LOG_FILE_NAME, "rb");
    ASSERT_NE(file, nullptr);
    fclose(file);

    Log_Shutdown();

    // Повторный Shutdown идемпотентен (без краша).
    Log_Shutdown();
    remove_log_file();
}

TEST(Log, MessageFormatMatchesContract)
{
    remove_log_file();
    ASSERT_TRUE(Log_Init());

    // Контракт формата: "[LEVEL][domain] message\n".
    Log_Message(LOG_LEVEL_INFO, "test-log", "hello %d", 42);
    Log_Message(LOG_LEVEL_WARNING, "test-log", "warn");
    Log_Message(LOG_LEVEL_ERROR, "test-log", "err %s", "abc");
    Log_Shutdown();

    char buffer[2048];
    ASSERT_TRUE(read_log_file(buffer, sizeof(buffer)));
    ASSERT_NE(strstr(buffer, "[INFO][test-log] hello 42\n"), nullptr);
    ASSERT_NE(strstr(buffer, "[WARNING][test-log] warn\n"), nullptr);
    ASSERT_NE(strstr(buffer, "[ERROR][test-log] err abc\n"), nullptr);
    remove_log_file();
}

TEST(Log, MessageBeforeInitIsSafeNoOp)
{
    remove_log_file();
    Log_Shutdown(); // Гарантия «не инициализирован».

    // Сообщение до Init / после Shutdown — no-op без краша; файл не создаётся.
    Log_Message(LOG_LEVEL_INFO, "test-log", "dropped");
    Log_Message(LOG_LEVEL_ERROR, nullptr, "%s", "also dropped");

    FILE* file = fopen(LOG_FILE_NAME, "rb");
    ASSERT_EQ(file, nullptr); // Файл NOT создан сообщениями вне Init.
    remove_log_file();
}

TEST(Log, RepeatInitIsIdempotentAndKeepsContent)
{
    remove_log_file();
    ASSERT_TRUE(Log_Init());
    Log_Message(LOG_LEVEL_INFO, "test-log", "first");
    // Повторный Init: дескриптор тот же, файл НЕ переоткрывается/НЕ обнуляется.
    ASSERT_TRUE(Log_Init());
    Log_Message(LOG_LEVEL_INFO, "test-log", "second");
    Log_Shutdown();

    char buffer[2048];
    ASSERT_TRUE(read_log_file(buffer, sizeof(buffer)));
    ASSERT_NE(strstr(buffer, "first"), nullptr);
    ASSERT_NE(strstr(buffer, "second"), nullptr);
    remove_log_file();
}

TEST(Log, OversizedMessageIsTruncatedWithoutUB)
{
    remove_log_file();
    ASSERT_TRUE(Log_Init());

    // 600 символов 'x' > LOG_MESSAGE_MAX: усечение + суффикс «...», без UB.
    char big[600];
    memset(big, 'x', sizeof(big));
    big[sizeof(big) - 1] = '\0';
    Log_Message(LOG_LEVEL_INFO, "test-log", "%s", big);
    Log_Shutdown();

    char buffer[2048];
    ASSERT_TRUE(read_log_file(buffer, sizeof(buffer)));
    const char* tail = strstr(buffer, "[INFO][test-log] x");
    ASSERT_NE(tail, nullptr);
    const char* suffix = strstr(tail, "....\n"); // Суффикс усечения присутствует.
    ASSERT_NE(suffix, nullptr);
    // До усечённого хвоста были реальные 'x' (буфер заполнился символами).
    ASSERT_GT(suffix - tail, static_cast<ptrdiff_t>(0));
    remove_log_file();
}
