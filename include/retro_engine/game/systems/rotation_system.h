// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/systems/rotation_system.h
//
// Система вращения слоя игры (header-only): приращение yaw на ФИКСИРОВАННУЮ
// скорость 90°/сек по длине тика фикс-симуляции. Норматив:
// docs/plans/sprint_17_18.md (неделя 33, задача «RotationSystem (fixed
// 90°/сек yaw)»); docs/RetroEngine_Engineering_Documentation_v1.43.md §10 —
// фаза GameUpdate тика 60 Гц (Input → GameUpdate → Collision → Movement).
//
// Контракты:
//   - цель — КАЖДАЯ сущность с парой MeshRenderer+Rotation (визуальное
//     вращение мешей). Камера не имеет MeshRenderer и не затрагивается;
//   - угол накапливается в Rotation::yaw (fixed 16.16, радианы) через
//     fixed_mul/fixed_add — детерминированный accumulate на всех
//     платформах (без float и без FMA-джиттера);
//   - 90°/сек = π/2 радиан/сек (fixed 16.16: 102943 — усечение от
//     102943.71, нормативное правило константных переводов).
#pragma once

#include <retro_engine/core/ecs/registry.h>
#include <retro_engine/core/math/fixed.h>

namespace game {

class RotationSystem {
public:
    /** Скорость вращения по yaw: 90°/сек = π/2 (16.16, радиан/секунду). */
    static constexpr fixed_t kYawSpeed = INT32_C(102943);

    /** Приращение yaw всех сущностей MeshRenderer+Rotation по длине тика. */
    static void update(Registry& registry, fixed_t dt);
};

inline void RotationSystem::update(Registry& registry, fixed_t dt)
{
    const fixed_t delta = fixed_mul(kYawSpeed, dt);

    for (auto row : registry.view<MeshRenderer, Rotation>()) {
        Rotation* rotation = &row.get<Rotation>();
        // Обратимость диапазона: fixed_add — насыщающая арифметика 16.16,
        // переполнение клампится (детерминизм §4 — без UB и без
        // wrap-артефактов). Период 2π не нормализуется ЗДЕСЬ: sin_i64
        // выполняет детерминированную целочисленную редукцию аргумента
        // mod 2π (core/math/math.cpp) — накопление угла свободное.
        rotation->yaw = fixed_add(rotation->yaw, delta);
    }
}

} // namespace game
