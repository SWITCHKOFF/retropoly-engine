// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_assets.h
//
// Загрузка .mesh/.tex v1 из read-only DFS в persistent asset arena N64
// (Спринт 41–42, неделя 82). Загрузчики ядра (mesh_validate_and_load/
// texture_validate_and_load) не меняются; N64-слой отвечает за FS,
// staging и rollback арены при malformed/truncated входе.
#pragma once

#include <stdint.h>

#include "assets/assets.h"
#include "hal/n64/n64_fs.h"

namespace n64 {

/** Результат boot-загрузки ассетов (для overlay/лога/тестов). */
struct BootAssets
{
    MeshAsset* mesh;
    TextureAsset* texture;
    uint32_t mesh_file_bytes;
    uint32_t texture_file_bytes;
    bool mesh_ok;
    bool texture_ok;
};

/** Сброс указателей на ассеты (память арены освобождает memory_shutdown). */
void assets_shutdown();

/** @return Состояние последней assets_load_boot(). */
const BootAssets& boot_assets();

/**
 * Загрузка CUBE.MSH + TEX0.TEX из DFS.
 *
 * Ошибка одного ассета не блокирует второй; возвращаемое значение — оба
 * ассета валидны. Malformed/truncated вход не оставляет памяти в арене.
 *
 * @return true — оба ассета загружены и валидны.
 */
bool assets_load_boot();

/**
 * Загрузка одного .mesh из DFS в asset arena.
 *
 * @param path DFS-путь (нормализуется FS-слоем).
 * @param out Выход: валидный меш только при true.
 * @param out_error AssetDiag (NULL допустим).
 * @return true при успехе.
 */
bool assets_load_mesh(const char* path, MeshAsset** out, uint32_t* out_error);

/**
 * Загрузка одного .tex из DFS в asset arena (симметрично assets_load_mesh).
 */
bool assets_load_texture(const char* path, TextureAsset** out, uint32_t* out_error);

}  // namespace n64
