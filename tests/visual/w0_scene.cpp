// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/visual/w0_scene.cpp
//
// Построение статической W0-сцены (контракты — tests/visual/w0_scene.h).
// Все данные — целые fixed; текстуры/палитра — константные паттерны
// (никакого рандома/файла). Winding «фронта» — конвенция ядра: экранный
// signed area (y вниз) A < 0 — фронт (см. renderer.cpp project_view_to_
// screen_sydown); порядок вершин quad выбирается под эту конвенцию.
#include "tests/visual/w0_scene.h"

#include "core/math/mat4.h"
#include "render/texture.h" // pack_rgb555_stp (нормативный bit-layout).

#include <string.h>

// ---------------------------------------------------------------------------
// Текстура: CI8-чекер 64×64 + палитра RGB555+STP
// ---------------------------------------------------------------------------

namespace {

/// Ячейка чекера (8×8 пикселей; 64/8 = 8 чекеров на плоскость).
const uint32_t W0_CHECKER_CELL = 8u;

} // namespace

const uint8_t* w0_make_checker_pixels(uint32_t* out_w, uint32_t* out_h)
{
    // Статический массив (64×64 = 4096 байт; вызовы — read-only).
    static uint8_t pixels[64 * 64];
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            const uint32_t cell =
                ((x / W0_CHECKER_CELL) + (y / W0_CHECKER_CELL)) & 1u;
            pixels[y * 64u + x] = (cell == 0u) ? 0u : 1u;
        }
    }
    if (out_w != NULL) {
        *out_w = 64u;
    }
    if (out_h != NULL) {
        *out_h = 64u;
    }
    return pixels;
}

const uint16_t* w0_make_checker_palette(void)
{
    // 256×u16: индекс 0 — тёмный фиолет (5-бит 6/2/12); индекс 1 — светлый
    // серо-голубой (5-бит 24/26/30). Остальные — градиент фона деталей
    // (заполняются честным pack; НЕ «(i<<8)|i» — §1.2.5 bit-layout).
    static uint16_t palette[256];
    palette[0] = pack_rgb555_stp(6, 2, 12, false);  // Тёмный чекер.
    palette[1] = pack_rgb555_stp(24, 26, 30, false); // Светлый чекер.
    for (uint32_t i = 2u; i < 256u; ++i) {
        // Градиент (детерминированный): тон полутон — для дезамбигоу PHI
        // визуального контроля CI8-интерполяции (не участвует в комната).
        const uint8_t shade = (uint8_t)(i % 32u);
        palette[i] = pack_rgb555_stp(shade, 0, (uint8_t)(31u - shade), false);
    }
    return palette;
}

// ---------------------------------------------------------------------------
// Комната: 6 плоскостей × 2 треугольника
// ---------------------------------------------------------------------------

namespace {

/**
 * Вставка одной плоскости (quad, переднего winding при контрате CCW изнутри
 * комнаты) в массивы: 4 вершины {u,v,r,g,b,a} + 2 треугольника (v0,v1,v2),
 * повершинная Gouraud-раскраска при вычислении.
 *
 * Порядок вершин quad: (x0,y0)-(x1,y1)-(x2,y2)-(x3,y3) — обход
 * против часовой (CCW СМОТРЯ ИЗНУРИ комнаты) → и A<0 при SY-DOWN
 * проекции ядра (front-face конвенция ядра).
 */
void w0_push_quad(uint32_t* vertex_cursor, uint32_t* tri_cursor,
                  Vertex* vertices, Triangle* triangles,
                  const fixed_t p[4][3], const uint8_t uv[4][2],
                  const uint8_t color[4][3])
{
    const uint32_t base = *vertex_cursor;
    for (uint32_t c = 0u; c < 4u; ++c) {
        Vertex* v = &vertices[base + c];
        memset(v, 0, sizeof(*v));
        v->x = p[c][0];
        v->y = p[c][1];
        v->z = p[c][2];
        v->u = uv[c][0];
        v->v = uv[c][1];
        v->r = color[c][0];
        v->g = color[c][1];
        v->b = color[c][2];
        v->a = 255u; // Прозрачность выключена (норматив W0).
    }
    const uint32_t t = *tri_cursor;
    triangles[t].v0 = (uint16_t)(base + 0u);
    triangles[t].v1 = (uint16_t)(base + 1u);
    triangles[t].v2 = (uint16_t)(base + 2u);
    triangles[t + 1u].v0 = (uint16_t)(base + 0u);
    triangles[t + 1u].v1 = (uint16_t)(base + 2u);
    triangles[t + 1u].v2 = (uint16_t)(base + 3u);
    *vertex_cursor = base + 4u;
    *tri_cursor = t + 2u;
}

} // namespace

