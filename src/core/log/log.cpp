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
#elif defined(PLATFORM_SATURN)
#include "hal/saturn/jo_saturn_bridge.h"
/// VDP2-текст инициализируется вместе с Jo Engine core.
static bool s_log_inited = false;
#elif defined(PLATFORM_N64)
#include "hal/n64/n64_bridge.h"
/// ISViewer/USB debug-канал инициализируется в rpe_n64_core_init.
static bool s_log_inited = false;
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
#elif defined(PLATFORM_SATURN)
    // Saturn: sink — VDP2-строка 27 (NBG0). Файлов/CD-записи нет; после
    // Platform_Init доступен всегда (строку выводит C-мост).
    s_log_inited = true;
    return true;
#elif defined(PLATFORM_N64)
    // N64: sink — ISViewer (эмулятор) / USB debug-канал (libdragon debugf).
    // Мост инициализируется до Log_Init в Platform_Init.
    s_log_inited = rpe_n64_core_ready();
    return s_log_inited;
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
#elif defined(PLATFORM_SATURN)
    s_log_inited = false;
#elif defined(PLATFORM_N64)
    s_log_inited = false;
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
#elif defined(PLATFORM_SATURN)
    if (!s_log_inited) {
        return; // No-op вне [Init..Shutdown] — контракт log.h.
    }
    // Форматирование — в C-мосте через jo_vsnprintf (без newlib/heap),
    // вывод — VDP2-строка 27.
    va_list args;
    va_start(args, format);
    rpe_saturn_log_write_va((int)level, (domain != nullptr && domain[0] != '\0') ? domain : "?",
                            format, &args);
    va_end(args);
#elif defined(PLATFORM_N64)
    if (!s_log_inited) {
        return; // No-op вне [Init..Shutdown] — контракт log.h.
    }
    // Форматирование — в C-мосте (фиксированный буфер), вывод — ISViewer/USB.
    va_list args;
    va_start(args, format);
    rpe_n64_log_write_va((int)level, (domain != nullptr && domain[0] != '\0') ? domain : "?",
                         format, &args);
    va_end(args);
#else
    (void)level;
    (void)domain;
    (void)format;
#endif
}
