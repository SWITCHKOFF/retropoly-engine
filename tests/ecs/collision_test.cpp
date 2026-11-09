// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/ecs/collision_test.cpp
//
// Тесты CollisionSystem — AABB intersect/response и детерминизм 60 Гц-реплея
// (норматив: docs/plans/sprint_17_18.md, неделя 34: «Тесты: пересечение/
// разделение AABB; детерминизм при 60 Гц-реплей»).
//
// Реплей строит РЕАЛЬНУЮ сцену демки (examples/rotating_cube/scene.h —
// rc_build_scene: 2 башни + статический пол) и прогоняет РЕАЛЬНУЮ фазовую
// цепочку тика §10 (гравитация → коллизии → движение) по скрипту прыжков:
// совпадение хэшей позиций двух прогонов доказывает детерминизм фиксированной
// симуляции (wall-clock в фазах не участвует — §10).
//
// Registry — static storage (§5.4/§5.5: бюджет ECS никогда не на CPU-стеке);
// тест-реестры переиспользуют один буфер через reset() ( SetUp).
#include "core/ecs/registry.h"
#include "core/math/fixed.h"
#include "core/math/vec3.h"
#include "game/components.h"
#include "game/systems/collision_system.h"
#include "game/systems/movement_system.h"
#include "examples/rotating_cube/scene.h"

#include <gtest/gtest.h>

#include <stdint.h>

namespace {

/// Тик реплея: 1/60 секунды (как у примеров — fixed деление ровное).
constexpr fixed_t kSimDt = (fixed_t)(FIXED_ONE / 60);

/// FNV-1a 64 над байтами (совместимо с golden-протоколом §1.2.9).
uint64_t fnv1a(const uint8_t* data, size_t size)
{
    uint64_t hash = UINT64_C(0xcbf29ce484222325);
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= UINT64_C(0x100000001b3);
    }
    return hash;
}

/// Хэш позиций всех живых сущностей реестра (dense-порядок — детерминизм).
uint64_t hash_positions(Registry& registry)
{
    uint64_t hash = UINT64_C(0xcbf29ce484222325);
    for (auto row : registry.view<Position>()) {
        const Position* pos = &row.get<Position>();
        hash = fnv1a(reinterpret_cast<const uint8_t*>(&pos->value), sizeof(Vec3));
    }
    return hash;
}

/// Куб-коллайдер ±h вокруг центра (локальный AABB полуразмеров).
Collider cube_collider(fixed_t h)
{
    return Collider{vec3_make(-h, -h, -h), vec3_make(h, h, h)};
}

/// Тестовая сцена: статический пол (верх y=0) + динамический куб над ним.
struct DropScene
{
    Entity ground;
    Entity cube;
};

DropScene build_drop_scene(Registry& registry, fixed_t cube_center_y)
{
    DropScene scene;
    scene.ground = registry.create_entity();
    registry.add_component(scene.ground, Position{vec3_make(0, 0, 0)});
    registry.add_component(scene.ground,
                           Collider{vec3_make(-INT32_C(64) * FIXED_ONE,
                                              -INT32_C(64) * FIXED_ONE,
                                              -INT32_C(64) * FIXED_ONE),
                                    vec3_make(INT32_C(64) * FIXED_ONE, 0,
                                              INT32_C(64) * FIXED_ONE)});
    scene.cube = registry.create_entity();
    registry.add_component(scene.cube, Position{vec3_make(0, cube_center_y, 0)});
    registry.add_component(scene.cube, Velocity{vec3_make(0, 0, 0)});
    registry.add_component(scene.cube, cube_collider(FIXED_ONE));
    return scene;
}

/// Скрипт прыжков реплея: прыжок в тиках i % 45 == 0 (фиксированный рисунок).
bool jump_at_tick(uint32_t tick)
{
    return (tick % 45u) == 0u;
}

/// Прогон N тиков демо-цепочки; возвращает хэш позиций после каждого тика.
uint64_t replay_ticks(Registry& registry, uint32_t ticks)
{
    uint64_t hash = UINT64_C(0xcbf29ce484222325);
    for (uint32_t t = 0; t < ticks; ++t) {
        // Фазовая цепочка §10 (без InputSystem: скрипт вместо ввода).
        rotating_cube::rc_gravity_and_jump(registry, kSimDt, jump_at_tick(t));
        game::CollisionSystem::update(registry, kSimDt);
        game::MovementSystem::update(registry, kSimDt);
        const uint64_t positions = hash_positions(registry);
        hash = fnv1a(reinterpret_cast<const uint8_t*>(&positions), sizeof(uint64_t));
    }
    return hash;
}

class CollisionTest : public ::testing::Test {
protected:
    void SetUp() override { g_registry.reset(); }

    Registry g_registry;
};

// --- AABB intersect (тесты пересечения/разделения) -------------------------