void w0_build_room(Vertex* vertices, Triangle* triangles)
{
    uint32_t vc = 0u;
    uint32_t tc = 0u;

    // Комната 10×10×10 ПЕРЕД камерой: куб x,y ∈ [−5,+5], z ∈ [−20,−5];
    // камера (0,0,5) смотрит вдоль −Z — все вершины с d = 5 − z ∈ [10..25]
    // внутри [near..far] = [1..100] (normative: 12 подано, 0 reject).
    // Порядок вершин каждого квада — обход экрана CCW при raster y-вниз
    // (front-face конвенция ядра: signed area A < 0), выведен из видимости
    // с точки камеры: пол/потолок — окольцовка «ближнее ребро→дальнее»,
    // стены x = −5/+5 — зеркальные кольца, стены z = −5/−20 — BL→BR→TR→TL.
    // Чекер UV: (0,0),(255,0),(255,255),(0,255); Gouraud — по цветам стен.
    const uint8_t uv_quad[4][2] = {{0, 0}, {255, 0}, {255, 255}, {0, 255}};

    // --- Пол (y=−5; цвет тёплый, градиент по кольцу) ----------------------
    {
        const fixed_t p[4][3] = {
            {fixed_from_int(-5), fixed_from_int(-5), fixed_from_int(-5)},
            {fixed_from_int(5), fixed_from_int(-5), fixed_from_int(-5)},
            {fixed_from_int(5), fixed_from_int(-5), fixed_from_int(-20)},
            {fixed_from_int(-5), fixed_from_int(-5), fixed_from_int(-20)},
        };
        const uint8_t color[4][3] = {
            {200, 120, 100}, {240, 160, 60}, {120, 200, 60}, {60, 100, 240},
        };
        w0_push_quad(&vc, &tc, vertices, triangles, p, uv_quad, color);
    }

    // --- Потолок (y=+5; холодный, градиент по кольцу) ---------------------
    {
        const fixed_t p[4][3] = {
            {fixed_from_int(-5), fixed_from_int(5), fixed_from_int(-5)},
            {fixed_from_int(-5), fixed_from_int(5), fixed_from_int(-20)},
            {fixed_from_int(5), fixed_from_int(5), fixed_from_int(-20)},
            {fixed_from_int(5), fixed_from_int(5), fixed_from_int(-5)},
        };
        const uint8_t color[4][3] = {
            {80, 80, 160}, {160, 80, 80}, {80, 160, 80}, {160, 80, 160},
        };
        w0_push_quad(&vc, &tc, vertices, triangles, p, uv_quad, color);
    }

    // --- Стена −X (тёпло-жёлтая; кольцо NB→FB→FT→NT) ----------------------
    {
        const fixed_t p[4][3] = {
            {fixed_from_int(-5), fixed_from_int(-5), fixed_from_int(-5)},
            {fixed_from_int(-5), fixed_from_int(-5), fixed_from_int(-20)},
            {fixed_from_int(-5), fixed_from_int(5), fixed_from_int(-20)},
            {fixed_from_int(-5), fixed_from_int(5), fixed_from_int(-5)},
        };
        const uint8_t color[4][3] = {
            {220, 220, 60}, {220, 60, 220}, {60, 220, 220}, {220, 220, 220},
        };
        w0_push_quad(&vc, &tc, vertices, triangles, p, uv_quad, color);
    }

    // --- Стена +X (сине-зелёная; зеркальное кольцо NB→NT→FT→FB) -----------
    {
        const fixed_t p[4][3] = {
            {fixed_from_int(5), fixed_from_int(-5), fixed_from_int(-5)},
            {fixed_from_int(5), fixed_from_int(5), fixed_from_int(-5)},
            {fixed_from_int(5), fixed_from_int(5), fixed_from_int(-20)},
            {fixed_from_int(5), fixed_from_int(-5), fixed_from_int(-20)},
        };
        const uint8_t color[4][3] = {
            {60, 220, 120}, {120, 60, 220}, {220, 120, 60}, {220, 220, 60},
        };
        w0_push_quad(&vc, &tc, vertices, triangles, p, uv_quad, color);
    }

    // --- Стена z=−5 (ближняя, белая-синий градиент; BL→BR→TR→TL) ----------
    {
        const fixed_t p[4][3] = {
            {fixed_from_int(-5), fixed_from_int(-5), fixed_from_int(-5)},
            {fixed_from_int(5), fixed_from_int(-5), fixed_from_int(-5)},
            {fixed_from_int(5), fixed_from_int(5), fixed_from_int(-5)},
            {fixed_from_int(-5), fixed_from_int(5), fixed_from_int(-5)},
        };
        const uint8_t color[4][3] = {
            {240, 240, 240}, {180, 180, 240}, {120, 120, 200}, {200, 200, 120},
        };
        w0_push_quad(&vc, &tc, vertices, triangles, p, uv_quad, color);
    }

    // --- Стена z=−20 (дальняя, тёмная; тот же BL→BR→TR→TL) ---------------
    {
        const fixed_t p[4][3] = {
            {fixed_from_int(-5), fixed_from_int(-5), fixed_from_int(-20)},
            {fixed_from_int(5), fixed_from_int(-5), fixed_from_int(-20)},
            {fixed_from_int(5), fixed_from_int(5), fixed_from_int(-20)},
            {fixed_from_int(-5), fixed_from_int(5), fixed_from_int(-20)},
        };
        const uint8_t color[4][3] = {
            {120, 60, 60}, {240, 240, 60}, {60, 240, 240}, {240, 60, 240},
        };
        w0_push_quad(&vc, &tc, vertices, triangles, p, uv_quad, color);
    }

    // Пересечения: нормы 12 треугольников (6 плоскостей × 2), 24 вершины.
    (void)W0_ROOM_TRIANGLE_COUNT;
    (void)W0_ROOM_VERTEX_COUNT;
}

