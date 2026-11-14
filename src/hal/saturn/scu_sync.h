// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/scu_sync.h
//
// Синхронизация Master + Slave SH-2 (Спринт 25–26, недели 49–52).
//
// Нормативные источники:
//   - docs/plans/sprint_25_26.md;
//   - docs/adr/ADR-009-master-slave.md;
//   - Sega Dual CPU User's Guide ST-202-R1 (FRT input capture, cache-through),
//     SCU User's Manual ST-097-R5 (SCU не имеет hardware-семафоров).
//
// Модель памяти Saturn: у каждого SH-2 свой 4 KiB кэш без bus snooping.
// Обмен возможен только через cache-through alias (addr | 0x20000000) либо
// после явного purge/invalidate. Протокол RPE — SPSC: Master пишет head,
// Slave пишет tail/completed; публикация — release, чтение — acquire.
//
// Заголовок host-совместим: SH-2/Jo Engine не протекают в C++ HAL.
#pragma once

#include <stdint.h>

namespace saturn {

/** Диагностические коды синхронизации. Порядок — контракт. */
enum ScuSyncDiag
{
    SCU_SYNC_OK = 0,
    SCU_SYNC_NULL_ARGUMENT,
    SCU_SYNC_TIMEOUT,
    SCU_SYNC_DISABLED,
    SCU_SYNC_COUNT
};

/**
 * Acquire-чтение 32-битной shared-ячейки.
 *
 * На SH-2 компилируется в обычный load (CPU in-order); compiler barrier
 * запрещает перестановку с последующими чтениями. Ячейка ДОЛЖНА быть
 * cache-through alias.
 */
uint32_t shared_load_u32(const volatile uint32_t* cell);

/**
 * Release-запись 32-битной shared-ячейки.
 *
 * Все предыдущие записи job-полей должны быть видны до публикации индекса;
 * на SH-2 порядок обеспечивает in-order CPU, барьер — компилятору.
 */
void shared_store_u32(volatile uint32_t* cell, uint32_t value);

/** Полный compiler barrier (на SH-2 дополнительных инструкций не требует). */
void shared_barrier(void);

/** Пауза busy-wait без обращений к внешней шине (деление SH-2). */
void cpu_relax(void);

/**
 * Ограниченное ожидание значения shared-ячейки.
 *
 * @param cell Cache-through ячейка (не nullptr).
 * @param value Ожидаемое значение.
 * @param timeout_ms Таймаут в миллисекундах (0 — одна проверка).
 * @param out_diag Диагностика (может быть nullptr).
 * @return true, если значение совпало до таймаута.
 */
bool wait_u32_until(const volatile uint32_t* cell, uint32_t value, uint32_t timeout_ms,
                    ScuSyncDiag* out_diag);

/** @return Cache-through alias для указателя (host — тот же указатель). */
void* shared_ptr(const void* local);

/** Purge/invalidate кэша текущего CPU. */
void cache_invalidate_local(void);

/**
 * @return Микросекунды Free Running Timer с момента start (wrap-safe).
 *
 * Интервал должен быть меньше 78 мс (16-битный FRT).
 */
uint32_t profile_elapsed_us(uint16_t start, uint16_t end);

/** @return Показания FRT текущего CPU. */
uint16_t profile_ticks(void);

/** @return Перевод FRT ticks в микросекунды. */
uint32_t profile_ticks_to_us(uint16_t ticks);

/** @return Статическая строка диагностики. */
const char* diag_string(ScuSyncDiag diag);

}  // namespace saturn
