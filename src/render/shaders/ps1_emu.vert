// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/shaders/ps1_emu.vert
//
// Вершина рендера с эмуляцией PS1 (нормативный источник:
// docs/RetroEngine_Engineering_Documentation_v1.43.md §7.4).
//
// Контракты:
//   - noperspective out v_texcoord — АФФИННАЯ интерполяция UV (текстуры
//     «плывут» на наклонных плоскостях — PS1-выветривание W14);
//   - квантование 1/16 px в 320×240 пространстве (wobble heuristic;
//     НЕ нативный SXY-формат GTE — просто детерминированный heuristic);
//   - арибуты/локации — контракт backend-буфера (HalRenderBackendVertex:
//     location 0 = позиция vec3, 1 = uv vec2, 2 = цвет vec4).
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;
layout(location = 2) in vec4 a_color;

uniform mat4 u_mvp;

noperspective out vec2 v_texcoord;
out vec4 v_color;

void main()
{
    vec4 clip = u_mvp * vec4(a_position, 1.0);
    vec2 ndc = clip.xy / clip.w;

    // PS1-style wobble heuristic: 1/16-пиксельное квантование в экранных
    // координатах 320×240 (вершей normative формула, floor(x*16+0.5)/16).
    vec2 screen = (ndc * 0.5 + 0.5) * vec2(320.0, 240.0);
    screen = floor(screen * 16.0 + 0.5) / 16.0;
    ndc = screen / vec2(320.0, 240.0) * 2.0 - 1.0;
    clip.xy = ndc * clip.w;

    gl_Position = clip;
    v_texcoord = a_texcoord; // noperspective => аффинная интерполяция.
    v_color = a_color;
}
