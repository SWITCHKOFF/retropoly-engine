// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/ecs/registry_test.cpp
//
// Юнит-тесты ECS Registry/ComponentPool/View (docs/plans/sprint_03_04.md,
// Неделя 8; engineering doc v1.43 §5.6, §6).
//
// Registry (~234 KiB) — static storage, не на CPU-стеке (§5.5).
#include "core/ecs/registry.h"
#include "core/math/fixed.h"
#include "core/math/vec3.h"

#include <gtest/gtest.h>

#include <stdint.h>

#include <set>
#include <vector>

namespace
{

Registry g_registry;

Position position_at(int x, int y, int z)
{
    return Position{vec3_make(fixed_from_int(x), fixed_from_int(y), fixed_from_int(z))};
}

Velocity velocity_at(int x, int y, int z)
{
    return Velocity{vec3_make(fixed_from_int(x), fixed_from_int(y), fixed_from_int(z))};
}

} // namespace

class RegistryTest : public ::testing::Test
{
protected:
    void SetUp() override { g_registry.reset(); }
};

TEST_F(RegistryTest, CreateEntityIsUniqueAndNonZero)
{
    std::set<Entity> handles;

    for (uint32_t i = 0; i < 1000; ++i)
    {
        const Entity entity = g_registry.create_entity();
        ASSERT_NE(entity, INVALID_ENTITY);
        EXPECT_TRUE(handles.insert(entity).second);
        EXPECT_TRUE(g_registry.is_alive(entity));
    }

    EXPECT_EQ(g_registry.alive_count(), 1000u);
}

TEST_F(RegistryTest, DestroyEntityRemovesComponentsAndHandle)
{
    const Entity entity = g_registry.create_entity();
    ASSERT_NE(entity, INVALID_ENTITY);
    ASSERT_TRUE(g_registry.add_component(entity, position_at(1, 2, 3)));

    EXPECT_TRUE(g_registry.destroy_entity(entity));
    EXPECT_FALSE(g_registry.is_alive(entity));
    EXPECT_FALSE(g_registry.has_component<Position>(entity));
    EXPECT_EQ(g_registry.get_component<Position>(entity), nullptr);
    EXPECT_FALSE(g_registry.destroy_entity(entity));
    EXPECT_FALSE(g_registry.add_component(entity, position_at(4, 5, 6)));
    EXPECT_FALSE(g_registry.remove_component<Position>(entity));
    EXPECT_EQ(g_registry.alive_count(), 0u);
}

TEST_F(RegistryTest, AddRemoveGetRoundTrip)
{
    const Entity entity = g_registry.create_entity();
    ASSERT_NE(entity, INVALID_ENTITY);

    EXPECT_FALSE(g_registry.has_component<Position>(entity));
    ASSERT_TRUE(g_registry.add_component(entity, position_at(10, 20, 30)));

    Position* position = g_registry.get_component<Position>(entity);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(position->value.x, fixed_from_int(10));
    EXPECT_EQ(position->value.y, fixed_from_int(20));
    EXPECT_EQ(position->value.z, fixed_from_int(30));

    ASSERT_TRUE(g_registry.add_component(entity, position_at(-1, -2, -3)));
    position = g_registry.get_component<Position>(entity);
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(position->value.x, fixed_from_int(-1));

    EXPECT_TRUE(g_registry.remove_component<Position>(entity));
    EXPECT_FALSE(g_registry.has_component<Position>(entity));
    EXPECT_EQ(g_registry.get_component<Position>(entity), nullptr);
    EXPECT_FALSE(g_registry.remove_component<Position>(entity));

    EXPECT_TRUE(g_registry.add_component(entity, position_at(7, 8, 9)));
    EXPECT_TRUE(g_registry.has_component<Position>(entity));

    const Registry& const_registry = g_registry;
    const Position* const_position = const_registry.get_component<Position>(entity);
    ASSERT_NE(const_position, nullptr);
    EXPECT_EQ(const_position->value.z, fixed_from_int(9));
}

