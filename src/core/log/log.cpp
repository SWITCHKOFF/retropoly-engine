// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/log/log.cpp
//
// Реализация логгера ядра (контракты — src/core/log/log.h).
// PC: построчный файловый sink (fopen — «файл создан», приемник журнала
// §10.1 плана тестирования). PS1 (freestanding): compile-time
// specialization — sink отключён, API безопасно no-op.
//
// Форматирование: vsnprintf в фиксированный стековый буфер; никаких
// heap/iostream; flush после каждого сообщения (диагностика сбоев).
#include "core/log/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_PC
/// Файловый дескриптор журнала (NULL — логгер не инициализирован).
static FILE* s_log_file = NULL;
#endif

bool Log_Init(void)
{
#ifdef PLATFORM_PC
    // Идемпотентность: повторный Init НЕ переоткрывает файл (но дескриптор,
    // потерянный Shutdown, восстанавливается честным открытием заново).
    if (s_log_file != NULL) {
        return true;
    }
    s_log_file = fopen(LOG_FILE_NAME, "w");
    return s_log_file != NULL;
#else
    // PS1: файловой подсистемы в MVP нет — Sink отключён; API сохраняет
    // контракт (false — «недоступен») без краша.
    return false;
#endif
}

void Log_Shutdown(void)
{
#ifdef PLATFORM_PC
    if (s_log_file != NULL) {
        fclose(s_log_file);
        s_log_file = NULL;
    }
#endif
}

#ifdef PLATFORM_PC
/**
 * Преобразование уровня в детерминированный тег строки журнала.
 * Вне перечисления — "ERROR" (отказ safe-degradation, log.h contract).
 */
static const char* log_level_tag(LogLevel level)
{
    switch (level) {
    case LOG_LEVEL_INFO:
        return "INFO";
    case LOG_LEVEL_WARNING:
        return "WARNING";
    case LOG_LEVEL_ERROR:
    default:
        return "ERROR";
    }
}
#endif // PLATFORM_PC

void Log_Message(LogLevel level, const char* domain, const char* format, ...)
{
#ifdef PLATFORM_PC
    if (s_log_file == NULL) {
        return; // No-op вне [Init..Shutdown] — контракт log.h.
    }

    char buffer[LOG_MESSAGE_MAX];
    va_list args;
    va_start(args, format);
    // vsnprintf: гарантированное завершение nul; усечение — по контракту.
    const int written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (written <= 0) {
        return; // Ошибка форматирования / пусто — строка не пишется.
    }

    if (written >= (int)sizeof(buffer)) {
        // Усечение сообщения: ASCII-суффикс «...» (UTF-8-надёжно — не режет
        // многобайтную последовательность, только приписывает точки).
        buffer[LOG_MESSAGE_MAX - 5] = '.';
        buffer[LOG_MESSAGE_MAX - 4] = '.';
        buffer[LOG_MESSAGE_MAX - 3] = '.';
        buffer[LOG_MESSAGE_MAX - 2] = '.';
        buffer[LOG_MESSAGE_MAX - 1] = '\0';
    }

    // Формат строки — контракт log.h: "[LEVEL][domain] message\n".
    // fprintf — внутренняя механика sink, наружный printf запрещён стайлгайдом.
    fprintf(s_log_file, "[%s][%s] %s\n", log_level_tag(level),
            (domain != NULL && domain[0] != '\0') ? domain : "?", buffer);
    // Flush каждого сообщения (диагностика сбоев, детерминизм файла).
    fflush(s_log_file);
#else
    (void)level;
    (void)domain;
    (void)format;
#endif // PLATFORM_PC
}
