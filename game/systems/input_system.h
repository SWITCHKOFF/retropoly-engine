// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/systems/input_system.h
//
// Система ввода слоя игры (header-only): клавиатура WASD/стрелки/Space/
// Shift, мышь-взгляд (yaw/pitch по consume-дельтам), ESC-выход.
// Норматив: docs/plans/sprint_11_12.md, неделя 23; §10 — InputSystem тик
// ПЕРВЫЙ внутри тика фикс-симуляции (60 Гц), мышь аккумулируется платформой
// между тиками (Input_GetMouseDelta — consume ровно один раз за тик).
//
// Контракты:
//   - игрок = сущность с PlayerControl+Camera+Position+Rotation+Velocity:
//     система пишет ЦЕЛЕВУЮ скорость текущего тика (units/секунду) в Velocity;
//     интеграция позиции — MovementSystem (разделение фаз §10);
//   - масштаб скорости от dt НЕ зависит: move_speed — units/секунду,
//     MovementSystem умножает на длину тика;
//   - направление диагонали нормируется (равная скорость WASD-A и W);
//   - мышь двигает yaw/pitch ТОЛЬКО в относительном режиме (платформа копит
//     дельты только в нём; в bench-режиме дельты всегда 0);
//   - @return true — запрошен выход (ESC зажат).
#pragma once

#include "core/ecs/registry.h"
#include "core/math/fixed.h"
#include "core/math/vec3.h"
#include "game/systems/camera_math.h"
#include "hal/platform.h"

namespace game {

/** Абсолютный кламп pitch: π/2 − 1/32 (≈ 88.2°) исключает вырождение
 *  look_at (forward ∥ up делило бы на ноль — контракт mat4_look_at). */
constexpr fixed_t CAMERA_PITCH_LIMIT = FIXED_HALF_PI - (FIXED_ONE / 32);

class InputSystem {
public:
    /**
     * Тик ввода 60 Гц: мышь → yaw/pitch игрока; клавиши → Velocity.
     *
     * @param registry ECS-реестр сцены.
     * @param dt Дельта тика (FIXED_ONE/60); масштаб скорости от dt не
     *        зависит (см. контракты) — параметр под единый сигнатурный
     *        контракт систем тика.
     * @return true — ESC зажат в этом тике (игрок просит выход).
     */
    static bool update(Registry& registry, fixed_t dt);
};

inline bool InputSystem::update(Registry& registry, fixed_t dt)
{
    (void)dt; // параметр под сигнатурный контракт тиков (см. doc выше)

    // Смещение мыши за межтикье: consume ровно раз за тик (контракт §10).
    int32_t mouse_dx = 0;
    int32_t mouse_dy = 0;
    Input_GetMouseDelta(&mouse_dx, &mouse_dy);

    // ESC — глобальный запрос выхода (в любой фазе, вне сцены).
    if (Input_IsKeyDown(INPUT_KEY_ESCAPE)) {
        return true;
    }

    for (auto row : registry.view<PlayerControl, Camera, Position, Rotation, Velocity>()) {
        const PlayerControl* control = &row.get<PlayerControl>();
        Position* position = &row.get<Position>();
        Rotation* rotation = &row.get<Rotation>();
        Velocity* velocity = &row.get<Velocity>();
        (void)position; // позиция читается копированием в MovementSystem

        // --- Мышь-взгляд ---------------------------------------------------
        // dx > 0 (вправо) → yaw увеличивается (взгляд уходит в +X при yaw=0);
        // dy > 0 (вниз) → pitch уменьшается (взгляд вниз), кламп ±предел.
        if (mouse_dx != 0 || mouse_dy != 0) {
            rotation->yaw = fixed_add(rotation->yaw,
                                      fixed_mul(fixed_from_int(mouse_dx), control->rotate_speed));
            rotation->pitch = fixed_sub(rotation->pitch,
                                        fixed_mul(fixed_from_int(mouse_dy), control->rotate_speed));
            if (rotation->pitch > CAMERA_PITCH_LIMIT) {
                rotation->pitch = CAMERA_PITCH_LIMIT;
            } else if (rotation->pitch < -CAMERA_PITCH_LIMIT) {
                rotation->pitch = -CAMERA_PITCH_LIMIT;
            }
        }

        // --- Клавиатура → целевая скорость входа тика -----------------------
        const Vec3 forward = camera_forward(rotation->yaw, rotation->pitch);
        const Vec3 right = camera_right(rotation->yaw);

        const bool move_fwd = Input_IsKeyDown(INPUT_KEY_W) || Input_IsKeyDown(INPUT_KEY_UP);
        const bool move_back = Input_IsKeyDown(INPUT_KEY_S) || Input_IsKeyDown(INPUT_KEY_DOWN);
        const bool move_right = Input_IsKeyDown(INPUT_KEY_D) || Input_IsKeyDown(INPUT_KEY_RIGHT);
        const bool move_left = Input_IsKeyDown(INPUT_KEY_A) || Input_IsKeyDown(INPUT_KEY_LEFT);
        const bool move_up = Input_IsKeyDown(INPUT_KEY_SPACE);
        const bool move_down =
            Input_IsKeyDown(INPUT_KEY_LSHIFT) || Input_IsKeyDown(INPUT_KEY_RSHIFT);

        Vec3 direction = vec3_make(0, 0, 0);
        if (move_fwd) {
            direction = vec3_add(direction, forward);
        }
        if (move_back) {
            direction = vec3_sub(direction, forward);
        }
        if (move_right) {
            direction = vec3_add(direction, right);
        }
        if (move_left) {
            direction = vec3_sub(direction, right);
        }
        if (move_up) {
            direction = vec3_add(direction, vec3_make(0, FIXED_ONE, 0));
        }
        if (move_down) {
            direction = vec3_sub(direction, vec3_make(0, FIXED_ONE, 0));
        }

        // Нормирование диагоналей (равная скорость всех 8 направлений).
        // Нулевое направление НЕ нормируется: контракт vec3_normalize делит
        // на нулевую длину (debug assert / release-насыщение) — честная
        // ветка пустого хода прямо здесь.
        velocity->value = vec3_make(0, 0, 0);
        if (direction.x != 0 || direction.y != 0 || direction.z != 0) {
            velocity->value = vec3_scale(vec3_normalize(direction), control->move_speed);
        }
    }
    return false;
}

} // namespace game
