// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/shaders/ps1_present.frag
//
// Пиксельный вывод кадра на окно: прямой nearest-сэмпл фреймбуфера
// (масштабирование 320×240 → окно билайнерно НЕ перефильтровывается:
// перф-вывод «classical pixel look»).
#version 330 core

in vec2 v_uv;
uniform sampler2D u_framebuffer;

out vec4 frag_color;

void main()
{
    frag_color = vec4(texture(u_framebuffer, v_uv).rgb, 1.0);
}
