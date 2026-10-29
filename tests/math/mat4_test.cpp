// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/math/mat4_test.cpp
//
// Юнит-тесты Mat4/тригонометрии (16.16) — docs/plans/sprint_01_02.md, Неделя 4.
// Эталонные значения — ручная математика в double; детерминизм — на raw-значениях
// с побитовой фиксацией golden-векторов (спринт-критерий: «зафиксировать golden»).
#include "core/math/mat3.h"
#include "core/math/mat4.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <climits>

namespace
{
/** Абсолютный float-допуск теста (16.16 точность много меньше). */
constexpr double k_tol = 0.01;

/** Компонентный NEAR для Vec4/DIM-3-вектора в float-единицах. */
void expect_near_vec4f(const Vec4& v, double x, double y, double z, double w, double tol)
{
    EXPECT_NEAR(fixed_to_float(v.x), x, tol);
    EXPECT_NEAR(fixed_to_float(v.y), y, tol);
    EXPECT_NEAR(fixed_to_float(v.z), z, tol);
    EXPECT_NEAR(fixed_to_float(v.w), w, tol);
}
} // namespace

// ----------------------------------------------------------------------------
// Тригонометрия (sin_fixed / cos_fixed)
// ----------------------------------------------------------------------------

TEST(Trig, SinCosKnownAngles)
{
    // sin(0) = 0 точно (полином в нуле даёт ровный 0).
    EXPECT_EQ(sin_fixed(0), 0);
    EXPECT_NEAR(fixed_to_float(sin_fixed(FIXED_HALF_PI)), 1.0, k_tol);
    EXPECT_NEAR(fixed_to_float(sin_fixed(FIXED_PI)), 0.0, 0.001);
    EXPECT_NEAR(fixed_to_float(sin_fixed(-FIXED_HALF_PI)), -1.0, k_tol);

    EXPECT_NEAR(fixed_to_float(cos_fixed(0)), 1.0, k_tol);
    EXPECT_NEAR(fixed_to_float(cos_fixed(FIXED_HALF_PI)), 0.0, 0.001);
    EXPECT_NEAR(fixed_to_float(cos_fixed(FIXED_PI)), -1.0, k_tol);

    // π/4: sin = cos = √2/2 ≈ 0.7071.
    EXPECT_NEAR(fixed_to_float(sin_fixed(FIXED_HALF_PI / 2)), 0.7071067, k_tol);
    EXPECT_NEAR(fixed_to_float(cos_fixed(FIXED_HALF_PI / 2)), 0.7071067, k_tol);

    // Периодичность и знаковая симметрия (raw).
    const fixed_t ref = sin_fixed(FIXED_TWO_PI + FIXED_HALF_PI / 3);
    EXPECT_NEAR(fixed_to_float(ref), fixed_to_float(sin_fixed(FIXED_HALF_PI / 3)), k_tol);
    EXPECT_NEAR(fixed_to_float(sin_fixed(-FIXED_HALF_PI / 3)),
        -fixed_to_float(sin_fixed(FIXED_HALF_PI / 3)), k_tol);

    // Большие углы: приведение остатком корректно (целочисленный rem — floor-мод).
    const fixed_t big = sin_fixed(7 * FIXED_TWO_PI + FIXED_HALF_PI / 4);
    EXPECT_NEAR(fixed_to_float(big), fixed_to_float(sin_fixed(FIXED_HALF_PI / 4)), k_tol);
}

// ----------------------------------------------------------------------------
// Identity / Multiply
// ----------------------------------------------------------------------------

TEST(Mat4, IdentityNeutral)
{
    Mat4 rot;
    Mat4 translated;
    mat4_rotate_y(&rot, FIXED_HALF_PI / 3);
    mat4_translate(&translated, fixed_from_int(5), fixed_from_int(-6), fixed_from_int(7));

    Mat4 id;
    mat4_identity(&id);

    Mat4 san;
    san = translated; // числовая копия аргумента: побитное сравнение out/in
    mat4_multiply(&san, &san, &id);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(san.m[i], translated.m[i]) << "idx " << i;
    }
    san = rot;
    mat4_multiply(&san, &id, &san);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(san.m[i], rot.m[i]) << "idx " << i;
    }
}

