// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/slave_worker.h
//
// Slave SH-2 worker и fixed-size SPSC job queue (Спринт 25–26).
//
// Контракты:
//   - очередь фиксированной ёмкости (8), без динамических аллокаций;
//   - переполнение/недоступность Slave — явный false + диагностика;
//   - Master НИКОГДА не ждёт Slave бесконечно: у каждого wait есть таймаут;
//   - после timeout/ошибки worker переходит в FAILED и Master работает в
//     single-CPU fallback до перезагрузки;
//   - Slave в v0.2.0 не трогает VDP1/VDP2/периферию — только shared WRAM.
//
// Нормативные документы: docs/adr/ADR-009-master-slave.md,
// docs/SATURN.md §13.
#pragma once

#include <stdint.h>

namespace saturn {

/** Функция job'а, исполняемого на Slave (или Master в fallback). */
typedef void (*SlaveJobFunction)(void* argument);

/** Ёмкость SPSC-очереди (степень двойки). */
constexpr uint32_t SLAVE_JOB_QUEUE_CAPACITY = 8u;

/** Состояние worker'а (Master-only; Slave не читает). */
enum SlaveWorkerState
{
    SLAVE_WORKER_UNINITIALIZED = 0,
    SLAVE_WORKER_READY,
    SLAVE_WORKER_STOPPED,
    SLAVE_WORKER_UNAVAILABLE,
    SLAVE_WORKER_FAILED,
    SLAVE_WORKER_STATE_COUNT
};

/** Диагностические коды worker'а. Порядок — контракт. */
enum SlaveWorkerDiag
{
    SLAVE_WORKER_OK = 0,
    SLAVE_WORKER_NOT_INITIALIZED,
    SLAVE_WORKER_DIAG_UNAVAILABLE,
    SLAVE_WORKER_QUEUE_FULL,
    SLAVE_WORKER_POST_FAILED,
    SLAVE_WORKER_WAIT_TIMEOUT,
    SLAVE_WORKER_STOP_TIMEOUT,
    SLAVE_WORKER_ALREADY_FAILED,
    SLAVE_WORKER_RESTART_REJECTED,
    SLAVE_WORKER_NULL_JOB,
    SLAVE_WORKER_COUNT
};

/** Fault injection для ручной приёмки (S25–26, неделя 52). */
enum SlaveFaultMode
{
    SLAVE_FAULT_NONE = 0,
    SLAVE_FAULT_HANG_NEXT = 1, /**< Следующий WORK-job зависает на Slave. */
    SLAVE_FAULT_COUNT
};

/** Метрики Master/Slave. Часть полей обновляется после успешного wait(). */
struct SlaveWorkerStats
{
    uint32_t submitted;          /**< WORK-jobs, принятые очередью. */
    uint32_t completed;          /**< WORK-jobs, подтверждённые completed. */
    uint32_t queue_overflows;    /**< Отказы из-за полной очереди. */
    uint32_t post_failures;      /**< Отказы постановки в очередь Jo Engine. */
    uint32_t timeouts;           /**< Таймауты handshake/wait/stop. */
    uint32_t fallback_frames;    /**< Кадры, где workload считал Master. */
    uint32_t start_attempts;     /**< Попытки handshake. */
    uint32_t start_failures;     /**< Неудачные handshake. */
    uint32_t restarts;           /**< Успешные restart из STOPPED. */
    uint32_t wait_calls;         /**< Вызовы wait(). */
    uint32_t last_wait_us;       /**< Длительность последнего wait(). */
    uint32_t total_wait_us;      /**< Суммарное время ожидания Master. */
    uint32_t max_wait_us;        /**< Максимальное время ожидания Master. */
    uint32_t slave_busy_us;      /**< Суммарное время WORK-job'ов на Slave. */
    uint32_t slave_jobs;         /**< WORK-jobs, исполненные на Slave. */
};

/**
 * Инициализация worker'а и handshake (PING-job, таймаут 200 мс).
 *
 * Если Saturn собран без dual-CPU или Slave не стартовал — возвращает false
 * и переводит worker в UNAVAILABLE (Master продолжает в fallback).
 * Идемпотентна; повторный вызов после STOPPED выполняет restart.
 *
 * @return true, если Slave готов принимать job'ы.
 */
bool slave_worker_init(void);

/** Логическая остановка worker'а (STOP-job, таймаут 100 мс). Идемпотентна. */
void slave_worker_shutdown(void);

/**
 * Повторный запуск после STOPPED (handshake заново).
 *
 * @return true при успехе; false для FAILED/UNAVAILABLE (нужна перезагрузка).
 */
bool slave_worker_restart(void);

/** @return true, если worker READY и Slave принимает job'ы. */
bool slave_worker_available(void);

/** @return Текущее состояние worker'а. */
SlaveWorkerState slave_worker_state(void);

/**
 * Постановка job'а в SPSC-очередь и wake-up Slave.
 *
 * Не блокирует Master. Переполнение — false + SLAVE_WORKER_QUEUE_FULL.
 *
 * @param function Функция job'а (не nullptr).
 * @param argument Аргумент (может быть nullptr); для cross-CPU данных
 *                 обязан указывать на cache-through alias.
 * @param out_diag Диагностика (может быть nullptr).
 * @return true, если job принят.
 */
bool slave_worker_dispatch(SlaveJobFunction function, void* argument,
                           SlaveWorkerDiag* out_diag = nullptr);

/**
 * Ожидание завершения всех поставленных WORK-job'ов.
 *
 * @param timeout_ms Таймаут; 0 — только проверка состояния.
 * @param out_diag Диагностика (может быть nullptr).
 * @return true, если completed == head; false при таймауте/FAILED.
 */
bool slave_worker_wait(uint32_t timeout_ms, SlaveWorkerDiag* out_diag = nullptr);

/** Учёт fallback-кадра (вызывает workload). */
void slave_worker_record_fallback(void);

/** @return Текущая глубина очереди (head − tail). */
uint32_t slave_worker_queue_depth(void);

/** @return Размер shared control block в байтах. */
uint32_t slave_worker_shared_bytes(void);

/** @return Метрики (Master-only snapshot). */
const SlaveWorkerStats& slave_worker_stats(void);

/** @return Последняя диагностика. */
SlaveWorkerDiag slave_worker_last_diag(void);

/** @return Статическая строка состояния. */
const char* slave_worker_state_string(SlaveWorkerState state);

/** @return Статическая строка диагностики. */
const char* slave_worker_diag_string(SlaveWorkerDiag diag);

/** Включить fault injection (HANG_NEXT). */
void slave_worker_set_fault(SlaveFaultMode mode);

/** @return Текущий fault mode. */
SlaveFaultMode slave_worker_fault(void);

/**
 * Fault injection: проверка переполнения очереди на реальном Saturn.
 *
 * Ставит «blocker»-job (20 мс на Slave), затем заполняет очередь no-op
 * job'ами до первого SLAVE_WORKER_QUEUE_FULL. После этого ждёт завершения
 * всех job'ов (200 мс).
 *
 * @return Число отказов QUEUE_FULL (0 — очередь не переполнилась).
 */
uint32_t slave_worker_debug_flood_probe(void);

/**
 * Host-test hook: полный сброс состояния worker'а.
 *
 * НЕ вызывать на Saturn при работающем Slave: обнуляет shared control,
 * который читает Slave. Предназначен только для изоляции host-тестов.
 */
void slave_worker_test_reset(void);

}  // namespace saturn
