// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/systems/render_system.h
//
// Рендер-система слоя игры (header-only): итерация Registry → команды ядра
// рендера. Норматив: docs/plans/sprint_11_12.md, неделя 22; docs/
// RetroEngine_Engineering_Documentation_v1.43.md §10 (главный цикл:
// RenderSystem::update собирает команды КАЖДОГО кадра — вне тиков фикс-
// симуляции) и §6 (DrawMesh/SetCamera contract).
//
// Контракты:
//   - система НЕ владеет ассетами: меши/текстуры приходят банками, память
//     у владельца (арена примера/теста), хэндлы — только ссылки;
//   - Painter's-сортировка и растеризация — ядро (Render_EndFrame);
//     система только подаёт примитивы;
//   - ровно одна камера: ПЕРВАЯ строка view<Camera, Position, Rotation>
//     (dense-порядок пула — детерминированный выбор); incomplete-камера
//     (нет Position/Rotation) — не собирательный путь: builder сцены
//     гарантирует полный состав камеры;
//   - view-матрица — единой формулой camera_math.h (fps-камера);
//   - model = translate · Rz(roll) · Ry(yaw) · Rx(pitch).
#pragma once

#include <retro_engine/assets/assets.h> // MeshAsset (банк мешей слоя игры)
#include <retro_engine/core/ecs/registry.h>
#include <retro_engine/core/math/fixed.h>
#include <retro_engine/core/math/mat4.h>
#include <retro_engine/core/math/vec3.h>
#include <retro_engine/core/memory/stack_allocator.h>
#include <retro_engine/game/systems/camera_math.h>
#include <retro_engine/game/systems/lighting_system.h>
#include <retro_engine/hal/platform.h> // Render_*, Vertex/Triangle, TextureHandle

#include <stddef.h> // uint32_t через registry.h/stdint; NULL не нужен

namespace game {

/** Банки ассетов для отрисовки (индексы mesh_id/texture_id → массивы). */
struct RenderBanks
{
    const MeshAsset* meshes = nullptr;       /**< Банк мешей по MeshRenderer::mesh_id. */
    uint32_t mesh_count = 0u;                /**< Число слотов банка (0 — кадр без draws). */
    const TextureHandle* textures = nullptr; /**< Хэндлы текстур по MeshRenderer::texture_id. */
    uint32_t texture_count = 0u;             /**< Число хэндлов банка (0 — все draws flat). */
    fixed_t aspect = FIXED_ONE;              /**< Ширина/высота фреймбуфера (16.16; 320/240). */
    /**
     * Банк света (S17–18, неделя 35): NULL — вершины рисуются цветами
     * ассета как есть (поведение до S17–18); не-NULL — per-draw vertex
     * lighting (ambient + directional, fixed) в lit-копии вершин.
     */
    const LightBank* lights = nullptr;
    /**
     * Арена кадра для lit-копий вершин (если lights != NULL): жизнь до
     * reset_frame после Render_EndFrame (§10). Исчерпание — деградация к
     * неосвещённому мешу (явная, документирована в lighting_system.h).
     */
    StackAllocator* frame_scratch = nullptr;
};

class RenderSystem {
public:
    /**
     * Сбор кадра: камера текущего Registry → Render_SetCamera; каждая
     * строка view<MeshRenderer, Position, Rotation> → матрица модели → draw.
     *
     * @param registry ECS-реестр сцены.
     * @param banks Банки ассетов и свойства проекции (см. RenderBanks).
     */
    static void update(Registry& registry, const RenderBanks& banks);
};

inline void RenderSystem::update(Registry& registry, const RenderBanks& banks)
{
    // --- Камера: первая строка пересечения Camera+Position+Rotation --------
    for (auto cam_row : registry.view<Camera, Position, Rotation>()) {
        const Camera* cam = &cam_row.get<Camera>();
        const Position* pos = &cam_row.get<Position>();
        const Rotation* rot = &cam_row.get<Rotation>();

        Mat4 view;
        Mat4 proj;
        camera_view(&view, pos->value, rot->yaw, rot->pitch);
        mat4_perspective(&proj, cam->fov, banks.aspect, cam->near_plane, cam->far_plane);
        Render_SetCamera(&view, &proj);
        break; // ровно одна активная камера (первая dense-строка пула)
    }

    // --- Прокладка: каждая сущность с мешем → командам ядра ---------------
    for (auto row : registry.view<MeshRenderer, Position, Rotation>()) {
        const MeshRenderer* mesh_render = &row.get<MeshRenderer>();
        const Position* pos = &row.get<Position>();
        const Rotation* rot = &row.get<Rotation>();

        // Ссылка вне банка — деградация сцены, а не краш: draw пропускается
        // (детерминированная защита слоя против ошибок контента).
        if (banks.mesh_count == 0u || mesh_render->mesh_id >= banks.mesh_count) {
            continue;
        }
        const MeshAsset* mesh = &banks.meshes[mesh_render->mesh_id];

        // model = translate · Rz(roll) · Ry(yaw) · Rx(pitch)
        Mat4 rotate_x, rotate_y, rotate_z, translate, model, model_rot;
        mat4_rotate_x(&rotate_x, rot->pitch);
        mat4_rotate_y(&rotate_y, rot->yaw);
        mat4_rotate_z(&rotate_z, rot->roll);
        mat4_translate(&translate, pos->value.x, pos->value.y, pos->value.z);
        mat4_multiply(&model_rot, &rotate_y, &rotate_x);  // Ry·Rx
        mat4_multiply(&model_rot, &rotate_z, &model_rot); // Rz·(Ry·Rx)
        mat4_multiply(&model, &translate, &model_rot);    // T·(Rz·Ry·Rx)

        // Текстура draw'а: слот вне банка → явный unbind (flat-Gouraud —
        // валидный нетекстурированный путь ядра, а не «тихий сбой»).
        if (banks.texture_count != 0u && mesh_render->texture_id < banks.texture_count) {
            Render_BindTexture(banks.textures[mesh_render->texture_id]);
        } else {
            Render_BindTexture(RENDER_TEXTURE_INVALID);
        }

        // Vertex lighting (S17–18, неделя 35): lit-копия вершин в арене
        // кадра; отказ арены — деградация к цветам ассета (не краш).
        const Vertex* draw_vertices = mesh->vertices;
        if (banks.lights != nullptr) {
            Vertex* lit =
                LightingSystem::light_mesh_copy(banks.frame_scratch, mesh, model, *banks.lights);
            if (lit != nullptr) {
                draw_vertices = lit;
            }
        }

        Render_DrawMesh(draw_vertices, mesh->vertex_count, mesh->indices, mesh->index_count,
                        &model);
    }
}

} // namespace game
