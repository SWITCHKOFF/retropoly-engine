// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/systems/collision_system.h
//
// Система коллизий слоя игры (header-only): парное разрешение AABB —
// тест пересечения + response (выталкивание по оси минимального
// проникновения). Норматив: docs/plans/sprint_17_18.md (неделя 34, задача
// «CollisionSystem (AABB): intersect-тесты, response (выталкивание), не
// проходит сквозь»); docs/RetroEngine_Engineering_Documentation_v1.43.md
// §10 — CollisionSystem третий тик 60 Гц (цепочка Input → GameUpdate →
// Collision → Movement — фаза ДО интеграции позиций).
//
// СЕМАНТИКА Collider (норматив демо W1, сцена rotating_cube):
//   min/max — ЛОКАЛЬНЫЙ AABB относительно Position (полуразмеры для куба:
//   min = −h, max = +h). Мировые границы сущности = Position + min/max.
//   Статика = сущность с Collider БЕЗ Velocity (Position — любые мировые
//   координаты; для «куба в точке p» задаётся min/max = ±h и Position=p).
//
// КОНТРАКТЫ ДЕТЕРМИНИЗМА (60 Гц-реплей, §10):
//   - вся арифметика — fixed 16.16 (fixed_mul/fixed_add/sub, насыщающие);
//     сравнения — прямые int32-операции (fixed_t — int32, детерминизм
//     точного порядка бит);
//   - обход — dense-порядок пулов (пара (D,S) упорядочена позицией в
//     dense-массиве, не хэшем/указателем);
//   - ось выталкивания — МИНИМАЛЬНОЕ перекрытие; ничья разрешается
//     приоритетом X → Y → Z (первый минимум, строгие сравнения);
//   - response двигает ТОЛЬКО динамическую сущность D (со Velocity);
//     статика (без Velocity) неподвижна; D-vs-D — двигается первый в
//     dense-порядке (demo-level: без масс и импульсов — НЕ физика);
//   - скорость D по оси выталкивания обнуляется, только если направлена
//     В поверхность (проекция знака) — свободное скольжение вдоль стен
//     сохраняется, «проход сквозь» исключён.
#pragma once

#include "core/ecs/registry.h"
#include "core/math/fixed.h"
#include "core/math/vec3.h"

namespace game {

class CollisionSystem {
public:
    /** Фаза коллизий тика: парное AABB-разрешение (динамика против всех). */
    static void update(Registry& registry, fixed_t dt);