TEST(Mat4, MultiplyAssociativeOnFixedVectors)
{
    Mat4 a;
    Mat4 b;
    Mat4 c;
    mat4_translate(&a, fixed_from_int(2), fixed_from_int(1), fixed_from_int(-3)); // A
    mat4_rotate_y(&b, FIXED_HALF_PI / 2);                                        // B (45°)
    mat4_rotate_z(&c, FIXED_HALF_PI);                                            // C (90°)

    Mat4 ab;
    Mat4 bc;
    mat4_multiply(&ab, &a, &b);
    mat4_multiply(&bc, &b, &c);

    Mat4 abc_left;  // (A·B)·C
    Mat4 abc_right; // A·(B·C)
    mat4_multiply(&abc_left, &ab, &c);
    mat4_multiply(&abc_right, &a, &bc);

    const Vec4 points[4] = {
        vec4_make(fixed_from_int(1), fixed_from_int(0), fixed_from_int(0), FIXED_ONE),
        vec4_make(fixed_from_int(0), fixed_from_int(3), fixed_from_int(-2), FIXED_ONE),
        vec4_make(fixed_from_float(0.25f), fixed_from_float(-1.5f), fixed_from_float(2.75f), FIXED_ONE),
        vec4_make(fixed_from_int(7), fixed_from_int(11), fixed_from_int(13), FIXED_ONE),
    };
    for (const Vec4 p : points)
    {
        const Vec4 left = mat4_transform(&abc_left, p);
        const Vec4 right = mat4_transform(&abc_right, p);
        // Ассоциативность на фиксированных векторах: переход через расширенную
        // матрицу вводит округление ≤ 1 LSB на элемент → допуск 0.01.
        expect_near_vec4f(left, fixed_to_float(right.x), fixed_to_float(right.y),
            fixed_to_float(right.z), fixed_to_float(right.w), k_tol);
    }
}

TEST(Mat4, MultiplyAliasingSafe)
{
    Mat4 a;
    Mat4 b;
    mat4_translate(&a, fixed_from_int(3), fixed_from_int(1), fixed_from_int(0));
    mat4_rotate_z(&b, FIXED_HALF_PI);

    Mat4 expected; // эталон: без алиасинга
    mat4_multiply(&expected, &a, &b);

    // out == первый аргумент.
    Mat4 first = a;
    mat4_multiply(&first, &first, &b);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(first.m[i], expected.m[i]) << "idx " << i;
    }

    // out == второй аргумент.
    Mat4 second = b;
    mat4_multiply(&second, &a, &second);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(second.m[i], expected.m[i]) << "idx " << i;
    }

    // out == оба аргумента (I·I = I при Identity-алиасинге).
    Mat4 same;
    mat4_identity(&same);
    mat4_multiply(&same, &same, &same);
    Mat4 id;
    mat4_identity(&id);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(same.m[i], id.m[i]) << "idx " << i;
    }
}

// ----------------------------------------------------------------------------
// Translate / Rotate (известные углы: 90° → поворот осей)
// ----------------------------------------------------------------------------

TEST(Mat4, TranslateKnownValues)
{
    Mat4 t;
    mat4_translate(&t, fixed_from_int(1), fixed_from_int(2), fixed_from_int(3));
    const Vec4 origin = mat4_transform(&t, vec4_make(0, 0, 0, FIXED_ONE));
    EXPECT_EQ(origin.x, fixed_from_int(1));
    EXPECT_EQ(origin.y, fixed_from_int(2));
    EXPECT_EQ(origin.z, fixed_from_int(3));
    EXPECT_EQ(origin.w, FIXED_ONE);
}

TEST(Mat4, RotateAxes90Degrees)
{
    // 90°: sin(π/2)≈1.000, cos(π/2)≈0.0 — оси поворачиваются без float-накопления.
    Mat4 rz;
    mat4_rotate_z(&rz, FIXED_HALF_PI);
    const Vec4 x_axis = mat4_transform(&rz, vec4_make(FIXED_ONE, 0, 0, FIXED_ONE));
    expect_near_vec4f(x_axis, 0.0, 1.0, 0.0, 1.0, k_tol);

    const Vec4 y_axis = mat4_transform(&rz, vec4_make(0, FIXED_ONE, 0, FIXED_ONE));
    expect_near_vec4f(y_axis, -1.0, 0.0, 0.0, 1.0, k_tol);

    Mat4 rx;
    mat4_rotate_x(&rx, FIXED_HALF_PI);
    const Vec4 y_to_z = mat4_transform(&rx, vec4_make(0, FIXED_ONE, 0, FIXED_ONE));
    expect_near_vec4f(y_to_z, 0.0, 0.0, 1.0, 1.0, k_tol);

    Mat4 ry;
    mat4_rotate_y(&ry, FIXED_HALF_PI);
    const Vec4 z_to_x = mat4_transform(&ry, vec4_make(0, 0, FIXED_ONE, FIXED_ONE));
    expect_near_vec4f(z_to_x, 1.0, 0.0, 0.0, 1.0, k_tol);
}

