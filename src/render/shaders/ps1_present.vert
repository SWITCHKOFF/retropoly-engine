// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/shaders/ps1_present.vert
//
// Выкладка reference-FBO (320×240) на окно: полноэкранный quad.
// UV прямо из атрибута (nearest-сэмплинг задаёт GL_TEXTURE_MAG_FILTER
// самой фреймбуфер-текстуры — ближайший сосед, без фильтра).
#version 330 core

layout(location = 0) in vec2 a_position; // NDC (−1..1).
layout(location = 1) in vec2 a_texcoord; // UV фреймбуфера (0..1).

out vec2 v_uv;

void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_uv = a_texcoord;
}
