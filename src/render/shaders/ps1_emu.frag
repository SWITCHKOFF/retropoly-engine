// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/shaders/ps1_emu.frag
//
// Пиксельный рендер с эмуляцией PS1 (нормативный источник §7.4):
// CI8-текстура (R8-индекс → палитра 256×1 texelfetch) + Gouraud-умножение
// на вершинный цвет; прозрачность выключена (alpha = 1).
#version 330 core

noperspective in vec2 v_texcoord; // affine UV (соглашение о vert; §7.4)
in vec4 v_color;

uniform sampler2D u_texture_index;
uniform sampler2D u_palette;

out vec4 frag_color;

void main()
{
    uint index = uint(round(texture(u_texture_index, v_texcoord).r * 255.0));
    vec3 color = texelFetch(u_palette, ivec2(int(index), 0), 0).rgb;
    frag_color = vec4(color * v_color.rgb, 1.0);
}