TEST_F(RegistryTest, ViewSingleComponent)
{
    std::vector<Entity> entities;
    for (int i = 0; i < 10; ++i)
    {
        const Entity entity = g_registry.create_entity();
        ASSERT_NE(entity, INVALID_ENTITY);
        entities.push_back(entity);
        if (i < 6)
        {
            ASSERT_TRUE(g_registry.add_component(entity, position_at(i, 0, 0)));
        }
    }

    View<Position> view = g_registry.view<Position>();
    EXPECT_EQ(view.count(), 6u);

    size_t visited = 0;
    fixed_t sum = 0;
    for (View<Position>::Iterator it = view.begin(); it != view.end(); ++it)
    {
        ++visited;
        sum = fixed_add(sum, it.get<Position>().value.x);
        EXPECT_TRUE(g_registry.has_component<Position>(it.entity()));
    }
    EXPECT_EQ(visited, 6u);
    EXPECT_EQ(sum, fixed_from_int(0 + 1 + 2 + 3 + 4 + 5));

    size_t range_count = 0;
    for (auto row : view)
    {
        ++range_count;
        EXPECT_TRUE(g_registry.has_component<Position>(row.entity()));
    }
    EXPECT_EQ(range_count, 6u);
}

TEST_F(RegistryTest, ViewTwoComponentsUsesIntersection)
{
    std::vector<Entity> entities;
    for (int i = 0; i < 10; ++i)
    {
        const Entity entity = g_registry.create_entity();
        ASSERT_NE(entity, INVALID_ENTITY);
        entities.push_back(entity);

        ASSERT_TRUE(g_registry.add_component(entity, position_at(i, i, i)));
        if (i % 3 == 0)
        {
            ASSERT_TRUE(g_registry.add_component(entity, velocity_at(1, 2, 3)));
        }
    }

    View<Position, Velocity> view = g_registry.view<Position, Velocity>();
    EXPECT_EQ(view.count(), 4u); // 0, 3, 6, 9

    size_t visited = 0;
    for (View<Position, Velocity>::Iterator it = view.begin(); it != view.end(); ++it)
    {
        ++visited;
        EXPECT_TRUE(g_registry.has_component<Position>(it.entity()));
        EXPECT_TRUE(g_registry.has_component<Velocity>(it.entity()));
        EXPECT_EQ(it.get<Velocity>().value.x, fixed_from_int(1));

        // Прямая модификация dense-строки через View.
        it.get<Velocity>().value.x = fixed_add(it.get<Velocity>().value.x, FIXED_ONE);
    }
    EXPECT_EQ(visited, 4u);

    for (size_t i = 0; i < entities.size(); ++i)
    {
        const Velocity* velocity = g_registry.get_component<Velocity>(entities[i]);
        if (i % 3 == 0)
        {
            ASSERT_NE(velocity, nullptr);
            EXPECT_EQ(velocity->value.x, fixed_from_int(2));
        }
        else
        {
            EXPECT_EQ(velocity, nullptr);
        }
    }
}

TEST_F(RegistryTest, ViewWithModificationMovementSystem)
{
    struct MovementFixture
    {
        Entity entity;
        int start_x;
        int velocity_x;
    };

    const MovementFixture fixtures[] = {
        {g_registry.create_entity(), 10, 1},
        {g_registry.create_entity(), 20, 3},
        {g_registry.create_entity(), 30, 5},
    };

    for (const MovementFixture& fixture : fixtures)
    {
        ASSERT_NE(fixture.entity, INVALID_ENTITY);
        ASSERT_TRUE(g_registry.add_component(fixture.entity, position_at(fixture.start_x, 0, 0)));
        ASSERT_TRUE(
            g_registry.add_component(fixture.entity, velocity_at(fixture.velocity_x, 0, 0)));
    }

    const fixed_t delta_time = FIXED_ONE;
    View<Position, Velocity> view = g_registry.view<Position, Velocity>();
    for (View<Position, Velocity>::Iterator it = view.begin(); it != view.end(); ++it)
    {
        Position& position = it.get<Position>();
        const Velocity& velocity = it.get<Velocity>();
        position.value.x = fixed_add(position.value.x, fixed_mul(velocity.value.x, delta_time));
        position.value.y = fixed_add(position.value.y, fixed_mul(velocity.value.y, delta_time));
        position.value.z = fixed_add(position.value.z, fixed_mul(velocity.value.z, delta_time));
    }

    for (const MovementFixture& fixture : fixtures)
    {
        const Position* position = g_registry.get_component<Position>(fixture.entity);
        ASSERT_NE(position, nullptr);
        EXPECT_EQ(position->value.x, fixed_from_int(fixture.start_x + fixture.velocity_x));
    }
}

