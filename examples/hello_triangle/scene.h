// SPDX-License-Identifier: MIT
// RetroPolyEngine — examples/hello_triangle/scene.h
//
// Сцена примера hello_triangle (норматив: docs/plans/sprint_11_12.md,
// недели 22–23): константы поз, загрузка ассетов и сборка ECS-сцены.
// Header ЕДИН для двух потребителей — runtime main.cpp и golden-теста
// (tests/visual/hello_triangle_golden_test.cpp): эталон кадра строится
// тем же кодом, что и играемый кадр (недопущение рассинхронизации
// «двух сцен»).
//
// Потребитель определяет HT_ASSET_DIR (compile-time): CMake-цель примера
// получает ${CMAKE_SOURCE_DIR}/assets/build, тесты — через
// RPE_TEST_SOURCE_DIR; дефолт «.» — прогон из корня репозитория вручную.
#pragma once

#include "assets/assets.h" // MeshAsset/TextureAsset + загрузчики
#include "core/ecs/registry.h"
#include "core/log/log.h" // Log_Message (failure-репорты загрузки)
#include "core/math/fixed.h"
#include "core/math/vec3.h"
#include "hal/platform.h" // FileData/FileSystem_*, Log_Message

#include <stdio.h>

#ifndef HT_ASSET_DIR
#define HT_ASSET_DIR "."
#endif

namespace hello_triangle {

// --- Константы сцены (16.16; источник golden-кадра «default») ------------

/** FOV камеры по вертикали: 60° = π/3 (fixed деление ровное: 68629). */
constexpr fixed_t kCameraFov = FIXED_PI / 3;

/** Плоскости отсечения: near = 1.0, far = 100.0 (как у W0-конформанса). */
constexpr fixed_t kCameraNear = FIXED_ONE;
constexpr fixed_t kCameraFar = INT32_C(100) * FIXED_ONE;

/** Дефолтная поза камеры: (0, 0, 5), yaw/pitch = 0 (взгляд в −Z). */
constexpr fixed_t kCameraEyeZ = INT32_C(5) * FIXED_ONE;

/** Скорость полёта камеры с клавиатуры: 3 единицы/секунду. */
constexpr fixed_t kCameraMoveSpeed = INT32_C(3) * FIXED_ONE;

/** Сенса мыши, rad/пиксель: 0.005 → 327.68 → 327 (16.16, усечение). */
constexpr fixed_t kMouseSensitivity = INT32_C(327);

/** Файлы ассетов примера в каталоге HT_ASSET_DIR. */
const char* const kMeshFileName = "triangle.mesh";
const char* const kTextureFileName = "texture.tex";

/** Хэндлы сцены, возвращённые сборщиком (для иерархии main/тестов). */
struct SceneHandles
{
    Entity camera = INVALID_ENTITY;   /**< Сущность fps-камеры (управление). */
    Entity triangle = INVALID_ENTITY; /**< Сущность треугольника (рендер). */
};

/**
 * Загрузка пары ассетов примера (.mesh + .tex) с валидацией форматов.
 *
 * Файлы читаются FileSystem_LoadFile, разбор — через нормативные
 * mesh_validate_and_load / texture_validate_and_load: арена @p arena
 * растёт ровно на 0 байт при любой malformed-ошибке (контракт §1.2.7 —
 * alloc-after-validate + rollback).
 *
 * @param asset_dir Каталог ассетов (HT_ASSET_DIR).
 * @param arena Persistent-арена результата (жизнь на всю сессию).
 * @param out_mesh Выход: разобранный .mesh.
 * @param out_tex Выход: разобранная .tex (upload в Render_CreateTexture —
 *        задача вызывающего).
 * @param out_error_code Выход: последний AssetDiag (NULL допустим).
 * @return true — оба ассета разобраны; false — ошибка (залогирована
 *         Logger'ом домена «hello_triangle»; содержимое арены НЕ изменено).
 */
inline bool ht_load_assets(const char* asset_dir, LinearAllocator* arena, MeshAsset** out_mesh,
                           TextureAsset** out_tex, uint32_t* out_error_code)
{
    *out_mesh = nullptr;
    *out_tex = nullptr;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", asset_dir, kMeshFileName);
    FileData mesh_file = FileSystem_LoadFile(path);
    if (mesh_file.data == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "файл .mesh не читается: %s", path);
        return false;
    }
    const bool mesh_ok =
        mesh_validate_and_load(mesh_file.data, mesh_file.size, arena, out_mesh, out_error_code);
    FileSystem_FreeFile(&mesh_file);
    if (!mesh_ok) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "валидация .mesh провалена: %s (код %u)",
                    path, (unsigned int)*out_error_code);
        return false;
    }

    snprintf(path, sizeof(path), "%s/%s", asset_dir, kTextureFileName);
    FileData tex_file = FileSystem_LoadFile(path);
    if (tex_file.data == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "файл .tex не читается: %s", path);
        return false;
    }
    const bool tex_ok =
        texture_validate_and_load(tex_file.data, tex_file.size, arena, out_tex, out_error_code);
    FileSystem_FreeFile(&tex_file);
    if (!tex_ok) {
        Log_Message(LOG_LEVEL_ERROR, "hello_triangle", "валидация .tex провалена: %s (код %u)",
                    path, (unsigned int)*out_error_code);
        return false;
    }
    return true;
}

/**
 * Сборка сцены: fps-камера (управляемая) + треугольник (рендер).
 *
 * Состав камеры: Camera(fov 60°, near 1, far 100) + Position(0,0,5) +
 * Rotation(0,0,0) + Velocity(0) + PlayerControl(3 units/s, сенса 327).
 * Треугольник: MeshRenderer(mesh_id 0, texture_id 0) на позе origin.
 *
 * @param registry ECS-реестр (сцена добавляется к текущему состоянию).
 * @return Хэндлы (INVALID-поле — ошибка создания; частичная сборка
 *         возможна только при отказе пулов — бюджета §5.6 хватает).
 */
inline SceneHandles ht_build_scene(Registry& registry)
{
    SceneHandles result;

    const Entity camera = registry.create_entity();
    if (camera == INVALID_ENTITY) {
        return result;
    }
    registry.add_component(camera, Camera{kCameraFov, kCameraNear, kCameraFar});
    registry.add_component(camera, Position{vec3_make(0, 0, kCameraEyeZ)});
    registry.add_component(camera, Rotation{0, 0, 0});
    registry.add_component(camera, Velocity{vec3_make(0, 0, 0)});
    registry.add_component(camera, PlayerControl{kCameraMoveSpeed, kMouseSensitivity});

    const Entity triangle = registry.create_entity();
    if (triangle == INVALID_ENTITY) {
        registry.destroy_entity(camera);
        return result;
    }
    registry.add_component(triangle, MeshRenderer{0u, 0u});
    registry.add_component(triangle, Position{vec3_make(0, 0, 0)});
    registry.add_component(triangle, Rotation{0, 0, 0});

    result.camera = camera;
    result.triangle = triangle;
    return result;
}

} // namespace hello_triangle