// ----------------------------------------------------------------------------
// Perspective
// ----------------------------------------------------------------------------

TEST(Mat4, PerspectiveNearFarProjectCorrectly)
{
    const Mat4 proj = [&]() {
        Mat4 p;
        mat4_perspective(&p, fixed_from_float(1.0471976f), fixed_from_float(1.3333333f),
            fixed_from_float(0.1f), fixed_from_int(100));
        return p;
    }();

    const fixed_t nz = fixed_from_float(0.1f);
    const fixed_t fz = fixed_from_int(100);

    // Точка на ближней плоскости: NDC z = −1; на дальней: NDC z = +1.
    const Vec4 vn = mat4_transform(&proj, vec4_make(0, 0, -nz, FIXED_ONE));
    EXPECT_NEAR(fixed_to_float(vn.w), 0.1, 0.01); // w' = −z_view = near
    EXPECT_NEAR(fixed_to_float(vn.z) / fixed_to_float(vn.w), -1.0, k_tol);

    const Vec4 vf = mat4_transform(&proj, vec4_make(0, 0, -fz, FIXED_ONE));
    EXPECT_NEAR(fixed_to_float(vf.z) / fixed_to_float(vf.w), +1.0, k_tol);

    // Экранные координаты на оси не смещаются (x = y = 0 остаётся 0).
    EXPECT_NEAR(fixed_to_float(vn.x), 0.0, 0.001);
    EXPECT_NEAR(fixed_to_float(vn.y), 0.0, 0.001);

    // Смещение по X проецируется с фактором f/aspect на полуплоскость ±1 при
    // x_n = near (грань видимой области): x_ndc = 1 при x = tan(fov/2)·near·aspect.
    EXPECT_GT(fixed_to_float(vn.x), -0.001);
}

TEST(Mat4, PerspectiveDegenerateFovQuarter)
{
    // fov = π/2: f = cot(π/4) = 1 → m[5] == 1 (raw).
    Mat4 p;
    mat4_perspective(&p, FIXED_HALF_PI, FIXED_ONE, fixed_from_float(0.1f), fixed_from_int(100));
    EXPECT_NEAR(fixed_to_float(p.m[5]), 1.0, k_tol);
    // frustum по Y: вершина z_view = −near → ndc z = −1 независимо от fov.
    const Vec4 vn = mat4_transform(&p, vec4_make(0, 0, -fixed_from_float(0.1f), FIXED_ONE));
    EXPECT_NEAR(fixed_to_float(vn.z) / fixed_to_float(vn.w), -1.0, k_tol);
}

// ----------------------------------------------------------------------------
// LookAt
// ----------------------------------------------------------------------------

TEST(Mat4, LookAtViewsTarget)
{
    Mat4 view;
    mat4_look_at(&view, vec3_make(0, 0, fixed_from_int(5)), vec3_make(0, 0, 0),
        vec3_make(0, FIXED_ONE, 0));

    // Цель попадает на ось взгляда: view-пространство (0, 0, −5) (камера смотрит в −Z).
    const Vec4 vt = mat4_transform(&view, vec4_make(0, 0, 0, FIXED_ONE));
    expect_near_vec4f(vt, 0.0, 0.0, -5.0, 1.0, k_tol);

    // Точка перед камерой (z_world = 1, между камерой и целью) → (0, 0, −4).
    const Vec4 v1 = mat4_transform(&view, vec4_make(0, 0, fixed_from_int(1), FIXED_ONE));
    expect_near_vec4f(v1, 0.0, 0.0, -4.0, 1.0, k_tol);

    // Ортогональный базис: столбцы 0..2 попарно ортогональны (dot ≈ 0).
    EXPECT_NEAR(fixed_to_float(vec3_dot(
                    vec3_make(view.m[0], view.m[1], view.m[2]), vec3_make(view.m[4], view.m[5], view.m[6]))),
        0.0, k_tol);
}

// ----------------------------------------------------------------------------
// Mat3: rotation-подмножество (для GTE-конверсии 15–16)
// ----------------------------------------------------------------------------

