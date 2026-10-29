// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/ecs/entity.h
//
// Каноническая кодировка хэндла сущности (docs/plans/sprint_03_04.md,
// Неделя 8; engineering doc v1.43 §1.2.4 «ENTITY HANDLE CONTRACT», §6.1).
//
// Контракт (нормативно):
//   - Entity — uint32_t; 0 = INVALID_ENTITY;
//   - нижние 24 бита — index слота, верхние 8 бит — generation;
//   - generation защищает от stale-handle aliasing: после destroy слота
//     generation увеличивается, а при попытке wrap 255→0 слот навсегда
//     выходит из оборота (retirement);
//   - MAX_ENTITIES — домен хэндлов (10 000), а не ёмкость каждого
//     компонентного пула (см. бюджет ECS в §5.6).
#pragma once

#include <stddef.h>
#include <stdint.h>

/** Хэндл сущности: index (24 бита) | generation (8 бит). */
using Entity = uint32_t;

/** Число бит индекса слота в хэндле. */
constexpr uint32_t ENTITY_INDEX_BITS = 24;

/** Число бит generation в хэндле. */
constexpr uint32_t ENTITY_GENERATION_BITS = 8;

/** Маска индекса слота (нижние 24 бита). */
constexpr uint32_t ENTITY_INDEX_MASK = 0x00FFFFFFu;

/** Сдвиг generation в хэндле. */
constexpr uint32_t ENTITY_GENERATION_SHIFT = 24;

/** Маска generation (8 бит) до сдвига. */
constexpr uint32_t ENTITY_GENERATION_MASK = 0x000000FFu;

/** Невалидный хэндл; в частности, результат неудачного create_entity(). */
constexpr Entity INVALID_ENTITY = 0;

/** Домен хэндлов ECS (не ёмкость отдельных компонентных пулов). */
constexpr uint32_t MAX_ENTITIES = 10000;

static_assert(ENTITY_INDEX_BITS + ENTITY_GENERATION_BITS == 32,
    "Entity handle must use exactly 32 bits");
static_assert(MAX_ENTITIES <= (ENTITY_INDEX_MASK + 1u),
    "MAX_ENTITIES must fit into the 24-bit index domain");

/**
 * Формирование хэндла из индекса слота и generation.
 *
 * Индекс маскируется до 24 бит, generation — до 8 бит. Хэндл
 * entity_make(0, 0) равен INVALID_ENTITY; registry никогда не выдаёт
 * такой хэндл (слот 0 начинает жизнь с generation = 1).
 *
 * @param index Индекс слота (значащие 24 бита).
 * @param generation Поколение слота (значащие 8 бит).
 * @return Упакованный хэндл сущности.
 */
static inline Entity entity_make(uint32_t index, uint32_t generation)
{
    return (index & ENTITY_INDEX_MASK) |
           ((generation & ENTITY_GENERATION_MASK) << ENTITY_GENERATION_SHIFT);
}

/**
 * Извлечение индекса слота из хэндла.
 *
 * @param entity Хэндл сущности.
 * @return Нижние 24 бита хэндла (индекс слота).
 */
static inline uint32_t entity_index(Entity entity) { return entity & ENTITY_INDEX_MASK; }

/**
 * Извлечение generation из хэндла.
 *
 * @param entity Хэндл сущности.
 * @return Верхние 8 бит хэндла (generation слота).
 */
static inline uint32_t entity_generation(Entity entity)
{
    return (entity >> ENTITY_GENERATION_SHIFT) & ENTITY_GENERATION_MASK;
}
