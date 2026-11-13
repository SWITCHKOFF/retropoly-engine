// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/systems/lighting_system.h
//
// Vertex lighting слоя игры (header-only): ambient + один directional
// свет на fixed 16.16. Норматив: docs/plans/sprint_17_18.md (неделя 35:
// «Vertex lighting: ambient + directional (fixed)»; «Gouraud: per-vertex
// RGB → интерполяция (PC: varying; PS1: GT3 c r/g/b per vertex)»);
// docs/RetroEngine_Engineering_Documentation_v1.43.md §10 (фаза рендера
// кадра) и §1.2.5 (PS1 RGB555+STP-палитра — цвет вершин 8 бит/канал).
//
// МОДЕЛЬ ОСВЕЩЕНИЯ (MVP, детерминированная):
//   color_v = clamp255(ambient + light · max(0, dot(n_world, L)));
//   n_world = R_model · n_local (нормали int8 ×127 → fixed; модель БЕЗ
//   масштаба — вращение сохраняет длину, ренормализация не требуется);
//   L — направление НА источник (GL-конвенция), нормализовано банком.
// Gouraud-интерполяция — конвейер ядра (PC: varying vec4; PS1: POLY_GT3
// с r/g/b per vertex) — система только готовит вершинные цвета.
//
// ПАМЯТЬ: lit-копия вершин — арена кадра (StackAllocator::push; сброс
// reset_frame после Render_EndFrame — контракт §10). Исчерпание арены →
// nullptr → рисуется НЕосвещённый меш (явная деградация, не краш).
#pragma once

#include <retro_engine/assets/assets.h> // MeshAsset, Vertex
#include <retro_engine/core/ecs/registry.h>
#include <retro_engine/core/math/fixed.h>
#include <retro_engine/core/math/mat4.h>
#include <retro_engine/core/math/vec3.h>
#include <retro_engine/core/memory/stack_allocator.h>

namespace game {

/** Фоновая подсветка (ambient): канал 0..255 на компоненту. */
struct AmbientLight
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/**
 * Направленный свет: направление НА источник (GL-конвенция), нормализовано
 * (fixed 16.16); цвет 0..255 на компоненту.
 */
struct DirectionalLight
{
    Vec3 direction;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/** Банк света MVP: ambient + один directional (fixed, детерминированный). */
struct LightBank
{
    AmbientLight ambient;
    DirectionalLight directional;
};

class LightingSystem {
public:
    /** Масштаб нормали int8 → fixed: FIXED_ONE/127 = 65536/127 → 516. */
    static constexpr fixed_t kNormalScale = INT32_C(516);

    /**
     * Lit-копия вершин меша в арене кадра: позиции/UV/нормали — из меша,
     * r/g/b — по модели освещения (нормаль поворачивается моделью @p model,
     * w=0 — перенос не влияет на направление).
     *
     * @param scratch Арена кадра (жизнь копии — до reset_frame кадра).
     * @param mesh Исходный меш (не изменяется).
     * @param model Матрица модели СУБЪЕКТА (T·Rz·Ry·Rx; масштаб запрещён —
     *        нормали без inverse-transpose).
     * @param lights Банк света (ambient + directional).
     * @return Lit-массив vertex_count вершин или nullptr (нет арены/памяти).
     */
    static Vertex* light_mesh_copy(StackAllocator* scratch, const MeshAsset* mesh,
                                   const Mat4& model, const LightBank& lights);
};

inline uint8_t lighting_channel(uint8_t ambient, uint8_t light, fixed_t diffuse)
{
    // light ≤ 255, diffuse ∈ [0..1] fixed → fixed_mul ≤ 255; сумма ≤ 510 →
    // clamp 255 (Gouraud-насыщение — PS1-эквивалент клампа GTE-цвета).
    const int32_t value = (int32_t)ambient + (int32_t)fixed_mul(light, diffuse);
    return (value > 255) ? 255 : (uint8_t)value;
}

inline Vertex* LightingSystem::light_mesh_copy(StackAllocator* scratch, const MeshAsset* mesh,
                                               const Mat4& model, const LightBank& lights)
{
    if (scratch == nullptr || mesh == nullptr || mesh->vertex_count == 0u) {
        return nullptr;
    }
    Vertex* out =
        static_cast<Vertex*>(scratch->push(sizeof(Vertex) * (size_t)mesh->vertex_count));
    if (out == nullptr) {
        return nullptr; // Арена кадра исчерпана — явная деградация (шапка).
    }

    const Vec3 ldir = lights.directional.direction;
    for (uint32_t v = 0; v < mesh->vertex_count; ++v) {
        const Vertex* src = &mesh->vertices[v];
        out[v] = *src;

        // Нормаль: int8 (±127) → fixed (умножение сырых значений —
        // |127·516| < 2^31, без риска насыщения); поворот моделью (w=0).
        const Vec4 n_world = mat4_transform(&model, Vec4{(fixed_t)src->nx * kNormalScale,
                                                         (fixed_t)src->ny * kNormalScale,
                                                         (fixed_t)src->nz * kNormalScale, 0});
        // Диффуз: max(0, dot(n_world, L)); L нормализован банком, n —
        // единичная (вращение без масштаба) → dot ∈ [−1..1] fixed.
        fixed_t diffuse = fixed_add(fixed_add(fixed_mul(n_world.x, ldir.x),
                                              fixed_mul(n_world.y, ldir.y)),
                                    fixed_mul(n_world.z, ldir.z));
        if (diffuse < 0) {
            diffuse = 0; // Грани от света — чистый ambient (без negative).
        }

        out[v].r = lighting_channel(lights.ambient.r, lights.directional.r, diffuse);
        out[v].g = lighting_channel(lights.ambient.g, lights.directional.g, diffuse);
        out[v].b = lighting_channel(lights.ambient.b, lights.directional.b, diffuse);
        // a не трогаем (MVP: transparency off — альфа пост-MVP).
    }
    return out;
}

} // namespace game
