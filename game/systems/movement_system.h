// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/systems/movement_system.h
//
// Система движения слоя игры (header-only): интеграция Position += Velocity·dt
// тика фикс-симуляции. Норматив: docs/plans/sprint_11_12.md, неделя 23;
// docs/RetroEngine_Engineering_Documentation_v1.43.md §10 — MovementSystem
// ПОСЛЕДНИЙ тик 60 Гц (фазовая цепочка Input → GameUpdate → Collision →
// Movement), интеграция строго по SIM_DT (wall-clock до геймплея не пробегает).
//
// Контракты:
//   - интеграция покомпонентная через fixed_mul (насыщение 16.16 —
//     детерминированно на всех платформах);
//   - Velocity и Position — независимые компоненты (пересечение view).
#pragma once

#include "core/ecs/registry.h"
#include "core/math/fixed.h"
#include "core/math/vec3.h"

namespace game {

class MovementSystem {
public:
    /** Интеграция позиций всех сущностей с парой Position+Velocity. */
    static void update(Registry& registry, fixed_t dt);
};

inline void MovementSystem::update(Registry& registry, fixed_t dt)
{
    for (auto row : registry.view<Position, Velocity>()) {
        Position* position = &row.get<Position>();
        const Velocity* velocity = &row.get<Velocity>();

        position->value.x = fixed_add(position->value.x, fixed_mul(velocity->value.x, dt));
        position->value.y = fixed_add(position->value.y, fixed_mul(velocity->value.y, dt));
        position->value.z = fixed_add(position->value.z, fixed_mul(velocity->value.z, dt));
    }
}

} // namespace game
