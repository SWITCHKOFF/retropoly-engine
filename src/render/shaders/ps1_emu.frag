// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/shaders/ps1_emu.frag
//
// Пиксельный рендер с эмуляцией PS1 (нормативный источник §7.4):
// CI8-текстура (R8-индекс → палитра 256×1 texelfetch) + Gouraud-умножение
// на вершинный цвет; прозрачность выключена (alpha = 1).
// Нетекстурированный режим (u_has_texture = 0, недели 15–16): без семпла —
// «flat»-Gouraud прямо по вершинному цвету (PS1-паттерн: polygon без
// texture рисуется цветом вершин; ветка семплирования не выполняется).
#version 330 core

noperspective in vec2 v_texcoord; // affine UV (соглашение о vert; §7.4)
in vec4 v_color;

uniform sampler2D u_texture_index;
uniform sampler2D u_palette;
uniform int u_has_texture; // 1 = CI8-семпл; 0 = нетекстурированный Gouraud.

out vec4 frag_color;

void main()
{
    if (u_has_texture != 0) {
        uint index = uint(round(texture(u_texture_index, v_texcoord).r * 255.0));
        vec3 surface = texelFetch(u_palette, ivec2(int(index), 0), 0).rgb;
        frag_color = vec4(surface * v_color.rgb, 1.0);
        return;
    }
    // Нетекстурированный: чистый Gouraud (текстурные юниты не семплируются).
    frag_color = vec4(v_color.rgb, 1.0);
}