// ---------------------------------------------------------------------------
// Конформанс-кейсы (детерминированные одиночные треугольники)
// ---------------------------------------------------------------------------

void w0_build_case(W0CaseId case_id, Vertex* vertices, Triangle* triangle)
{
    memset(vertices, 0, sizeof(*vertices) * 3u);
    triangle->v0 = 0u;
    triangle->v1 = 1u;
    triangle->v2 = 2u;

    // Помощник: вершина (x, y, z) — повершинный цвет из фикс-значений.
    Vertex* v = vertices;

    switch (case_id) {
    case W0_CASE_VISIBLE:
        // Фронтальный по winding (A < 0) — виден на экране целиком.
        v[0].x = fixed_from_int(-1);
        v[1].x = fixed_from_int(1);
        v[2].x = 0;
        v[0].y = fixed_from_int(-1);
        v[1].y = fixed_from_int(-1);
        v[2].y = fixed_from_int(1);
        v[0].z = fixed_from_int(-5);
        v[1].z = fixed_from_int(-5);
        v[2].z = fixed_from_int(-5);
        v[0].r = 255;
        v[0].g = 0;
        v[0].b = 0;
        v[1].r = 0;
        v[1].g = 255;
        v[1].b = 0;
        v[2].r = 0;
        v[2].g = 0;
        v[2].b = 255;
        v[0].u = 0;
        v[0].v = 0;
        v[1].u = 255;
        v[1].v = 0;
        v[2].u = 128;
        v[2].v = 255;
        break;
    case W0_CASE_BACKFACE:
        // GEометриі та ж, winding обратен (A ≥ 0) — CULL reject.
        v[0].x = fixed_from_int(1);
        v[1].x = fixed_from_int(-1);
        v[2].x = 0;
        v[0].y = fixed_from_int(-1);
        v[1].y = fixed_from_int(-1);
        v[2].y = fixed_from_int(1);
        v[0].z = fixed_from_int(-5);
        v[1].z = fixed_from_int(-5);
        v[2].z = fixed_from_int(-5);
        break;
    case W0_CASE_NEAR_CROSS:
        // Вершина «пересекает» near plane: d = −(z−5) = −1 (< near = 1),
        // остальные d = 5 (внутри). Whole-triangle clip reject (норматив:
        // explicit rejection, клипсование без сохранения — не реализовано).
        v[0].x = fixed_from_int(-1);
        v[1].x = fixed_from_int(1);
        v[2].x = 0;
        v[0].y = fixed_from_int(-1);
        v[1].y = fixed_from_int(-1);
        v[2].y = fixed_from_int(1);
        v[0].z = fixed_from_int(6);    // d = −1: вне [near..far].
        v[1].z = fixed_from_int(0);    // d = 5.
        v[2].z = fixed_from_int(0);    // d = 5.
        break;
    case W0_CASE_FAR_CROSS:
        // Вершина дальше far: d = −(−110−5) = 115 (> far = 100).
        v[0].x = fixed_from_int(-1);
        v[1].x = fixed_from_int(1);
        v[2].x = 0;
        v[0].y = fixed_from_int(-1);
        v[1].y = fixed_from_int(-1);
        v[2].y = fixed_from_int(1);
        v[0].z = fixed_from_int(-110); // d = 115 → clip reject.
        v[1].z = fixed_from_int(0);
        v[2].z = fixed_from_int(0);
        break;
    case W0_CASE_BEHIND_CAMERA:
    default:
        // Все вершины ЗА камерой (world z = 8 > глаз z = 5; d = −(8−5) =
        // −3 < 0 < near) — invalid depth / clip rejection (кейс 5 нормы).
        v[0].x = fixed_from_int(-1);
        v[1].x = fixed_from_int(1);
        v[2].x = 0;
        v[0].y = fixed_from_int(-1);
        v[1].y = fixed_from_int(-1);
        v[2].y = fixed_from_int(1);
        v[0].z = fixed_from_int(8);
        v[1].z = fixed_from_int(8);
        v[2].z = fixed_from_int(8);
        break;
    }
    // Gouraud/UV оставлены 0 для reject-кейсов — не рисуются, инфлюэнса
    // хэша нет; видимый кейс имеет полную раскраску (см. visible).
}

// ---------------------------------------------------------------------------
// Фиксированная камера W0
// ---------------------------------------------------------------------------

void w0_camera(Mat4* view, Mat4* proj)
{
    const Vec3 eye = {fixed_from_int(0), fixed_from_int(0), fixed_from_int(5)};
    const Vec3 target = {fixed_from_int(0), fixed_from_int(0),
                         fixed_from_int(0)};
    const Vec3 up = {fixed_from_int(0), fixed_from_int(1), fixed_from_int(0)};
    mat4_look_at(view, eye, target, up);
    // fov 60° в fixed (§4.3: FIXED_PI радиан = 205887; 60° = π/3 = 68629).
    const fixed_t fov = FIXED_PI / 3;
    const fixed_t aspect = (fixed_t)FIXED_ONE * 4 / 3; // 4:3 = 320/240.
    mat4_perspective(proj, fov, aspect, fixed_from_int(1), fixed_from_int(100));
}
