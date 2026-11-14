// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/systems/camera_math.h
//
// Общая FPS-камера слоя игры (детерминированная fixed-математика).
// Норматив: docs/plans/sprint_11_12.md, неделя 23 (мышь-взгляд, fixed
// perspective + look_at); формула объявлена здесь ЕДИНСТВЕННО для
// InputSystem (движение вдоль направлений камеры) и RenderSystem
// (view-матрица кадра). Единая формула предотвращает расползание констант
// систем (drift).
//
// Соглашения (right-handed, Y-up, камера смотрит в −Z при yaw = pitch = 0):
//   forward = (sin yaw·cos pitch, sin pitch, −cos yaw·cos pitch);
//   right   = (cos yaw, 0, sin yaw)   — горизонтальна, от pitch не зависит;
//   экранный dx > 0 (мышь вправо) увеличивает yaw — взгляд пойдёт в +X;
//   экранный dy > 0 (мышь вниз) уменьшает pitch (взгляд вниз) — Y экрана
//   направлен вниз (контракт Input_GetMouseDelta).
#pragma once

#include <retro_engine/core/math/fixed.h>
#include <retro_engine/core/math/mat4.h>
#include <retro_engine/core/math/vec3.h>

namespace game {

/** Направление взгляда FPS-камеры по yaw/pitch (радианы, 16.16). */
inline Vec3 camera_forward(fixed_t yaw, fixed_t pitch)
{
    const fixed_t cos_pitch = cos_fixed(pitch);
    return vec3_make(fixed_mul(sin_fixed(yaw), cos_pitch), sin_fixed(pitch),
                     fixed_sub(0, fixed_mul(cos_fixed(yaw), cos_pitch)));
}

/** Горизонтальная ось «вправо» камеры (не зависит от pitch). */
inline Vec3 camera_right(fixed_t yaw)
{
    return vec3_make(cos_fixed(yaw), 0, sin_fixed(yaw));
}

/**
 * View-матрица кадра: look_at(eye, eye + forward, +Y).
 *
 * Вырожденность (pitch = ±π/2 — forward ∥ up) исключается InputSystem:
 * pitch клампится до ±(π/2 − 1/32) (см. input_system.h).
 */
inline void camera_view(Mat4* out_view, Vec3 eye, fixed_t yaw, fixed_t pitch)
{
    const Vec3 forward = camera_forward(yaw, pitch);
    mat4_look_at(out_view, eye, vec3_add(eye, forward), vec3_make(0, FIXED_ONE, 0));
}

} // namespace game