TEST_F(RegistryTest, ViewDenseOrderAfterSwapBackRemoval)
{
    const Entity e0 = g_registry.create_entity();
    const Entity e1 = g_registry.create_entity();
    const Entity e2 = g_registry.create_entity();
    const Entity e3 = g_registry.create_entity();
    ASSERT_NE(e3, INVALID_ENTITY);

    ASSERT_TRUE(g_registry.add_component(e0, position_at(0, 0, 0)));
    ASSERT_TRUE(g_registry.add_component(e1, position_at(1, 1, 1)));
    ASSERT_TRUE(g_registry.add_component(e2, position_at(2, 2, 2)));
    ASSERT_TRUE(g_registry.add_component(e3, position_at(3, 3, 3)));
    ASSERT_TRUE(g_registry.remove_component<Position>(e1));

    std::vector<Entity> order;
    View<Position> view = g_registry.view<Position>();
    for (View<Position>::Iterator it = view.begin(); it != view.end(); ++it)
        order.push_back(it.entity());

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], e0);
    EXPECT_EQ(order[1], e3); // swap-back: последняя dense-строка встала на место e1
    EXPECT_EQ(order[2], e2);
}

TEST_F(RegistryTest, StaleHandleRejected)
{
    const Entity first = g_registry.create_entity();
    ASSERT_NE(first, INVALID_ENTITY);
    const uint32_t slot = entity_index(first);
    const uint32_t generation = entity_generation(first);
    ASSERT_TRUE(g_registry.add_component(first, position_at(1, 2, 3)));

    ASSERT_TRUE(g_registry.destroy_entity(first));
    const Entity second = g_registry.create_entity();
    ASSERT_NE(second, INVALID_ENTITY);

    EXPECT_EQ(entity_index(second), slot);
    EXPECT_EQ(entity_generation(second), generation + 1u);
    EXPECT_NE(first, second);

    EXPECT_FALSE(g_registry.is_alive(first));
    EXPECT_FALSE(g_registry.has_component<Position>(first));
    EXPECT_EQ(g_registry.get_component<Position>(first), nullptr);
    EXPECT_FALSE(g_registry.add_component(first, position_at(9, 9, 9)));
    EXPECT_FALSE(g_registry.remove_component<Position>(first));
    EXPECT_FALSE(g_registry.destroy_entity(first));

    EXPECT_TRUE(g_registry.is_alive(second));
    EXPECT_FALSE(g_registry.has_component<Position>(second));
}

TEST_F(RegistryTest, GenerationIncrementOnReuse)
{
    const Entity first = g_registry.create_entity();
    ASSERT_NE(first, INVALID_ENTITY);
    const uint32_t generation = entity_generation(first);
    ASSERT_TRUE(g_registry.destroy_entity(first));

    const Entity second = g_registry.create_entity();
    ASSERT_NE(second, INVALID_ENTITY);
    EXPECT_EQ(entity_index(second), entity_index(first));
    EXPECT_EQ(entity_generation(second), generation + 1u);
}

TEST_F(RegistryTest, GenerationWrapRetirement)
{
    Entity current = g_registry.create_entity();
    ASSERT_NE(current, INVALID_ENTITY);
    const uint32_t retired_slot = entity_index(current);

    // 255 циклов: generation растёт 1,2,...,255; на destroy при 255 слот
    // выводится из оборота, и следующий create обязан взять другой слот.
    for (uint32_t cycle = 0; cycle < 255; ++cycle)
    {
        EXPECT_EQ(entity_index(current), retired_slot);
        EXPECT_EQ(entity_generation(current), cycle + 1u);
        ASSERT_TRUE(g_registry.destroy_entity(current));
        current = g_registry.create_entity();
        ASSERT_NE(current, INVALID_ENTITY);
    }

    EXPECT_NE(entity_index(current), retired_slot);
    EXPECT_TRUE(g_registry.is_alive(current));
}

