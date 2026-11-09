// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/systems/input_system.h
//
// Система ввода слоя игры (header-only): единая абстракция для PC
// (клавиатура + мышь + геймпад) и PS1 (геймпад). Норматив:
// docs/plans/sprint_11_12.md, неделя 23 (мышь/клавиатура) и
// docs/plans/sprint_15_16.md, неделя 31 (геймпад на PC+PS1).
//
// §10 — InputSystem тик ПЕРВЫЙ внутри тика фикс-симуляции (60 Гц), мышь
// аккумулируется платформой между тиками (Input_GetMouseDelta — consume
// ровно один раз за тик).
//
// Контракты:
//   - игрок = сущность с PlayerControl+Camera+Position+Rotation+Velocity:
//     система пишет ЦЕЛЕВУЮ скорость текущего тика (units/секунду) в Velocity;
//     интеграция позиции — MovementSystem (разделение фаз §10);
//   - масштаб скорости от dt НЕ зависит: move_speed — units/секунду,
//     MovementSystem умножает на длину тика;
//   - направление диагонали нормируется (равная скорость всех направлений);
//   - мышь двигает yaw/pitch ТОЛЬКО в относительном режиме (платформа копит
//     дельты только в нём; в bench-режиме дельты всегда 0);
//   - геймпад: левый стик / D-pad — движение камеры в плоскости yaw;
//     правый стик — yaw/pitch; START (PC/PS1) — выход;
//   - @return true — запрошен выход (ESC зажат или START на геймпаде).
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

/** Deadzone для аналоговых осей геймпада (в сжатом масштабе −128..127). */
constexpr int16_t INPUT_PAD_AXIS_DEADZONE = 16;

/** Множитель скорости взгляда правого стика относительно сенсы мыши.
 *  Full deflection правого стика ≈ rotate_speed * kPadRotateScale за тик. */
const fixed_t kPadRotateScale = fixed_from_int(8);

namespace {

/**
 * Сжатая ось геймпада → коэффициент −1..1 в fixed 16.16 с deadzone.
 */
inline fixed_t pad_axis_ratio(int16_t axis)
{
    if (axis > -INPUT_PAD_AXIS_DEADZONE && axis < INPUT_PAD_AXIS_DEADZONE) {
        return 0;
    }
    if (axis < -128) {
        axis = -128;
    } else if (axis > 127) {
        axis = 127;
    }
    return fixed_div(fixed_from_int(axis), fixed_from_int(128));
}

inline void clamp_pitch(fixed_t* pitch)
{
    if (*pitch > CAMERA_PITCH_LIMIT) {
        *pitch = CAMERA_PITCH_LIMIT;
    } else if (*pitch < -CAMERA_PITCH_LIMIT) {
        *pitch = -CAMERA_PITCH_LIMIT;
    }
}

} // namespace

class InputSystem {
public:
    /**
     * Тик ввода 60 Гц: мышь/правый стик → yaw/pitch игрока; клавиши/
     * левый стик/D-pad → Velocity.
     *
     * @param registry ECS-реестр сцены.
     * @param dt Дельта тика (FIXED_ONE/60); масштаб скорости от dt не
     *        зависит (см. контракты) — параметр под единый сигнатурный
     *        контракт систем тика.
     * @return true — ESC зажат или START на геймпаде (запрос выхода).
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

    // START на геймпаде — единый запрос выхода на PC и PS1.
    if (Input_IsPadButtonDown(0, INPUT_PAD_START)) {
        return true;
    }

    for (auto row : registry.view<PlayerControl, Camera, Position, Rotation, Velocity>()) {
        const PlayerControl* control = &row.get<PlayerControl>();
        Rotation* rotation = &row.get<Rotation>();
        Velocity* velocity = &row.get<Velocity>();

        // --- Мышь-взгляд (PC) --------------------------------------------
        if (mouse_dx != 0 || mouse_dy != 0) {
            rotation->yaw = fixed_add(rotation->yaw,
                                      fixed_mul(fixed_from_int(mouse_dx), control->rotate_speed));
            rotation->pitch = fixed_sub(rotation->pitch,
                                        fixed_mul(fixed_from_int(mouse_dy), control->rotate_speed));
            clamp_pitch(&rotation->pitch);
        }

        // --- Правый стик — взгляд (геймпад PC+PS1) ------------------------
        const fixed_t rx = pad_axis_ratio(Input_GetPadAxis(0, INPUT_AXIS_RIGHTX));
        const fixed_t ry = pad_axis_ratio(Input_GetPadAxis(0, INPUT_AXIS_RIGHTY));
        if (rx != 0 || ry != 0) {
            rotation->yaw = fixed_add(
                rotation->yaw, fixed_mul(fixed_mul(rx, control->rotate_speed), kPadRotateScale));
            rotation->pitch = fixed_sub(
                rotation->pitch, fixed_mul(fixed_mul(ry, control->rotate_speed), kPadRotateScale));
            clamp_pitch(&rotation->pitch);
        }

        // --- Направление движения (камера в плоскости yaw) ----------------
        const Vec3 forward = camera_forward(rotation->yaw, rotation->pitch);
        const Vec3 right = camera_right(rotation->yaw);

        Vec3 direction = vec3_make(0, 0, 0);

        // Клавиатура PC.
        if (Input_IsKeyDown(INPUT_KEY_W) || Input_IsKeyDown(INPUT_KEY_UP)) {
            direction = vec3_add(direction, forward);
        }
        if (Input_IsKeyDown(INPUT_KEY_S) || Input_IsKeyDown(INPUT_KEY_DOWN)) {
            direction = vec3_sub(direction, forward);
        }
        if (Input_IsKeyDown(INPUT_KEY_D) || Input_IsKeyDown(INPUT_KEY_RIGHT)) {
            direction = vec3_add(direction, right);
        }
        if (Input_IsKeyDown(INPUT_KEY_A) || Input_IsKeyDown(INPUT_KEY_LEFT)) {
            direction = vec3_sub(direction, right);
        }
        if (Input_IsKeyDown(INPUT_KEY_SPACE)) {
            direction = vec3_add(direction, vec3_make(0, FIXED_ONE, 0));
        }
        if (Input_IsKeyDown(INPUT_KEY_LSHIFT) || Input_IsKeyDown(INPUT_KEY_RSHIFT)) {
            direction = vec3_sub(direction, vec3_make(0, FIXED_ONE, 0));
        }

        // Геймпад: левый стик.
        const fixed_t lx = pad_axis_ratio(Input_GetPadAxis(0, INPUT_AXIS_LEFTX));
        const fixed_t ly = pad_axis_ratio(Input_GetPadAxis(0, INPUT_AXIS_LEFTY));
        if (lx != 0) {
            direction = vec3_add(direction, vec3_scale(right, lx));
        }
        if (ly != 0) {
            // Ось Y стика: вверх (отрицательная) → вперёд.
            direction = vec3_sub(direction, vec3_scale(forward, ly));
        }

        // Геймпад: D-pad (PC-эмуляция клавиатурой тоже попадает сюда).
        if (Input_IsPadButtonDown(0, INPUT_PAD_UP)) {
            direction = vec3_add(direction, forward);
        }
        if (Input_IsPadButtonDown(0, INPUT_PAD_DOWN)) {
            direction = vec3_sub(direction, forward);
        }
        if (Input_IsPadButtonDown(0, INPUT_PAD_RIGHT)) {
            direction = vec3_add(direction, right);
        }
        if (Input_IsPadButtonDown(0, INPUT_PAD_LEFT)) {
            direction = vec3_sub(direction, right);
        }

        // Нормирование диагоналей (равная скорость всех направлений).
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
