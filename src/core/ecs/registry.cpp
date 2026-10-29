// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/ecs/registry.cpp
//
// Реализация ECS Registry: жизненный цикл сущностей и бюджет памяти.
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §5.6, §6.2;
//   - docs/plans/sprint_03_04.md, Неделя 8.
#include "core/ecs/registry.h"

static_assert(MAX_ENTITIES <= 65535u, "MAX_ENTITIES must fit uint16_t free-list index");
static_assert(sizeof(Registry) <= 256u * 1024u,
    "ECS Registry exceeds the normative 256 KiB ECS/dynamic-pool budget");

Registry::Registry() : m_free_head(0xFFFFu), m_alive_count(0) { reset(); }

void Registry::reset()
{
    m_positions.clear();
    m_rotations.clear();
    m_velocities.clear();
    m_mesh_renderers.clear();
    m_cameras.clear();
    m_player_controls.clear();
    m_colliders.clear();

    for (uint32_t i = 0; i < MAX_ENTITIES; ++i)
    {
        m_slots[i].generation = 0;
        m_slots[i].alive = false;
        m_free_next[i] = (i + 1 < MAX_ENTITIES) ? static_cast<uint16_t>(i + 1) : 0xFFFFu;
    }

    // Слот 0 начинает жизнь с generation = 1, чтобы handle entity_make(0, 0)
    // оставался зарезервированным INVALID_ENTITY на всём домене.
    m_slots[0].generation = 1;
    m_free_head = 0;
    m_alive_count = 0;
}

Entity Registry::create_entity()
{
    if (m_free_head == 0xFFFFu || m_free_head >= MAX_ENTITIES)
        return INVALID_ENTITY;

    const uint16_t slot = m_free_head;
    m_free_head = m_free_next[slot];
    m_slots[slot].alive = true;
    ++m_alive_count;
    return entity_make(slot, m_slots[slot].generation);
}

bool Registry::destroy_entity(Entity entity)
{
    if (!is_alive(entity))
        return false;

    const uint32_t slot = entity_index(entity);
    remove_all_components(entity);

    m_slots[slot].alive = false;

    // Generation 255 — последняя валидная; после destroy слот выводится из
    // оборота навсегда, чтобы 255→0 не дал stale-handle aliasing.
    const bool retire = (m_slots[slot].generation == 255);
    if (!retire)
    {
        ++m_slots[slot].generation;
        m_free_next[slot] = m_free_head;
        m_free_head = static_cast<uint16_t>(slot);
    }

    --m_alive_count;
    return true;
}

bool Registry::is_alive(Entity entity) const
{
    if (entity == INVALID_ENTITY)
        return false;

    const uint32_t slot = entity_index(entity);
    if (slot >= MAX_ENTITIES)
        return false;

    return m_slots[slot].alive && m_slots[slot].generation == entity_generation(entity);
}

void Registry::remove_all_components(Entity entity)
{
    m_positions.remove(entity);
    m_rotations.remove(entity);
    m_velocities.remove(entity);
    m_mesh_renderers.remove(entity);
    m_cameras.remove(entity);
    m_player_controls.remove(entity);
    m_colliders.remove(entity);
}