TEST_F(RegistryTest, DestroyRemovesAllComponentTypes)
{
    const Entity entity = g_registry.create_entity();
    ASSERT_NE(entity, INVALID_ENTITY);

    ASSERT_TRUE(g_registry.add_component(entity, position_at(1, 2, 3)));
    ASSERT_TRUE(g_registry.add_component(entity, Rotation{0, 0, 0}));
    ASSERT_TRUE(g_registry.add_component(entity, velocity_at(4, 5, 6)));
    ASSERT_TRUE(g_registry.add_component(entity, MeshRenderer{1, 2}));
    ASSERT_TRUE(g_registry.add_component(entity, Camera{0, 0, 0}));
    ASSERT_TRUE(g_registry.add_component(entity, PlayerControl{0, 0}));
    ASSERT_TRUE(g_registry.add_component(entity, Collider{vec3_make(0, 0, 0), vec3_make(1, 1, 1)}));

    EXPECT_TRUE(g_registry.has_component<Position>(entity));
    EXPECT_TRUE(g_registry.has_component<Rotation>(entity));
    EXPECT_TRUE(g_registry.has_component<Velocity>(entity));
    EXPECT_TRUE(g_registry.has_component<MeshRenderer>(entity));
    EXPECT_TRUE(g_registry.has_component<Camera>(entity));
    EXPECT_TRUE(g_registry.has_component<PlayerControl>(entity));
    EXPECT_TRUE(g_registry.has_component<Collider>(entity));

    ASSERT_TRUE(g_registry.destroy_entity(entity));

    EXPECT_FALSE(g_registry.has_component<Position>(entity));
    EXPECT_FALSE(g_registry.has_component<Rotation>(entity));
    EXPECT_FALSE(g_registry.has_component<Velocity>(entity));
    EXPECT_FALSE(g_registry.has_component<MeshRenderer>(entity));
    EXPECT_FALSE(g_registry.has_component<Camera>(entity));
    EXPECT_FALSE(g_registry.has_component<PlayerControl>(entity));
    EXPECT_FALSE(g_registry.has_component<Collider>(entity));
}

TEST_F(RegistryTest, ComponentPoolCapacityIsEnforced)
{
    std::vector<Entity> entities;
    for (uint32_t i = 0; i < ComponentCapacity<Position>::VALUE + 1u; ++i)
    {
        const Entity entity = g_registry.create_entity();
        ASSERT_NE(entity, INVALID_ENTITY);
        entities.push_back(entity);
    }

    for (size_t i = 0; i < ComponentCapacity<Position>::VALUE; ++i)
        ASSERT_TRUE(g_registry.add_component(entities[i], position_at(static_cast<int>(i), 0, 0)));

    EXPECT_FALSE(g_registry.add_component(entities.back(), position_at(1, 1, 1)));
    EXPECT_EQ(g_registry.view<Position>().count(), ComponentCapacity<Position>::VALUE);

    ASSERT_TRUE(g_registry.remove_component<Position>(entities[0]));
    EXPECT_TRUE(g_registry.add_component(entities.back(), position_at(1, 1, 1)));
}

TEST_F(RegistryTest, CapacityIsMaxEntities)
{
    for (uint32_t i = 0; i < MAX_ENTITIES; ++i)
    {
        const Entity entity = g_registry.create_entity();
        ASSERT_NE(entity, INVALID_ENTITY) << "i = " << i;
    }

    EXPECT_EQ(g_registry.alive_count(), MAX_ENTITIES);
    EXPECT_EQ(g_registry.create_entity(), INVALID_ENTITY);
}

TEST_F(RegistryTest, TenThousandEntitiesDenseIteration)
{
    std::vector<Entity> entities;
    entities.reserve(MAX_ENTITIES);

    for (uint32_t i = 0; i < MAX_ENTITIES; ++i)
    {
        const Entity entity = g_registry.create_entity();
        ASSERT_NE(entity, INVALID_ENTITY);
        entities.push_back(entity);
    }

    const size_t component_count = ComponentCapacity<Position>::VALUE;
    for (size_t i = 0; i < component_count; ++i)
    {
        ASSERT_TRUE(g_registry.add_component(entities[i], position_at(static_cast<int>(i), 0, 0)));
        ASSERT_TRUE(g_registry.add_component(entities[i], velocity_at(1, 0, 0)));
    }

    View<Position, Velocity> view = g_registry.view<Position, Velocity>();
    EXPECT_EQ(view.count(), component_count);

    size_t visited = 0;
    for (View<Position, Velocity>::Iterator it = view.begin(); it != view.end(); ++it)
    {
        ++visited;
        it.get<Position>().value.y =
            fixed_add(it.get<Position>().value.y, it.get<Velocity>().value.x);
    }

    EXPECT_EQ(visited, component_count);
    EXPECT_EQ(g_registry.alive_count(), MAX_ENTITIES);

    const Position* last = g_registry.get_component<Position>(entities[component_count - 1u]);
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->value.y, fixed_from_int(1));
}
