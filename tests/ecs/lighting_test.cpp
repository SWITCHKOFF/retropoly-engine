// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/ecs/lighting_test.cpp
//
// Тесты LightingSystem — vertex lighting ambient + directional на fixed 16.16
// (норматив: docs/plans/sprint_17_18.md, неделя 35: «Vertex lighting:
// ambient + directional (fixed)», «Gouraud: per-vertex RGB»).
//
// Проверяется: точные значения каналов для осевых нормалей (детерминированная
// fixed-математика), ambient-only для граней от света, поворот нормали
// моделью (yaw), неизменность позиций/UV/нормалей lit-копии, деградация при
// отсутствии арены.
#include "core/math/fixed.h"
#include "core/math/mat4.h"
#include "core/math/vec3.h"
#include "core/memory/stack_allocator.h"
#include "game/systems/lighting_system.h"

#include <gtest/gtest.h>

#include <stdint.h>

namespace {

/// Меш-кубик из одной вершины для точечных проверок освещения.
/// Non-copyable: MeshAsset::vertices указывает внутрь той же фикстуры
/// (копия оставила бы указатель на оригинал — dangling).
struct OneVertexFixture
{
    Vertex vertex;
    MeshAsset mesh;

    OneVertexFixture(int8_t nx, int8_t ny, int8_t nz)
        : vertex{0, 0, 0, nx, ny, nz, 0, 0, 255, 255, 255, 255}
    {
        mesh.vertices = &vertex;
        mesh.indices = nullptr;
        mesh.vertex_count = 1u;
        mesh.index_count = 0u;
        mesh.texture_id = 0u;
        mesh.bounding_sphere_radius = FIXED_ONE;
    }

    OneVertexFixture(const OneVertexFixture&) = delete;
    OneVertexFixture& operator=(const OneVertexFixture&) = delete;
};

/// Арена кадра теста (static: не на CPU-стеке — §5.4).
alignas(16) uint8_t g_scratch_buffer[4u * 1024u];

/// Свет строго вверх (+Y), ambient 50, directional 100 — точные ожидания.
game::LightBank up_light_bank()
{
    game::LightBank bank;
    bank.ambient = game::AmbientLight{50, 50, 50};
    bank.directional.direction = vec3_make(0, FIXED_ONE, 0);
    bank.directional.r = 100;
    bank.directional.g = 100;
    bank.directional.b = 100;
    return bank;
}

} // namespace

TEST(LightingTest, UpwardNormalGetsExactDiffuseChannel)
{
    StackAllocator scratch(g_scratch_buffer, sizeof(g_scratch_buffer));
    OneVertexFixture src(0, 127, 0); // нормаль +Y
    Mat4 identity;
    mat4_identity(&identity);

    const game::LightBank lights = up_light_bank();
    const Vertex* lit =
        game::LightingSystem::light_mesh_copy(&scratch, &src.mesh, identity, lights);
    ASSERT_NE(lit, nullptr);
    // n = 127·516 = 65532 (0.99988); diffuse = 65532;
    // channel = 50 + trunc(100·65532/65536) = 50 + 99 = 149 (все каналы).
    EXPECT_EQ(lit->r, 149u);
    EXPECT_EQ(lit->g, 149u);
    EXPECT_EQ(lit->b, 149u);
}

TEST(LightingTest, NormalAwayFromLightGetsAmbientOnly)
{
    StackAllocator scratch(g_scratch_buffer, sizeof(g_scratch_buffer));
    OneVertexFixture src(0, -127, 0); // нормаль −Y
    Mat4 identity;
    mat4_identity(&identity);

    const Vertex* lit =
        game::LightingSystem::light_mesh_copy(&scratch, &src.mesh, identity, up_light_bank());
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->r, 50u); // ambient, диффуз клампится к 0
    EXPECT_EQ(lit->g, 50u);
    EXPECT_EQ(lit->b, 50u);
}

