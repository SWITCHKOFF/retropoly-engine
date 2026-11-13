// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/project.h
//
// Модель проекта `.rpeproject` (JSON, схема 1) — Спринт 35–36, неделя 69.
//
// Поля: name, engine_version, sdk_path, target/platform, assets, build_dir,
// run_target, emulator_path. Неизвестные поля и небезопасные пути —
// детерминированная ошибка (не краш).
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "i18n.h"

namespace rpe {
namespace ui {

/** Целевая платформа проекта. */
enum class Platform
{
    Pc,
    Ps1,
    Saturn
};

/** Код платформы для JSON/CLI ("pc"/"ps1"/"saturn"). */
const char* platformCode(Platform platform);
/** Разобрать код платформы. */
bool parsePlatform(const std::string& code, Platform* out);
/** Ключ локализации названия платформы. */
const char* platformNameKey(Platform platform);

/** Тип ассета проекта. */
enum class AssetType
{
    Mesh,
    Texture
};

const char* assetTypeCode(AssetType type);
bool parseAssetType(const std::string& code, AssetType* out);
/** Ключ локализации типа ассета. */
const char* assetTypeNameKey(AssetType type);

/** Запись об импортированном ассете (пути — относительно корня проекта). */
struct AssetEntry
{
    std::string source; /**< assets/source/<file>. */
    std::string output; /**< assets/build/<file>.mesh|.tex. */
    AssetType type = AssetType::Mesh;
    uint64_t size = 0; /**< Размер выходного файла (байт). */
    uint64_t source_size = 0; /**< Размер исходного файла (байт). */
    uint32_t width = 0; /**< Текстура: ширина. */
    uint32_t height = 0; /**< Текстура: высота. */
    uint32_t vertex_count = 0; /**< Меш: число вершин. */
    uint32_t triangle_count = 0; /**< Меш: число треугольников. */
    uint32_t texture_id = 0; /**< Меш: texture_id из заголовка. */
};

/** Модель проекта (схема 1). */
struct Project
{
    int schema = 1;
    std::string name;
    std::string engine_version;
    std::string sdk_path;
    Platform platform = Platform::Pc;
    std::string target; /**< Имя CMake-цели/исполняемого файла. */
    std::vector<AssetEntry> assets;
    std::string build_dir = "build";
    std::string run_target; /**< Переопределение имени запускаемого файла. */
    std::string emulator_path; /**< Переопределение эмулятора (пусто = settings). */
};

/** Важность диагностики валидации. */
enum class IssueSeverity
{
    Warning,
    Error
};

/** Диагностика валидации проекта. */
struct ValidationIssue
{
    IssueSeverity severity = IssueSeverity::Error;
    std::string field;
    std::string message;
};

/** Результат загрузки `.rpeproject`. */
struct ProjectLoadResult
{
    bool ok = false;
    Project project;
    std::vector<ValidationIssue> issues;
    std::string error;
};

/** Каталог проекта по пути к `.rpeproject`. */
std::string projectRoot(const std::string& project_file);
/** Путь `.rpeproject` в каталоге проекта. */
std::string projectFilePath(const std::string& root);

/**
 * Валидировать модель проекта.
 *
 * @param project Проект.
 * @param root Корень проекта (для проверки существования путей).
 * @param check_paths true — проверять существование source/output/sdk.
 * @param lang Язык сообщений.
 * @return Список диагностик (пустой — проект валиден).
 */
std::vector<ValidationIssue> validateProject(const Project& project, const std::string& root,
                                             bool check_paths, Lang lang);

/**
 * Загрузить проект из файла.
 *
 * @param path Путь к `.rpeproject` (или каталогу с ним).
 * @param lang Язык сообщений.
 */
ProjectLoadResult loadProject(const std::string& path, Lang lang);

/**
 * Сохранить проект (атомарно, детерминированный JSON).
 *
 * @param path Путь к `.rpeproject` (или каталогу проекта).
 * @param project Проект.
 * @param error Текст ошибки.
 * @param lang Язык сообщений.
 */
bool saveProject(const std::string& path, const Project& project, std::string* error, Lang lang);

/** Абсолютный путь SDK проекта. */
std::string resolveSdkPath(const std::string& root, const Project& project);
/** Абсолютный каталог сборки проекта. */
std::string resolveBuildDir(const std::string& root, const Project& project);
/** Абсолютный путь source-ассета. */
std::string resolveAssetSource(const std::string& root, const AssetEntry& asset);
/** Абсолютный путь output-ассета. */
std::string resolveAssetOutput(const std::string& root, const AssetEntry& asset);

/** Имя запускаемого файла (target или run_target). */
std::string runTargetName(const Project& project);

/** Создать проект из шаблона в каталоге (без записи на диск). */
Project makeProjectTemplate(const std::string& name, const std::string& engine_version,
                            const std::string& sdk_path, Platform platform,
                            const std::string& target, const std::string& run_target);

}  // namespace ui
}  // namespace rpe