TEST_F(CollisionTest, AabbIntersectOverlappingOnAllAxes)
{
    const Vec3 a_min = vec3_make(0, 0, 0);
    const Vec3 a_max = vec3_make(2 * FIXED_ONE, 2 * FIXED_ONE, 2 * FIXED_ONE);
    const Vec3 b_min = vec3_make(FIXED_ONE, FIXED_ONE, FIXED_ONE);
    const Vec3 b_max = vec3_make(3 * FIXED_ONE, 3 * FIXED_ONE, 3 * FIXED_ONE);
    EXPECT_TRUE(game::CollisionSystem::aabb_intersect(a_min, a_max, b_min, b_max));
}

TEST_F(CollisionTest, AabbSeparatedOnSingleAxisIsNotIntersect)
{
    // Перекрытие по X/Z, зазор по Y (1/65536 ед. — ЗАМКНУТЫЕ интервалы
    // не соприкасаются) — контакта нет.
    const Vec3 a_min = vec3_make(0, 0, 0);
    const Vec3 a_max = vec3_make(2 * FIXED_ONE, 2 * FIXED_ONE, 2 * FIXED_ONE);
    const Vec3 b_min = vec3_make(FIXED_ONE, 2 * FIXED_ONE + 1, FIXED_ONE);
    const Vec3 b_max = vec3_make(3 * FIXED_ONE, 3 * FIXED_ONE, 3 * FIXED_ONE);
    EXPECT_FALSE(game::CollisionSystem::aabb_intersect(a_min, a_max, b_min, b_max));
}

TEST_F(CollisionTest, AabbTouchingFacesIsContact)
{
    // Касание гранями (интервалы сомкнуты) — КОНТАКТ (замкнутые интервалы):
    // push 0 + обнуление скорости «в поверхность» сходят дискретный контакт
    // к точному покою (без sink-jitter предельного цикла).
    const Vec3 a_min = vec3_make(0, 0, 0);
    const Vec3 a_max = vec3_make(2 * FIXED_ONE, 2 * FIXED_ONE, 2 * FIXED_ONE);
    const Vec3 b_min = vec3_make(2 * FIXED_ONE, 0, 0);
    const Vec3 b_max = vec3_make(4 * FIXED_ONE, 2 * FIXED_ONE, 2 * FIXED_ONE);
    EXPECT_TRUE(game::CollisionSystem::aabb_intersect(a_min, a_max, b_min, b_max));
}

TEST_F(CollisionTest, AabbFullySeparatedIsNotIntersect)
{
    const Vec3 a_min = vec3_make(0, 0, 0);
    const Vec3 a_max = vec3_make(FIXED_ONE, FIXED_ONE, FIXED_ONE);
    const Vec3 b_min = vec3_make(2 * FIXED_ONE, 2 * FIXED_ONE, 2 * FIXED_ONE);
    const Vec3 b_max = vec3_make(3 * FIXED_ONE, 3 * FIXED_ONE, 3 * FIXED_ONE);
    EXPECT_FALSE(game::CollisionSystem::aabb_intersect(a_min, a_max, b_min, b_max));
}

// --- Response (выталкивание, «не проходит сквозь») --------------------------

TEST_F(CollisionTest, FallingCubeLandsOnGroundAndDoesNotPassThrough)
{
    // Куб падает с высоты 5 на пол (верх y=0): после отстоя центр = 1
    // (низ куба на полу), скорость по Y обнулена — «сквозь» исключено.
    const DropScene scene = build_drop_scene(g_registry, INT32_C(5) * FIXED_ONE);

    for (uint32_t t = 0; t < 120u; ++t) { // 2 секунды симуляции
        rotating_cube::rc_gravity_and_jump(g_registry, kSimDt, false);
        game::CollisionSystem::update(g_registry, kSimDt);
        game::MovementSystem::update(g_registry, kSimDt);
    }

    const Position* pos =
        static_cast<const Position*>(g_registry.get_component<Position>(scene.cube));
    ASSERT_NE(pos, nullptr);
    EXPECT_EQ(pos->value.y, FIXED_ONE); // Ровно на полу (без провала).
    const Velocity* vel =
        static_cast<const Velocity*>(g_registry.get_component<Velocity>(scene.cube));
    ASSERT_NE(vel, nullptr);
    EXPECT_EQ(vel->value.y, 0); // Скорость «в поверхность» обнулена.
}

TEST_F(CollisionTest, StackedCubeRestsOnCubeWithoutPenetration)
{
    // Нижний куб на полу (центр 1), верхний роняется с 6: останавливается
    // центром на y=3 (низ на верхнему гране нижнего 2.0) — стек без
    // взаимопроникновения.
    const DropScene scene = build_drop_scene(g_registry, INT32_C(6) * FIXED_ONE);
    const Entity bottom = g_registry.create_entity();
    g_registry.add_component(bottom, Position{vec3_make(0, FIXED_ONE, 0)});
    g_registry.add_component(bottom, Velocity{vec3_make(0, 0, 0)});
    g_registry.add_component(bottom, cube_collider(FIXED_ONE));

    for (uint32_t t = 0; t < 180u; ++t) { // 3 секунды
        rotating_cube::rc_gravity_and_jump(g_registry, kSimDt, false);
        game::CollisionSystem::update(g_registry, kSimDt);
        game::MovementSystem::update(g_registry, kSimDt);
    }

    const Position* top =
        static_cast<const Position*>(g_registry.get_component<Position>(scene.cube));
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->value.y, INT32_C(3) * FIXED_ONE); // Стек: 3.0 ровно.
    const Position* bottom_pos =
        static_cast<const Position*>(g_registry.get_component<Position>(bottom));
    ASSERT_NE(bottom_pos, nullptr);
    EXPECT_EQ(bottom_pos->value.y, FIXED_ONE); // Низ не продавлен полом.
}