    /**
     * Тест контакта двух мировых AABB (ЗАМКНУТЫЕ интервалы по всем осям).
     * Касание гранями — ТОЖЕ контакт (push 0 + обнуление скорости «в
     * поверхность»): дискретный контакт 60 Гц сходится к точному покою
     * без sink-jitter цикла (строгие интервалы дали бы предельный цикл
     * «гравитация-тик → микропровал → выталкивание»).
     */
    static bool aabb_intersect(const Vec3& a_min, const Vec3& a_max, const Vec3& b_min,
                               const Vec3& b_max);
};

inline bool CollisionSystem::aabb_intersect(const Vec3& a_min, const Vec3& a_max,
                                            const Vec3& b_min, const Vec3& b_max)
{
    return a_max.x >= b_min.x && a_min.x <= b_max.x && a_max.y >= b_min.y &&
           a_min.y <= b_max.y && a_max.z >= b_min.z && a_min.z <= b_max.z;
}

inline void CollisionSystem::update(Registry& registry, fixed_t dt)
{
    (void)dt; // dt к контракту фазы ДО интеграции: система двигает позиции
              // только через response (дискрет из penetration, не ·dt)

    for (auto dyn_row : registry.view<Collider, Position, Velocity>()) {
        Position* dyn_pos = &dyn_row.get<Position>();
        const Collider* dyn_col = &dyn_row.get<Collider>();
        Velocity* dyn_vel = &dyn_row.get<Velocity>();

        // Мировой AABB динамики на текущий момент (пересчитывается после
        // каждого response — Gauss-Seidel по dense-порядку, детерминированно).
        Vec3 dyn_min = vec3_add(dyn_pos->value, dyn_col->min);
        Vec3 dyn_max = vec3_add(dyn_pos->value, dyn_col->max);

        for (auto other_row : registry.view<Collider, Position>()) {
            if (other_row.entity() == dyn_row.entity()) {
                continue; // Самопарная проверка не участвует.
            }
            const Position* oth_pos = &other_row.get<Position>();
            const Collider* oth_col = &other_row.get<Collider>();
            const Vec3 oth_min = vec3_add(oth_pos->value, oth_col->min);
            const Vec3 oth_max = vec3_add(oth_pos->value, oth_col->max);

            if (!aabb_intersect(dyn_min, dyn_max, oth_min, oth_max)) {
                continue;
            }

            // Перекрытия по осям (строго положительные после intersect-теста).
            const fixed_t overlap_x = fixed_sub(
                (dyn_max.x < oth_max.x) ? dyn_max.x : oth_max.x,
                (dyn_min.x > oth_min.x) ? dyn_min.x : oth_min.x);
            const fixed_t overlap_y = fixed_sub(
                (dyn_max.y < oth_max.y) ? dyn_max.y : oth_max.y,
                (dyn_min.y > oth_min.y) ? dyn_min.y : oth_min.y);
            const fixed_t overlap_z = fixed_sub(
                (dyn_max.z < oth_max.z) ? dyn_max.z : oth_max.z,
                (dyn_min.z > oth_min.z) ? dyn_min.z : oth_min.z);

            // Ось минимального проникновения; ничья → приоритет X → Y → Z.
            int axis = 0; // 0 = X, 1 = Y, 2 = Z
            fixed_t overlap = overlap_x;
            if (overlap_y < overlap) {
                axis = 1;
                overlap = overlap_y;
            }
            if (overlap_z < overlap) {
                axis = 2;
                overlap = overlap_z;
            }

            // Направление выталкивания: знак разности центров по оси
            // (при равенстве — «+», детерминированный tie-break).
            const fixed_t dyn_center = (axis == 0)   ? dyn_pos->value.x
                                       : (axis == 1) ? dyn_pos->value.y
                                                     : dyn_pos->value.z;
            const fixed_t oth_center = (axis == 0)   ? oth_pos->value.x
                                       : (axis == 1) ? oth_pos->value.y
                                                     : oth_pos->value.z;
            const int dir = (dyn_center < oth_center) ? -1 : 1;

            // Выталкивание: позиция D уходит из пересечения ровно на overlap.
            const fixed_t push = (dir < 0) ? fixed_sub(0, overlap) : overlap;
            fixed_t* dyn_axis_pos =
                (axis == 0)   ? &dyn_pos->value.x
                : (axis == 1) ? &dyn_pos->value.y
                              : &dyn_pos->value.z;
            *dyn_axis_pos = fixed_add(*dyn_axis_pos, push);

            // Скорость: обнулить ТОЛЬКО составляющую В поверхность.
            fixed_t* dyn_axis_vel =
                (axis == 0)   ? &dyn_vel->value.x
                : (axis == 1) ? &dyn_vel->value.y
                              : &dyn_vel->value.z;
            const bool moving_into_surface =
                (dir < 0) ? (*dyn_axis_vel > 0) : (*dyn_axis_vel < 0);
            if (moving_into_surface) {
                *dyn_axis_vel = 0;
            }

            // Обновить локальный AABB динамики (Gauss-Seidel: следующие пары
            // видят уже вытолкнутую позицию).
            dyn_min = vec3_add(dyn_pos->value, dyn_col->min);
            dyn_max = vec3_add(dyn_pos->value, dyn_col->max);
        }
    }
}

} // namespace game