TEST(LightingTest, PerpendicularNormalGetsAmbientOnly)
{
    StackAllocator scratch(g_scratch_buffer, sizeof(g_scratch_buffer));
    OneVertexFixture src(127, 0, 0); // нормаль +X
    Mat4 identity;
    mat4_identity(&identity);

    const Vertex* lit =
        game::LightingSystem::light_mesh_copy(&scratch, &src.mesh, identity, up_light_bank());
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->r, 50u); // dot = 0 → ambient
}

TEST(LightingTest, ModelRotationTurnsNormalIntoLight)
{
    // Нормаль +Z, свет — в +X; yaw 90° поворачивает +Z → +X: диффуз полный
    // (fixed-тригонометрия sin/cos(π/2) — допуск ±1 LSB канала).
    StackAllocator scratch(g_scratch_buffer, sizeof(g_scratch_buffer));
    OneVertexFixture src(0, 0, 127); // нормаль +Z
    Mat4 yaw;
    mat4_rotate_y(&yaw, FIXED_PI / 2); // yaw 90°

    game::LightBank lights = up_light_bank();
    lights.directional.direction = vec3_make(FIXED_ONE, 0, 0); // на +X

    const Vertex* lit = game::LightingSystem::light_mesh_copy(&scratch, &src.mesh, yaw, lights);
    ASSERT_NE(lit, nullptr);
    EXPECT_GE(lit->r, 148u);
    EXPECT_LE(lit->r, 150u);
}

TEST(LightingTest, LitCopyKeepsGeometryAndUv)
{
    StackAllocator scratch(g_scratch_buffer, sizeof(g_scratch_buffer));
    OneVertexFixture src(0, 127, 0);
    Mat4 identity;
    mat4_identity(&identity);

    const Vertex* lit =
        game::LightingSystem::light_mesh_copy(&scratch, &src.mesh, identity, up_light_bank());
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->x, src.vertex.x);
    EXPECT_EQ(lit->y, src.vertex.y);
    EXPECT_EQ(lit->z, src.vertex.z);
    EXPECT_EQ(lit->nx, src.vertex.nx);
    EXPECT_EQ(lit->ny, src.vertex.ny);
    EXPECT_EQ(lit->nz, src.vertex.nz);
    EXPECT_EQ(lit->u, src.vertex.u);
    EXPECT_EQ(lit->v, src.vertex.v);
    EXPECT_EQ(lit->a, src.vertex.a); // альфа не трогается (transparency off)
}

TEST(LightingTest, ExhaustedScratchDegradesToNull)
{
    // Арена меньше одной вершины — явный nullptr (деградация к неосвещённой
    // отрисовке; никакого чтения за границей и никакого краша).
    alignas(16) uint8_t tiny[sizeof(Vertex) / 2u];
    StackAllocator scratch(tiny, sizeof(tiny));
    OneVertexFixture src(0, 127, 0);
    Mat4 identity;
    mat4_identity(&identity);

    EXPECT_EQ(game::LightingSystem::light_mesh_copy(&scratch, &src.mesh, identity,
                                                    up_light_bank()),
              nullptr);
}

TEST(LightingTest, SameInputsProduceSameChannels)
{
    // Детерминизм: два прогона с идентичными входами — байт-в-байт каналы.
    StackAllocator scratch_a(g_scratch_buffer, sizeof(g_scratch_buffer));
    alignas(16) uint8_t second_buffer[4u * 1024u];
    StackAllocator scratch_b(second_buffer, sizeof(second_buffer));
    OneVertexFixture src(-80, 97, 34);
    Mat4 model;
    mat4_rotate_z(&model, FIXED_PI / 5);
    const game::LightBank lights = up_light_bank();

    const Vertex* lit_a =
        game::LightingSystem::light_mesh_copy(&scratch_a, &src.mesh, model, lights);
    const Vertex* lit_b =
        game::LightingSystem::light_mesh_copy(&scratch_b, &src.mesh, model, lights);
    ASSERT_NE(lit_a, nullptr);
    ASSERT_NE(lit_b, nullptr);
    EXPECT_EQ(lit_a->r, lit_b->r);
    EXPECT_EQ(lit_a->g, lit_b->g);
    EXPECT_EQ(lit_a->b, lit_b->b);
}
