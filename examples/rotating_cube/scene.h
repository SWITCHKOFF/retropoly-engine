// SPDX-License-Identifier: MIT
// RetroPolyEngine — examples/rotating_cube/scene.h
//
// Сцена примера rotating_cube — демо MVP, веха M7 (норматив:
// docs/plans/sprint_17_18.md, неделя 33): W1-сцена — 4 видимых куба
// (48 текстурированных треугольников), фиксированная камера (эталонная
// расстановка), загрузка меша и ЧЕТЫРЁХ текстур (по одной на куб; ≤ 1
// текстуры 256×256 на куб), CI8/256, Gouraud, transparency off.
//
// Header ЕДИН для двух потребителей — runtime main.cpp и golden-теста
// (tests/visual/rotating_cube_golden_test.cpp): эталон кадра строится тем
// же кодом, что и играемый кадр (недопущение рассинхронизации «двух
// сцен») — паттерн hello_triangle/scene.h (S11–12).
//
// Потребитель определяет RC_ASSET_DIR (compile-time): CMake-цель примера
// получает ${CMAKE_SOURCE_DIR}/assets/build, тесты — через
// RPE_TEST_SOURCE_DIR; дефолт «.» — прогон из корня репозитория вручную.
#pragma once

#include "assets/assets.h" // MeshAsset/TextureAsset + загрузчики
#include "core/ecs/registry.h"
#include "core/log/log.h" // Log_Message (failure-репорты загрузки)
#include "core/math/fixed.h"
#include "core/math/vec3.h"
#include "game/systems/lighting_system.h" // LightBank (банк света W1)
#include "hal/platform.h" // FileData/FileSystem_*, Input_IsKeyDown/IsPadButtonDown

#include <stdio.h>

#ifndef RC_ASSET_DIR
#define RC_ASSET_DIR "."
#endif

