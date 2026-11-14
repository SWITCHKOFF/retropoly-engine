// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/sdk.h
//
// Обнаружение и валидация установленного SDK по sdk_manifest.json
// (Спринт 35–36, неделя 70).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "i18n.h"
#include "project.h"
#include "settings.h"

namespace rpe {
namespace ui {

/** Разобранный и проверенный sdk_manifest.json. */
struct SdkInfo
{
    bool valid = false;
    std::string path; /**< Абсолютный корень SDK. */
    int schema = 0;
    std::string package;
    std::string version;
    std::string git_commit;
    std::string git_describe;
    std::string commit_date;
    bool git_dirty = false;
    Platform platform = Platform::Pc;
    std::string arch;
    std::string os;
    std::string build_type;
    std::string compiler_id;
    std::string compiler_version;
    std::string compiler_banner;
    std::string abi_cxx;
    std::string abi_endianness;
    int pointer_bits = 0;
    std::map<std::string, bool> components;
    std::vector<std::string> examples;
    std::vector<std::string> targets;
    std::string warning; /**< Например, patch-несовпадение UI/SDK. */
    std::string error; /**< Причина invalid. */
    std::string converter_path; /**< tools/asset_converter[.exe]. */
};

/** Результат загрузки SDK. */
struct SdkLoadResult
{
    bool ok = false;
    SdkInfo sdk;
    std::string error;
};

/**
 * Загрузить SDK из каталога (manifest + обязательные файлы/компоненты).
 * Проверяет совместимость версии SDK с UI.
 */
SdkLoadResult loadSdk(const std::string& path, Lang lang);

/** Кандидаты путей SDK для платформы (явные, env, sdk_root). */
std::vector<std::string> candidateSdkPaths(const AppSettings& settings, Platform platform);

/** Найти первый валидный SDK для платформы; пустой path — не найден. */
SdkInfo findSdkForPlatform(const AppSettings& settings, Platform platform, Lang lang);

/** Доступность target'а для UI. */
struct TargetAvailability
{
    Platform platform = Platform::Pc;
    bool available = false;
    std::string sdk_path;
    std::string reason_key; /**< Ключ i18n причины недоступности. */
    std::string reason_detail;
};

/** Проверить доступность PC/PS1/Saturn (SDK + toolchain env). */
std::vector<TargetAvailability> discoverTargets(const AppSettings& settings, Lang lang);

/**
 * Найти исполняемый файл: явный путь из настроек или поиск в PATH.
 *
 * @param configured Явный путь (может быть пустым).
 * @param names Имена для поиска (например, {"cmake", "cmake.exe"}).
 * @return Абсолютный путь или пустая строка.
 */
std::string findExecutable(const std::string& configured, const std::vector<std::string>& names);

/** @return переменная окружения или пустая строка. */
std::string environmentValue(const char* name);

/** Каталог examples для платформы (например, examples/hello_triangle_ps1). */
std::string exampleNameForPlatform(Platform platform);

}  // namespace ui
}  // namespace rpe