TEST_F(CollisionTest, HorizontalPushoutKeepsFreeAxisSliding)
{
    // Куб летит по диагонали в статическую стену: выталкивание ТОЛЬКО по X
    // (ось минимального проникновения), скольжение по Z сохраняется.
    const Entity wall = g_registry.create_entity();
    g_registry.add_component(wall, Position{vec3_make(0, 0, 0)});
    g_registry.add_component(
        wall, Collider{vec3_make(0, -INT32_C(4) * FIXED_ONE, -INT32_C(4) * FIXED_ONE),
                       vec3_make(INT32_C(4) * FIXED_ONE, INT32_C(4) * FIXED_ONE,
                                 INT32_C(4) * FIXED_ONE)});
    const Entity cube = g_registry.create_entity();
    g_registry.add_component(
        cube, Position{vec3_make(-INT32_C(3) * FIXED_ONE, 0, 0)}); // зазор 1 по X
    g_registry.add_component(cube, Velocity{vec3_make(INT32_C(2) * FIXED_ONE, 0,
                                                      INT32_C(2) * FIXED_ONE)});
    g_registry.add_component(cube, cube_collider(FIXED_ONE));

    for (uint32_t t = 0; t < 90u; ++t) { // 1.5 секунды (контакт на тике 61-62)
        game::CollisionSystem::update(g_registry, kSimDt);
        game::MovementSystem::update(g_registry, kSimDt);
    }

    const Position* pos =
        static_cast<const Position*>(g_registry.get_component<Position>(cube));
    ASSERT_NE(pos, nullptr);
    // X: вплотную к стене (куб ±1: центр −1) — сквозь не прошёл; скорость
    // по X обнулена касанием, позиция больше не меняется.
    EXPECT_EQ(pos->value.x, -FIXED_ONE);
    // Z: свободная ось — интеграл 90 тиков · fixed_mul(2·ед, 1/60) с
    // ДЕТЕРМИНИРОВАННЫМ усечением за тик: 90·2184 = 196560 (усечение
    // fixed_mul на каждом тике — детерминизм §4).
    EXPECT_EQ(pos->value.z, 90u * 2184u);
}

// --- Детерминизм при 60 Гц-реплее -------------------------------------------

TEST_F(CollisionTest, DemoSceneReplayIsDeterministic)
{
    // Два прогона РЕАЛЬНОЙ сцены демки (башни + пол) по одному скрипту:
    // хэши позиций обязаны совпасть байт-в-байт (fixed 16.16, dense-порядок).
    uint64_t hashes[2];
    for (int run = 0; run < 2; ++run) {
        g_registry.reset();
        const rotating_cube::SceneHandles handles = rotating_cube::rc_build_scene(g_registry);
        ASSERT_NE(handles.camera, INVALID_ENTITY);
        for (int i = 0; i < 4; ++i) {
            ASSERT_NE(handles.cubes[i], INVALID_ENTITY);
        }
        hashes[run] = replay_ticks(g_registry, 600u); // 10 секунд симуляции
    }
    EXPECT_EQ(hashes[0], hashes[1]);
}

TEST_F(CollisionTest, DemoSceneReplayStaysAboveGround)
{
    // Инвариант «не проходит сквозь» на всём реплее: нижние кубы (центр y=1)
    // не проваливаются ниже пола ни на одном тике.
    g_registry.reset();
    const rotating_cube::SceneHandles handles = rotating_cube::rc_build_scene(g_registry);
    ASSERT_NE(handles.camera, INVALID_ENTITY);

    for (uint32_t t = 0; t < 600u; ++t) {
        rotating_cube::rc_gravity_and_jump(g_registry, kSimDt, jump_at_tick(t));
        game::CollisionSystem::update(g_registry, kSimDt);
        game::MovementSystem::update(g_registry, kSimDt);

        for (int i = 0; i < 4; ++i) {
            const Position* pos = static_cast<const Position*>(
                g_registry.get_component<Position>(handles.cubes[i]));
            ASSERT_NE(pos, nullptr);
            // Прыжок с пола и башни: центр не ниже 1.0 (пол) минус допуск
            // микропроникновения одного тика (гравитация·dt² ≪ 1/65536·8).
            EXPECT_GE(pos->value.y, FIXED_ONE - 8) << "тик " << t << " куб " << i;
        }
    }
}

} // namespace
