// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/memory/memory_profiler.h
//
// Профилировщик памяти MVP (Спринт 15–16): учёт total/used RAM+VRAM
// по зарегистрированным источникам (аллокаторы, статические блоки,
// VRAM-бэкенд). Норматив: docs/plans/sprint_15_16.md, неделя 32.
//
// Контракты:
//   - источник = {имя, тип(RAM/VRAM), capacity, callback used(user)};
//   - static-источники регистрируются через register_static (used == capacity);
//   - динамические источники (арены) передают callback, возвращающий
//     актуальное потребление;
//   - бюджеты RAM/VRAM задаются независимо от суммы capacity (PS1: RAM 1.5 MiB,
//     VRAM 800 KiB);
//   - print_stats печатает per-source + total; check_thresholds возвращает false
//     и логирует ERROR при превышении порога (>90% по умолчанию).
#pragma once

#include <stddef.h>
#include <stdint.h>

/** Тип источника памяти. */
typedef enum MemoryProfilerSourceType
{
    MEMORY_PROFILER_RAM = 0,
    MEMORY_PROFILER_VRAM = 1
} MemoryProfilerSourceType;

/** Callback: текущее потребление источника в байтах. */
typedef size_t (*MemoryProfilerUsedFn)(void* user);

/**
 * Сброс всех источников и бюджетов (используется при старте сессии).
 */
void memory_profiler_reset(void);

/**
 * Установка бюджета типа памяти (байт).
 *
 * @param type RAM или VRAM.
 * @param budget_bytes Бюджет; 0 — не проверять total threshold.
 */
void memory_profiler_set_budget(MemoryProfilerSourceType type, size_t budget_bytes);

/**
 * Регистрация динамического источника.
 *
 * @param name Имя источника (статическая строка).
 * @param type RAM/VRAM.
 * @param capacity Ёмкость источника в байтах.
 * @param used_fn Callback текущего использования; NULL трактуется как 0.
 * @param user Пользовательский указатель для callback.
 */
void memory_profiler_register_source(const char* name, MemoryProfilerSourceType type,
                                     size_t capacity, MemoryProfilerUsedFn used_fn, void* user);

/**
 * Регистрация статического блока (used == capacity).
 *
 * @param name Имя источника.
 * @param type RAM/VRAM.
 * @param size Размер блока в байтах.
 */
void memory_profiler_register_static(const char* name, MemoryProfilerSourceType type, size_t size);

/**
 * Печать статистики: per-source used/capacity/percent + total used/budget/percent.
 */
void memory_profiler_print_stats(void);

/**
 * Проверка порогов заполнения.
 *
 * Для каждого источника: used*100 > capacity*percent => ERROR.
 * Для каждого типа: total_used*100 > budget*percent => ERROR.
 *
 * @param ram_percent_threshold Порог RAM (например, 90).
 * @param vram_percent_threshold Порог VRAM (например, 90).
 * @return true, если ни один порог не превышен; false при алерте.
 */
bool memory_profiler_check_thresholds(uint32_t ram_percent_threshold,
                                      uint32_t vram_percent_threshold);
