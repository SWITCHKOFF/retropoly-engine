// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/asset_import.h
//
// Импорт ассетов через установленный asset_converter (Спринт 35–36,
// неделя 70): .gltf/.glb → .mesh, .png/.jpg → .tex.
#pragma once

#include <string>

#include "i18n.h"
#include "project.h"
#include "sdk.h"

namespace rpe {
namespace ui {

/** Результат импорта одного ассета. */
struct AssetImportResult
{
    bool ok = false;
    std::string error;
    std::string log; /**< Вывод asset_converter (строки через '\n'). */
    std::string output; /**< Абсолютный путь выходного файла. */
    AssetEntry entry;
};

/**
 * Импортировать ассет: скопировать источник в assets/source, вызвать
 * asset_converter и обновить project.assets.
 *
 * @param project_root Корень проекта.
 * @param project Проект (обновляется при успехе).
 * @param sdk Валидный SDK с asset_converter (PC).
 * @param source_path Исходный .gltf/.glb/.png/.jpg/.jpeg.
 * @param lang Язык сообщений.
 */
AssetImportResult importAsset(const std::string& project_root, Project* project, const SdkInfo& sdk,
                              const std::string& source_path, Lang lang);

/**
 * Удалить запись об ассете и (опционально) выходной файл.
 */
bool removeAsset(const std::string& project_root, Project* project, size_t index,
                 bool delete_output, std::string* error, Lang lang);

/**
 * Прочитать заголовок .mesh/.tex и заполнить метаданные.
 *
 * @param path Выходной файл.
 * @param project_root Корень проекта (для относительного пути).
 * @param entry Запись (source/output должны быть уже заполнены).
 */
bool probeAssetFile(const std::string& path, const std::string& project_root, AssetEntry* entry,
                    std::string* error, Lang lang);

/** Тип ассета по расширению источника; false — неподдерживаемое расширение. */
bool assetTypeFromExtension(const std::string& extension, AssetType* out);

}  // namespace ui
}  // namespace rpe