TEST(Mat3, IdentityAndRotationSubset)
{
    Mat4 rot4;
    Mat4 t4;
    mat4_rotate_x(&rot4, FIXED_HALF_PI / 2);
    mat4_translate(&t4, fixed_from_int(9), fixed_from_int(8), fixed_from_int(7));

    Mat3 r3;
    mat3_from_mat4(&r3, &rot4);
    // Rotation-часть не смешана с переносом: mat3 от translate — единичная.
    Mat3 t3;
    mat3_from_mat4(&t3, &t4);
    Mat3 id3;
    mat3_identity(&id3);
    for (int i = 0; i < 9; ++i)
    {
        EXPECT_EQ(t3.m[i], id3.m[i]) << "idx " << i;
    }

    // mat3_transform поворачивает как верхняя 3×3 Mat4.
    const Vec3 v3 = vec3_make(0, FIXED_ONE, 0);
    const Vec3 via4 = vec3_make(0, mat4_transform(&rot4, vec4_make(v3.x, v3.y, v3.z, FIXED_ONE)).y,
        mat4_transform(&rot4, vec4_make(v3.x, v3.y, v3.z, FIXED_ONE)).z);
    const Vec3 via3 = mat3_transform(&r3, v3);
    EXPECT_EQ(via3.x, via4.x);
    EXPECT_EQ(via3.y, via4.y);
    EXPECT_EQ(via3.z, via4.z);
}

// ----------------------------------------------------------------------------
// Детерминизм: побитная фиксация золотых векторов (golden vectors)
// ----------------------------------------------------------------------------

namespace
{
// Golden-векторы захвачены фактическим прогоном канонической сборки
// (детерминированная Q16.16 арифметика; PC/PS1 требуют побитного равенства).
// Фиксированная последовательность: T(1,2,3) · Rz(90°) · Perspective(60°,4:3,0.1,100).
const fixed_t GOLDEN_G1[16] = {
    -1,
    65536,
    0,
    0,
    -65536,
    -1,
    0,
    0,
    0,
    0,
    65536,
    0,
    65536,
    131072,
    196608,
    65536,
};
const fixed_t GOLDEN_G2[16] = {
    -1,
    85134,
    0,
    0,
    -113512,
    -1,
    0,
    0,
    -65536,
    -131072,
    -262275,
    -65536,
    0,
    0,
    -13119,
    0,
};
const fixed_t GOLDEN_T[4] = { -17560, -219576, -537669, -131072 };
} // namespace

TEST(Mat4, GoldenDeterminismRaw)
{
    // Фиксированная последовательность: T(1,2,3) · Rz(90°) · Perspective(60°,4:3,0.1,100)
    Mat4 a;
    Mat4 b;
    Mat4 c;
    mat4_translate(&a, fixed_from_int(1), fixed_from_int(2), fixed_from_int(3));
    mat4_rotate_z(&b, FIXED_HALF_PI);
    mat4_perspective(&c, fixed_from_float(1.0471976f), fixed_from_float(1.3333333f),
        fixed_from_float(0.1f), fixed_from_int(100));

    Mat4 g1;
    Mat4 g2;
    mat4_multiply(&g1, &a, &b);
    mat4_multiply(&g2, &g1, &c);

    // Один и тот же вход → побитно идентичный raw-выход (повтор вызова).
    Mat4 again1;
    Mat4 again2;
    mat4_multiply(&again1, &a, &b);
    mat4_multiply(&again2, &again1, &c);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(again1.m[i], g1.m[i]) << "idx " << i;
        EXPECT_EQ(again2.m[i], g2.m[i]) << "idx " << i;
        EXPECT_EQ(g1.m[i], GOLDEN_G1[i]) << "idx " << i;
        EXPECT_EQ(g2.m[i], GOLDEN_G2[i]) << "idx " << i;
    }

    // Golden-вектор преобразования точки (0.5, −1.0, 2.0, 1) матрицей g2.
    const Vec4 v = vec4_make(fixed_from_float(0.5f), fixed_from_float(-1.0f),
        fixed_from_float(2.0f), FIXED_ONE);
    const Vec4 t = mat4_transform(&g2, v);
    EXPECT_EQ(t.x, GOLDEN_T[0]);
    EXPECT_EQ(t.y, GOLDEN_T[1]);
    EXPECT_EQ(t.z, GOLDEN_T[2]);
    EXPECT_EQ(t.w, GOLDEN_T[3]);
}
