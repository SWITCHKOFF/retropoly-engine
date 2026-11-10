// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/slave_worker.cpp
//
// Slave SH-2 worker: fixed-size SPSC ring + bounded waits (S25–26).
//
// Протокол:
//   Master: пишет job (fn/arg/seq/kind), release head → wake-up Slave.
//   Slave:  acquire head, исполняет job, release completed/tail.
//   Все shared-поля читаются/пишутся через cache-through alias; кэши CPU
//   не когерентны (Sega Dual CPU User's Guide §7).
//
// Master НЕ вызывает jo_core_wait_for_slave(): у него нет таймаута, а
// зависший Slave не должен блокировать кадр. Завершение отслеживается по
// shared completed + bounded wait (scu_sync).
#include "hal/saturn/slave_worker.h"

#include "core/log/log.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/scu_sync.h"

#include <string.h>

namespace saturn {

namespace {

constexpr uint32_t kSharedMagic = 0x534C5632u;  // "SLV2"
constexpr uint32_t kJobKindWork = 0u;
constexpr uint32_t kJobKindPing = 1u;
constexpr uint32_t kJobKindStop = 2u;
constexpr uint32_t kHandshakeTimeoutMs = 200u;
constexpr uint32_t kStopTimeoutMs = 100u;

struct SlaveSharedJob
{
    SlaveJobFunction fn;
    void* argument;
    uint32_t seq;
    uint32_t kind;
};

struct SlaveSharedControl
{
    uint32_t magic;
    uint32_t head;       /**< Пишет только Master. */
    uint32_t tail;       /**< Пишет только Slave. */
    uint32_t completed;  /**< Sequence последнего завершённого job (Slave). */
    uint32_t stop_ack;   /**< Slave подтвердил STOP. */
    uint32_t ping_ack;   /**< Slave подтвердил PING (диагностика). */
    uint32_t fault_flags;/**< Master → Slave fault injection. */
    uint32_t slave_busy_us; /**< Суммарное время WORK-job'ов (Slave). */
    uint32_t slave_jobs;    /**< Число WORK-job'ов на Slave. */
    uint32_t reserved[6];
    SlaveSharedJob jobs[SLAVE_JOB_QUEUE_CAPACITY];
};

static_assert((SLAVE_JOB_QUEUE_CAPACITY & (SLAVE_JOB_QUEUE_CAPACITY - 1u)) == 0u,
              "SPSC queue capacity must be a power of two");

// Static BSS в Work RAM-H; доступ с обоих CPU — только через cache-through.
alignas(32) SlaveSharedControl s_shared_local;
volatile SlaveSharedControl* s_shared = nullptr;

SlaveWorkerState s_state = SLAVE_WORKER_UNINITIALIZED;
SlaveWorkerDiag s_last_diag = SLAVE_WORKER_OK;
SlaveWorkerStats s_stats = {};
SlaveFaultMode s_fault = SLAVE_FAULT_NONE;

void set_diag(SlaveWorkerDiag diag)
{
    s_last_diag = diag;
}

void log_unavailable(void)
{
    Log_Message(LOG_LEVEL_WARNING, "scu", "slave off: single CPU");
}

void log_failed(const char* reason)
{
    Log_Message(LOG_LEVEL_ERROR, "scu", "slave failed: %s", reason);
}

extern "C" void saturn_slave_drain(void);

bool wait_for_completed(uint32_t expected, uint32_t timeout_ms, ScuSyncDiag* out_sync)
{
    if (s_shared == nullptr) {
        if (out_sync != nullptr) {
            *out_sync = SCU_SYNC_NULL_ARGUMENT;
        }
        return false;
    }
    return wait_u32_until(&s_shared->completed, expected, timeout_ms, out_sync);
}

/**
 * Постановка job'а в очередь. Для WORK требуется READY; PING/STOP — только
 * инициализированный shared block.
 */
bool enqueue(uint32_t kind, SlaveJobFunction function, void* argument, SlaveWorkerDiag* out_diag)
{
    if (out_diag != nullptr) {
        *out_diag = SLAVE_WORKER_OK;
    }
    if (s_shared == nullptr || s_shared->magic != kSharedMagic) {
        set_diag(SLAVE_WORKER_NOT_INITIALIZED);
        if (out_diag != nullptr) {
            *out_diag = SLAVE_WORKER_NOT_INITIALIZED;
        }
        return false;
    }
    if (kind == kJobKindWork && s_state != SLAVE_WORKER_READY) {
        const SlaveWorkerDiag diag = (s_state == SLAVE_WORKER_FAILED)
                                         ? SLAVE_WORKER_ALREADY_FAILED
                                         : SLAVE_WORKER_DIAG_UNAVAILABLE;
        set_diag(diag);
        if (out_diag != nullptr) {
            *out_diag = diag;
        }
        return false;
    }

    const uint32_t head = shared_load_u32(&s_shared->head);
    const uint32_t tail = shared_load_u32(&s_shared->tail);
    if ((head - tail) >= SLAVE_JOB_QUEUE_CAPACITY) {
        if (kind == kJobKindWork) {
            ++s_stats.queue_overflows;
        }
        set_diag(SLAVE_WORKER_QUEUE_FULL);
        if (out_diag != nullptr) {
            *out_diag = SLAVE_WORKER_QUEUE_FULL;
        }
        return false;
    }

    const uint32_t seq = head + 1u;
    const uint32_t index = head & (SLAVE_JOB_QUEUE_CAPACITY - 1u);
    volatile SlaveSharedJob& job = s_shared->jobs[index];
    job.fn = function;
    job.argument = argument;
    job.seq = seq;
    job.kind = kind;
    shared_barrier();
    shared_store_u32(&s_shared->head, seq);

    if (!rpe_saturn_slave_post(saturn_slave_drain)) {
        if (kind == kJobKindWork) {
            ++s_stats.post_failures;
        }
        s_state = SLAVE_WORKER_FAILED;
        set_diag(SLAVE_WORKER_POST_FAILED);
        if (out_diag != nullptr) {
            *out_diag = SLAVE_WORKER_POST_FAILED;
        }
        return false;
    }
    return true;
}

bool start_handshake(void)
{
    memset(&s_shared_local, 0, sizeof(s_shared_local));
    s_shared_local.magic = kSharedMagic;
    s_shared = static_cast<volatile SlaveSharedControl*>(shared_ptr(&s_shared_local));
    if (s_shared == nullptr) {
        set_diag(SLAVE_WORKER_DIAG_UNAVAILABLE);
        return false;
    }

    ++s_stats.start_attempts;
    SlaveWorkerDiag post_diag = SLAVE_WORKER_OK;
    if (!enqueue(kJobKindPing, nullptr, nullptr, &post_diag)) {
        ++s_stats.start_failures;
        s_state = SLAVE_WORKER_FAILED;
        log_failed("handshake post");
        return false;
    }

    ScuSyncDiag sync_diag = SCU_SYNC_OK;
    if (!wait_for_completed(1u, kHandshakeTimeoutMs, &sync_diag)) {
        ++s_stats.start_failures;
        ++s_stats.timeouts;
        s_state = SLAVE_WORKER_FAILED;
        set_diag(SLAVE_WORKER_WAIT_TIMEOUT);
        log_failed("handshake timeout");
        return false;
    }

    s_state = SLAVE_WORKER_READY;
    set_diag(SLAVE_WORKER_OK);
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// Slave-side drain (вызывается Jo Engine на Slave)
// ---------------------------------------------------------------------------

extern "C" void saturn_slave_drain(void)
{
    if (s_shared == nullptr) {
        return;
    }

    uint32_t tail = shared_load_u32(&s_shared->tail);
    for (;;) {
        const uint32_t head = shared_load_u32(&s_shared->head);
        if (tail == head) {
            break;
        }
        shared_barrier();

        const uint32_t index = tail & (SLAVE_JOB_QUEUE_CAPACITY - 1u);
        volatile SlaveSharedJob& job = s_shared->jobs[index];
        SlaveJobFunction function = job.fn;
        void* argument = job.argument;
        const uint32_t seq = job.seq;
        const uint32_t kind = job.kind;

        if (kind == kJobKindWork) {
            if ((s_shared->fault_flags & 1u) != 0u) {
                // Fault injection (SLAVE_FAULT_HANG_NEXT): Slave зависает.
                for (;;) {
                    cpu_relax();
                }
            }
            const uint16_t start = profile_ticks();
            if (function != nullptr) {
                function(argument);
            }
            const uint16_t end = profile_ticks();
            s_shared->slave_busy_us += profile_elapsed_us(start, end);
            ++s_shared->slave_jobs;
        } else if (kind == kJobKindPing) {
            s_shared->ping_ack = seq;
        } else if (kind == kJobKindStop) {
            s_shared->stop_ack = 1u;
        }

        shared_store_u32(&s_shared->completed, seq);
        ++tail;
        shared_store_u32(&s_shared->tail, tail);

        if (kind == kJobKindStop) {
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Master-side API
// ---------------------------------------------------------------------------

bool slave_worker_init(void)
{
    if (s_state == SLAVE_WORKER_READY) {
        return true;
    }
    if (s_state == SLAVE_WORKER_STOPPED) {
        return slave_worker_restart();
    }
    if (s_state == SLAVE_WORKER_FAILED) {
        set_diag(SLAVE_WORKER_ALREADY_FAILED);
        return false;
    }
    if (s_state == SLAVE_WORKER_UNAVAILABLE && !rpe_saturn_slave_available()) {
        return false;
    }

    if (!rpe_saturn_slave_available()) {
        s_state = SLAVE_WORKER_UNAVAILABLE;
        set_diag(SLAVE_WORKER_DIAG_UNAVAILABLE);
        log_unavailable();
        return false;
    }

    if (!start_handshake()) {
        return false;
    }
    Log_Message(LOG_LEVEL_INFO, "scu", "slave ready, queue=%u",
                static_cast<unsigned>(SLAVE_JOB_QUEUE_CAPACITY));
    return true;
}

void slave_worker_shutdown(void)
{
    if (s_state != SLAVE_WORKER_READY) {
        return;
    }

    const uint32_t expected = shared_load_u32(&s_shared->head);
    ScuSyncDiag sync_diag = SCU_SYNC_OK;
    if (!wait_for_completed(expected, kStopTimeoutMs, &sync_diag)) {
        ++s_stats.timeouts;
        s_state = SLAVE_WORKER_FAILED;
        set_diag(SLAVE_WORKER_STOP_TIMEOUT);
        log_failed("stop wait");
        return;
    }

    shared_store_u32(&s_shared->stop_ack, 0u);
    SlaveWorkerDiag post_diag = SLAVE_WORKER_OK;
    if (!enqueue(kJobKindStop, nullptr, nullptr, &post_diag)) {
        s_state = SLAVE_WORKER_FAILED;
        set_diag(SLAVE_WORKER_STOP_TIMEOUT);
        log_failed("stop post");
        return;
    }

    if (!wait_u32_until(&s_shared->stop_ack, 1u, kStopTimeoutMs, &sync_diag)) {
        ++s_stats.timeouts;
        s_state = SLAVE_WORKER_FAILED;
        set_diag(SLAVE_WORKER_STOP_TIMEOUT);
        log_failed("stop ack");
        return;
    }

    s_state = SLAVE_WORKER_STOPPED;
    set_diag(SLAVE_WORKER_OK);
    Log_Message(LOG_LEVEL_INFO, "scu", "slave stopped");
}

bool slave_worker_restart(void)
{
    if (s_state != SLAVE_WORKER_STOPPED) {
        set_diag(SLAVE_WORKER_RESTART_REJECTED);
        return false;
    }
    if (!rpe_saturn_slave_available()) {
        s_state = SLAVE_WORKER_UNAVAILABLE;
        set_diag(SLAVE_WORKER_DIAG_UNAVAILABLE);
        log_unavailable();
        return false;
    }
    if (!start_handshake()) {
        return false;
    }
    ++s_stats.restarts;
    Log_Message(LOG_LEVEL_INFO, "scu", "slave restart: ok");
    return true;
}

bool slave_worker_available(void)
{
    return s_state == SLAVE_WORKER_READY;
}

SlaveWorkerState slave_worker_state(void)
{
    return s_state;
}

bool slave_worker_dispatch(SlaveJobFunction function, void* argument, SlaveWorkerDiag* out_diag)
{
    if (function == nullptr) {
        set_diag(SLAVE_WORKER_NULL_JOB);
        if (out_diag != nullptr) {
            *out_diag = SLAVE_WORKER_NULL_JOB;
        }
        return false;
    }
    if (s_state == SLAVE_WORKER_FAILED) {
        set_diag(SLAVE_WORKER_ALREADY_FAILED);
        if (out_diag != nullptr) {
            *out_diag = SLAVE_WORKER_ALREADY_FAILED;
        }
        return false;
    }
    if (s_state != SLAVE_WORKER_READY) {
        set_diag(SLAVE_WORKER_DIAG_UNAVAILABLE);
        if (out_diag != nullptr) {
            *out_diag = SLAVE_WORKER_DIAG_UNAVAILABLE;
        }
        return false;
    }

    if (s_fault == SLAVE_FAULT_HANG_NEXT) {
        s_shared->fault_flags |= 1u;
        s_fault = SLAVE_FAULT_NONE;
    }

    if (!enqueue(kJobKindWork, function, argument, out_diag)) {
        return false;
    }
    ++s_stats.submitted;
    set_diag(SLAVE_WORKER_OK);
    return true;
}

bool slave_worker_wait(uint32_t timeout_ms, SlaveWorkerDiag* out_diag)
{
    ++s_stats.wait_calls;
    if (out_diag != nullptr) {
        *out_diag = SLAVE_WORKER_OK;
    }
    if (s_state == SLAVE_WORKER_FAILED) {
        set_diag(SLAVE_WORKER_ALREADY_FAILED);
        if (out_diag != nullptr) {
            *out_diag = SLAVE_WORKER_ALREADY_FAILED;
        }
        return false;
    }
    if (s_state != SLAVE_WORKER_READY) {
        set_diag(SLAVE_WORKER_DIAG_UNAVAILABLE);
        if (out_diag != nullptr) {
            *out_diag = SLAVE_WORKER_DIAG_UNAVAILABLE;
        }
        return false;
    }

    const uint32_t expected = shared_load_u32(&s_shared->head);
    const uint16_t start = profile_ticks();
    ScuSyncDiag sync_diag = SCU_SYNC_OK;
    const bool ok = wait_u32_until(&s_shared->completed, expected, timeout_ms, &sync_diag);
    const uint16_t end = profile_ticks();
    const uint32_t elapsed = profile_elapsed_us(start, end);

    s_stats.last_wait_us = elapsed;
    s_stats.total_wait_us += elapsed;
    if (elapsed > s_stats.max_wait_us) {
        s_stats.max_wait_us = elapsed;
    }

    if (!ok) {
        ++s_stats.timeouts;
        s_state = SLAVE_WORKER_FAILED;
        set_diag(SLAVE_WORKER_WAIT_TIMEOUT);
        if (out_diag != nullptr) {
            *out_diag = SLAVE_WORKER_WAIT_TIMEOUT;
        }
        log_failed("wait timeout");
        return false;
    }

    cache_invalidate_local();
    s_stats.completed = s_stats.submitted;
    s_stats.slave_busy_us = shared_load_u32(&s_shared->slave_busy_us);
    s_stats.slave_jobs = shared_load_u32(&s_shared->slave_jobs);
    set_diag(SLAVE_WORKER_OK);
    return true;
}

void slave_worker_record_fallback(void)
{
    ++s_stats.fallback_frames;
}

uint32_t slave_worker_queue_depth(void)
{
    if (s_shared == nullptr) {
        return 0u;
    }
    return shared_load_u32(&s_shared->head) - shared_load_u32(&s_shared->tail);
}

uint32_t slave_worker_shared_bytes(void)
{
    return static_cast<uint32_t>(sizeof(SlaveSharedControl));
}

const SlaveWorkerStats& slave_worker_stats(void)
{
    return s_stats;
}

SlaveWorkerDiag slave_worker_last_diag(void)
{
    return s_last_diag;
}

const char* slave_worker_state_string(SlaveWorkerState state)
{
    switch (state) {
        case SLAVE_WORKER_UNINITIALIZED:
            return "uninitialized";
        case SLAVE_WORKER_READY:
            return "ready";
        case SLAVE_WORKER_STOPPED:
            return "stopped";
        case SLAVE_WORKER_UNAVAILABLE:
            return "unavailable";
        case SLAVE_WORKER_FAILED:
            return "failed";
        case SLAVE_WORKER_STATE_COUNT:
        default:
            return "unknown";
    }
}

const char* slave_worker_diag_string(SlaveWorkerDiag diag)
{
    switch (diag) {
        case SLAVE_WORKER_OK:
            return "ok";
        case SLAVE_WORKER_NOT_INITIALIZED:
            return "not initialized";
        case SLAVE_WORKER_DIAG_UNAVAILABLE:
            return "unavailable";
        case SLAVE_WORKER_QUEUE_FULL:
            return "queue full";
        case SLAVE_WORKER_POST_FAILED:
            return "post failed";
        case SLAVE_WORKER_WAIT_TIMEOUT:
            return "wait timeout";
        case SLAVE_WORKER_STOP_TIMEOUT:
            return "stop timeout";
        case SLAVE_WORKER_ALREADY_FAILED:
            return "already failed";
        case SLAVE_WORKER_RESTART_REJECTED:
            return "restart rejected";
        case SLAVE_WORKER_NULL_JOB:
            return "null job";
        case SLAVE_WORKER_COUNT:
        default:
            return "unknown";
    }
}

void slave_worker_set_fault(SlaveFaultMode mode)
{
    s_fault = (mode == SLAVE_FAULT_HANG_NEXT) ? SLAVE_FAULT_HANG_NEXT : SLAVE_FAULT_NONE;
    Log_Message(LOG_LEVEL_WARNING, "scu", "fault: %s",
                (s_fault == SLAVE_FAULT_HANG_NEXT) ? "hang next" : "none");
}

SlaveFaultMode slave_worker_fault(void)
{
    return s_fault;
}

namespace {

void flood_blocker(void* argument)
{
    (void)argument;
    uint16_t previous = profile_ticks();
    uint32_t elapsed_us = 0u;
    while (elapsed_us < 20000u) {
        const uint16_t now = profile_ticks();
        elapsed_us += profile_elapsed_us(previous, now);
        previous = now;
        cpu_relax();
    }
}

void flood_noop(void* argument)
{
    (void)argument;
}

}  // namespace

uint32_t slave_worker_debug_flood_probe(void)
{
    if (s_state != SLAVE_WORKER_READY) {
        return 0u;
    }

    uint32_t overflows = 0u;
    if (!slave_worker_dispatch(flood_blocker, nullptr)) {
        return 0u;
    }

    // Blocker держит Slave 20 мс; Master успевает заполнить очередь.
    for (uint32_t i = 0u; i < SLAVE_JOB_QUEUE_CAPACITY + 2u; ++i) {
        if (!slave_worker_dispatch(flood_noop, nullptr)) {
            ++overflows;
            break;
        }
    }

    slave_worker_wait(200u);
    return overflows;
}

void slave_worker_test_reset(void)
{
    s_shared = nullptr;
    s_state = SLAVE_WORKER_UNINITIALIZED;
    s_last_diag = SLAVE_WORKER_OK;
    memset(&s_stats, 0, sizeof(s_stats));
    s_fault = SLAVE_FAULT_NONE;
    memset(&s_shared_local, 0, sizeof(s_shared_local));
}

}  // namespace saturn
