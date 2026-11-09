// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/systems/collision_system.h
//
// Система коллизий слоя игры (header-only). Норматив: docs/plans/
// sprint_11_12.md, неделя 23; docs/RetroEngine_Engineering_Documentation_
// v1.43.md §10 — CollisionSystem третий тик 60 Гц (цепочка Input →
// GameUpdate → Collision → Movement — фаза ДО интеграции позиций).
//
// MVP-статус: парное разрешение AABB — Спринт 13–14 (физика). Сегодня
// система фиксирует ДЕТЕРМИНИРОВАННУЮ кадровую стоимость фазы (обход пула
// Collider) и является no-op по состоянию для сцен без парных пересечений
// (hello_triangle: 0 сущностей с Collider). Включение в pipeline уже сейчас
// держит цикл примера конформным §10 без перестройки графика фаз позднее.
#pragma once

#include "core/ecs/registry.h"
#include "core/math/fixed.h"

namespace game {

class CollisionSystem {
public:
    /** Фаза коллизий тика: обход пула Collider (rd-only; разрешение — S13–14). */
    static void update(Registry& registry, fixed_t dt);
};

inline void CollisionSystem::update(Registry& registry, fixed_t dt)
{
    (void)dt; // dt к контракту фазы ДО интеграции (не меняет позиций в MVP)

    for (auto row : registry.view<Collider, Position>()) {
        // MVP: коллайдеры сцены примера неизменны; парное разрешение — S13–14.
        (void)row;
    }
}

} // namespace game
