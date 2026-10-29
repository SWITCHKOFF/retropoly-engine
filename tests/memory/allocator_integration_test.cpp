// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/memory/allocator_integration_test.cpp
//
// Интеграционный сценарий трёх аллокаторов при загрузке «уровня»
// (docs/plans/sprint_03_04.md, Неделя 7).
//
// ВАЖНО (нормативное правило sprint_03_04, Неделя 7): мегабайтные арены —
// только static/global или из memory subsystem, НИКОГДА на CPU-стеке.
// Буферы ниже объявлены в static storage (namespace scope), а не локально.
#include "core/memory/linear_allocator.h"
#include "core/memory/pool_allocator.h"
#include "core/memory/stack_allocator.h"
#include "core/math/fixed.h"
#include "core/math/vec3.h"

#include <gtest/gtest.h>

#include <stddef.h>
#include <stdint.h>

namespace
{

/** Уровневая арена: 448 KiB по бюджету §5.6 (static storage). */
alignas(64) uint8_t g_level_storage[448 * 1024];

/** Кадровая арена: 64 KiB (static storage). */
alignas(64) uint8_t g_frame_storage[64 * 1024];

struct LevelHeader
{
    uint32_t version;
    uint32_t mesh_count;
};

struct MeshVertex
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
};

struct Enemy
{
    Vec3 position;
    fixed_t health;
};

uintptr_t address_of(const void* pointer) { return reinterpret_cast<uintptr_t>(pointer); }

} // namespace

TEST(AllocatorsIntegration, LevelLoadScenario)
{
    LinearAllocator level(g_level_storage, sizeof(g_level_storage));
    StackAllocator frame(g_frame_storage, sizeof(g_frame_storage));
    Pool<Enemy, 64> enemies;

    // --- Уровень: заголовок + геометрия в linear arena. ---
    LevelHeader* header = static_cast<LevelHeader*>(level.allocate(sizeof(LevelHeader), 8));
    ASSERT_NE(header, nullptr);
    header->version = 3;
    header->mesh_count = 3;

    MeshVertex* vertices =
        static_cast<MeshVertex*>(level.allocate(3 * sizeof(MeshVertex), 16));
    ASSERT_NE(vertices, nullptr);
    vertices[0] = MeshVertex{fixed_from_int(0), fixed_from_int(0), fixed_from_int(0)};
    vertices[1] = MeshVertex{fixed_from_int(1), fixed_from_int(0), fixed_from_int(0)};
    vertices[2] = MeshVertex{fixed_from_int(0), fixed_from_int(1), fixed_from_int(0)};

    // Неудачная загрузка опционального ассета откатывается к checkpoint.
    const LinearAllocator::Marker checkpoint = level.mark();
    void* optional_asset = level.allocate(1024, 32);
    ASSERT_NE(optional_asset, nullptr);
    EXPECT_TRUE(level.rollback(checkpoint));
    EXPECT_EQ(level.used(), checkpoint.offset);

    void* texture = level.allocate(256, 32);
    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(address_of(texture) % 32u, 0u);

    EXPECT_GE(address_of(header), address_of(g_level_storage));
    EXPECT_LT(address_of(header), address_of(g_level_storage) + sizeof(g_level_storage));
    EXPECT_LE(address_of(vertices) + 3 * sizeof(MeshVertex), address_of(texture));

    // --- Динамические объекты: pool. ---
    Enemy* first = enemies.allocate();
    Enemy* second = enemies.allocate();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    first->position = vec3_make(fixed_from_int(1), fixed_from_int(2), fixed_from_int(3));
    first->health = fixed_from_int(100);
    second->position = vec3_make(fixed_from_int(4), fixed_from_int(5), fixed_from_int(6));
    second->health = fixed_from_int(50);

    EXPECT_TRUE(enemies.deallocate(first));
    Enemy* respawned = enemies.allocate();
    EXPECT_EQ(respawned, first);
    respawned->health = fixed_from_int(100);
    EXPECT_EQ(enemies.used(), 2u);

    size_t alive = 0;
    enemies.for_each([&](Enemy&) { ++alive; });
    EXPECT_EQ(alive, 2u);

    // --- Временные данные кадра: stack. ---
    const StackAllocator::Marker frame_mark = frame.mark();
    MeshVertex* temp_a = static_cast<MeshVertex*>(frame.push(sizeof(MeshVertex), 16));
    MeshVertex* temp_b = static_cast<MeshVertex*>(frame.push(sizeof(MeshVertex), 16));
    ASSERT_NE(temp_a, nullptr);
    ASSERT_NE(temp_b, nullptr);
    temp_a->x = fixed_from_int(7);
    temp_b->y = fixed_from_int(8);

    EXPECT_TRUE(frame.pop(frame_mark));
    EXPECT_EQ(frame.used(), 0u);

    // Арены независимы и не перекрываются (порядок в static storage не задан).
    const uintptr_t level_begin = address_of(g_level_storage);
    const uintptr_t level_end = level_begin + sizeof(g_level_storage);
    const uintptr_t frame_begin = address_of(g_frame_storage);
    const uintptr_t frame_end = frame_begin + sizeof(g_frame_storage);
    EXPECT_TRUE(level_end <= frame_begin || frame_end <= level_begin);
}