namespace rotating_cube {

// --- Константы W1-сцены (16.16; эталонная расстановка недели 33) ----------

/** FOV камеры по вертикали: 70° (запас от clip-reject верхушек башен). */
constexpr fixed_t kCameraFov = INT32_C(80067); // 70°·π/180 = 1.2217305 → 80067

/** Плоскости отсечения: near = 1.0, far = 100.0 (как у W1-полки §7.5). */
constexpr fixed_t kCameraNear = FIXED_ONE;
constexpr fixed_t kCameraFar = INT32_C(100) * FIXED_ONE;

/** Эталонная поза камеры: (0, 3.2, 7), pitch −0.15 rad (цент башен в кадре). */
constexpr fixed_t kCameraEyeY = INT32_C(3) * FIXED_ONE + FIXED_ONE / 5; // 3.2
constexpr fixed_t kCameraEyeZ = INT32_C(7) * FIXED_ONE;                 // 7.0
constexpr fixed_t kCameraPitch = -INT32_C(9830); // −0.15·65536 = −9830.4 → −9830

/** Полуразмер куба: источник cube.gltf — куб ±1 (полный размер 2.0). */
constexpr fixed_t kCubeHalfExtent = FIXED_ONE;

/**
 * Эталонная расстановка W1: ДВЕ БАШНИ по 2 куба (контакт куб-куб виден
 * сразу — верхние кубы стоят на нижних; прыжок поднимает башни синхронно).
 * Башни X = ±2.2 (зазор между ними 2.4), Z = 0; Y: низ 1.0 (пол y=0),
 * верх 3.0 (ровно на нижнем кубе: его верх y=2.0 + полуразмер 1.0).
 * Индексация: cube0 — низ левой, cube1 — верх левой, cube2 — низ правой,
 * cube3 — верх правой.
 */
const fixed_t kCubeCenterX[4] = {-INT32_C(144179), -INT32_C(144179), INT32_C(144179),
                                 INT32_C(144179)}; // ±2.2 (144179.2 → 144179)
const fixed_t kCubeCenterY[4] = {FIXED_ONE, INT32_C(3) * FIXED_ONE, FIXED_ONE,
                                 INT32_C(3) * FIXED_ONE};
const fixed_t kCubeCenterZ[4] = {0, 0, 0, 0};

/**
 * Файлы ассетов примера в каталоге RC_ASSET_DIR. Меш ОДИН (cube.mesh) —
 * W1-геометрия однородна, 4 куба ссылаются на него через MeshRenderer::
 * mesh_id 0 (RAM-бюджет: дублирование геометрии не нужно); текстуры —
 * по варианту на куб (texture_0.tex .. texture_3.tex, CI8 256×256).
 */
const char* const kMeshFileName = "cube.mesh";
const int kTextureCount = 4;
const char* const kTextureFileNames[4] = {"texture_0.tex", "texture_1.tex", "texture_2.tex",
                                          "texture_3.tex"};

/** Гравитация демо (неделя 34): −9.8 единицы/секунду² (16.16, усечение). */
constexpr fixed_t kGravity = -INT32_C(642253); // −9.8·65536 = −642252.8 → −642253

/** Скорость прыжка (импульс вверх): +4.9 единицы/секунду (подъём 1.225 —
 *  верхние кубы не выходят за верхнюю кромку кадра FOV 70°). */
constexpr fixed_t kJumpSpeed = INT32_C(321126); // 4.9·65536 = 321126.4 → 321126

// --- Банк света W1 (неделя 35; эталон сцены, един для main и golden) -------

/**
 * Эталонный свет демки: свет с верха-лева-фронта (направление НА источник,
 * GL-конвенция; нормализовано), тёплый directional + холодноватый ambient —
 * тёмные грани читаются, пересветов нет (clamp 255 не задействован).
 *
 * L: (−0.4, 0.8, 0.45)/|L| — |L| = 1.00125: (−0.3995, 0.7990, 0.4494) →
 * (−26188, 52366, 29458) 16.16 (усечение).
 */
inline const game::LightBank& rc_light_bank()
{
    static const game::LightBank kBank = {
        game::AmbientLight{64, 64, 72}, // ambient
        game::DirectionalLight{vec3_make(-INT32_C(26188), INT32_C(52366), INT32_C(29458)),
                               200, 200, 190}, // directional
    };
    return kBank;
}

/** Хэндлы сцены, возвращённые сборщиком (для иерархии main/тестов). */
struct SceneHandles
{
    Entity camera = INVALID_ENTITY;          /**< Сущность камеры (фиксированная). */
    Entity ground = INVALID_ENTITY;          /**< Статический пол (Collider, без Velocity). */
    Entity cubes[4] = {INVALID_ENTITY, INVALID_ENTITY, INVALID_ENTITY, INVALID_ENTITY};
};

/**
 * Загрузка ассетов примера (1 .mesh + 4 .tex) с валидацией форматов.
 *
 * Файлы читаются FileSystem_LoadFile, разбор — через нормативные
 * mesh_validate_and_load / texture_validate_and_load: арена @p arena
 * растёт ровно на загруженный контент (malformed-ошибка на середине
 * откатывает ТОЛЬКО её собственный marker — ранее загруженные ассеты
 * живут; упавший вариант — фатал с диагностикой, сцена не собирается).
 *
 * @param asset_dir Каталог ассетов (RC_ASSET_DIR).
 * @param arena Persistent-арена результата (жизнь на всю сессию).
 * @param out_mesh Выход: разобранный .mesh.
 * @param out_tex Выход: массив из 4 разобранных .tex (upload в
 *        Render_CreateTexture — задача вызывающего).
 * @param out_error_code Выход: последний AssetDiag (NULL допустим).
 * @return true — все 5 ассетов разобраны; false — ошибка (залогирована).
 */
inline bool rc_load_assets(const char* asset_dir, LinearAllocator* arena, MeshAsset** out_mesh,
                           TextureAsset** out_tex, uint32_t* out_error_code)
{
    *out_mesh = nullptr;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", asset_dir, kMeshFileName);
    FileData mesh_file = FileSystem_LoadFile(path);
    if (mesh_file.data == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "rotating_cube", "файл .mesh не читается: %s", path);
        return false;
    }
    const bool mesh_ok =
        mesh_validate_and_load(mesh_file.data, mesh_file.size, arena, out_mesh, out_error_code);
    FileSystem_FreeFile(&mesh_file);
    if (!mesh_ok) {
        Log_Message(LOG_LEVEL_ERROR, "rotating_cube", "валидация .mesh провалена: %s (код %u)",
                    path, (unsigned int)*out_error_code);
        return false;
    }

    for (int i = 0; i < kTextureCount; ++i) {
        snprintf(path, sizeof(path), "%s/%s", asset_dir, kTextureFileNames[i]);
        FileData tex_file = FileSystem_LoadFile(path);
        if (tex_file.data == nullptr) {
            Log_Message(LOG_LEVEL_ERROR, "rotating_cube", "файл .tex не читается: %s", path);
            return false;
        }
        const bool tex_ok = texture_validate_and_load(tex_file.data, tex_file.size, arena,
                                                      &out_tex[i], out_error_code);
        FileSystem_FreeFile(&tex_file);
        if (!tex_ok) {
            Log_Message(LOG_LEVEL_ERROR, "rotating_cube",
                        "валидация .tex провалена: %s (код %u)", path, (unsigned int)*out_error_code);
            return false;
        }
    }
    return true;
}

