// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/log/log.h
//
// Минимальный логгер ядра (diagnostics contract §1: «прямой ввод-вывод
// запрещён; диагностика — исключительно через src/core/log»).
//
// Долг Спринтов 3–4, закрываемый Спринтом 7–8 (sprint_07_08.md, неделя 14:
// «компиляция/линковка шейдеров с логированием ошибок через Logger»).
//
// Контракты:
//   - весь вывод — только через Log_Message; выход ФАЙЛОМ (PC) —
//     «retro_engine.log» в текущем рабочем каталоге, режим перезаписи
//     (Determinism: Init = «чистая» сессия);
//   - формат строки детерминированный: "[LEVEL][domain] message\n";
//   - без heap, без iostream/printf-вызовом наружу; форматирование —
//     фиксированный стековый буфер (усечение при переполнении, без UB);
//   - длина строки ограничена LOG_MESSAGE_MAX (включая суффикс «…»);
//   - Thread-safety MVP: однопоточная подсистема (движок — single-thread);
//   - PS1 (freestanding): файловый sink отключён — API безопасно no-op
//     (compile-time specialization допускается нормативом для такого
//     слоя — логгер единственный модуль, которому нужен файловый I/O).
#pragma once

#include <stdint.h>

/** Уровень журнала. Порядок — контракт (расширение — только перед COUNT). */
typedef enum
{
    LOG_LEVEL_INFO = 0,     /**< Обычный ход (инициализация, статус). */
    LOG_LEVEL_WARNING,      /**< Подозрительное, но безопасное поведение. */
    LOG_LEVEL_ERROR,        /**< Реальная ошибка (Init/scape/asset/render). */
    LOG_LEVEL_COUNT
} LogLevel;

/** Имя файла журнала (создаётся в cwd процесса; PC-only). */
#define LOG_FILE_NAME "retro_engine.log"

/** Максимум символов сообщения в строке (буфер + суффикс «…»). */
#define LOG_MESSAGE_MAX 256

/**
 * Инициализация логгера: открывает @ref LOG_FILE_NAME (PC), буферизация —
 * построчная; каждый сообщения flush сразу (детерминизм и диагностика
 * crash-сценария). Повторный вызов ПОСЛЕ init — идемпотентен (текущий
 * дескриптор сохраняется, файл НЕ переоткрывается); вызовы до Init и после
 * Shutdown — no-op (без краша).
 *
 * @return true — файл открыт (или уже был); false — файловый sink
 *         недоступен (PS1: freestanding — no-op; PC: отказ открытия не
 *         мешает работе движка, сообщения просто не пишутся).
 */
bool Log_Init(void);

/** Закрытие файла журнала. Идемпотентна; безопасна без Init. */
void Log_Shutdown(void);

/**
 * @param level Уровень (вне [0..LOG_LEVEL_COUNT) — трактуется как ERROR).
 * @param domain Короткая метка подсистемы без разделителей: "hal", "render",
 *               "ecs"; NULL/пустая — "?".
 * @param format printf-подобный формат (%d %u %x %s %p; без %f — float
 *               запрещён в игровом коде); NULL — пишет "(null)".
 *               Длинное сообщение усекается к LOG_MESSAGE_MAX-1 и
 *               помечается "…".
 */
void Log_Message(LogLevel level, const char* domain, const char* format, ...);