/**
 * Сборка сцены W1: фиксированная камера + 4 куба двумя башнями (эталонная
 * расстановка — верхние кубы стоят на нижних: контакт куб-куб в кадре с
 * первого тика). MeshRenderer: mesh_id 0 (общий .mesh), texture_id 0..3
 * (по варианту на куб). Rotation нулевая — RotationSystem стартует с 0;
 * Velocity нулевая — гравитация демо включается в цикле.
 *
 * Состав камеры: Camera(fov 70°, near 1, far 100) + Position(0, 3.2, 7) +
 * Rotation(0, pitch −0.15, 0). БЕЗ PlayerControl: камера фиксированная
 * (эталонная расстановка W1 — управление недоступно по плану демки).
 *
 * @param registry ECS-реестр (сцена добавляется к текущему состоянию).
 * @return Хэндлы (INVALID-поле — ошибка создания; частичная сборка
 *         возможна только при отказе пулов — бюджета §5.6 хватает).
 */
inline SceneHandles rc_build_scene(Registry& registry)
{
    SceneHandles result;

    const Entity camera = registry.create_entity();
    if (camera == INVALID_ENTITY) {
        return result;
    }
    registry.add_component(camera, Camera{kCameraFov, kCameraNear, kCameraFar});
    registry.add_component(camera, Position{vec3_make(0, kCameraEyeY, kCameraEyeZ)});
    registry.add_component(camera, Rotation{0, kCameraPitch, 0});

    // Статический пол: Collider БЕЗ Velocity (семантика статики системы
    // коллизий). Локальный AABB при Position(0,0,0): верхняя грань y=0 —
    // нижние кубы стоят центром на y=1 (полуразмер 1.0).
    const Entity ground = registry.create_entity();
    if (ground == INVALID_ENTITY) {
        registry.destroy_entity(camera);
        return result;
    }
    registry.add_component(ground, Position{vec3_make(0, 0, 0)});
    registry.add_component(
        ground,
        Collider{vec3_make(-INT32_C(64) * FIXED_ONE, -INT32_C(64) * FIXED_ONE,
                           -INT32_C(64) * FIXED_ONE),
                 vec3_make(INT32_C(64) * FIXED_ONE, 0, INT32_C(64) * FIXED_ONE)});
    result.ground = ground;

    for (int i = 0; i < 4; ++i) {
        const Entity cube = registry.create_entity();
        if (cube == INVALID_ENTITY) {
            registry.destroy_entity(camera);
            for (int k = 0; k < 4; ++k) {
                if (result.cubes[k] != INVALID_ENTITY) {
                    registry.destroy_entity(result.cubes[k]);
                }
            }
            return result;
        }
        registry.add_component(cube, MeshRenderer{(uint32_t)0u, (uint32_t)i});
        registry.add_component(cube, Position{vec3_make(kCubeCenterX[i], kCubeCenterY[i],
                                                        kCubeCenterZ[i])});
        registry.add_component(cube, Rotation{0, 0, 0});
        registry.add_component(cube, Velocity{vec3_make(0, 0, 0)});
        // Collider — локальный AABB относительно Position (полуразмеры):
        // мировой куб коллизий = Position ± kCubeHalfExtent (неделя 34).
        registry.add_component(
            cube, Collider{vec3_make(-kCubeHalfExtent, -kCubeHalfExtent, -kCubeHalfExtent),
                           vec3_make(kCubeHalfExtent, kCubeHalfExtent, kCubeHalfExtent)});
        result.cubes[i] = cube;
    }

    result.camera = camera;
    return result;
}

/**
 * Гравитация и прыжок демо (неделя 34, demo-level — НЕ полноценная физика):
 * фаза GameUpdate тика. Цель — все сущности с Velocity (в сцене W1 это
 * ровно 4 куба: у камеры Velocity нет). Каждому: Velocity.y −=
 * |kGravity|·dt (гравитация — всегда вниз); по запросу прыжка — импульс
 * +kJumpSpeed вверх (всем синхронно; удержание кнопки даёт непрерывные
 * подпрыгивания — осознанное demo-level поведение без grounded-автомата).
 *
 * @param registry ECS-реестр сцены.
 * @param dt Длина тика (16.16, секунды).
 * @param jump_requested true — импульс прыжка в этом тике (Space/Cross).
 */
inline void rc_gravity_and_jump(Registry& registry, fixed_t dt, bool jump_requested)
{
    for (auto row : registry.view<Velocity>()) {
        Velocity* velocity = &row.get<Velocity>();
        // Гравитация: v.y += g·dt (g отрицательная — константа выше).
        velocity->value.y = fixed_add(velocity->value.y, fixed_mul(kGravity, dt));
        if (jump_requested) {
            velocity->value.y = kJumpSpeed;
        }
    }
}

} // namespace rotating_cube
