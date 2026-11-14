// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/command.h
//
// Построение команд CMake/запуска (Спринт 35–36, неделя 70).
// Ядро не запускает процессы — только формирует argv; это позволяет
// юнит-тестировать command builder без GUI/сборки.
#pragma once

#include <string>
#include <vector>

#include "i18n.h"
#include "project.h"
#include "sdk.h"
#include "settings.h"

namespace rpe {
namespace ui {

/** План сборки: configure, затем build. */
struct BuildPlan
{
    bool ok = false;
    std::string error;
    std::vector<std::string> configure;
    std::vector<std::string> build;
    std::string build_dir; /**< Абсолютный каталог CMake. */
    std::string target;
};

/** План запуска target'а/эмулятора. */
struct RunPlan
{
    bool ok = false;
    std::string error;
    std::string error_key;
    std::string error_detail;
    std::vector<std::string> argv;
    std::string working_directory;
    bool uses_emulator = false;
    std::string cue_path;
};

/** Эффективный каталог сборки: <project build_dir>/<platform>. */
std::string effectiveBuildDir(const std::string& project_root, const Project& project);

/**
 * Построить план сборки.
 *
 * @param project Проект.
 * @param project_root Корень проекта.
 * @param sdk Валидный SDK для платформы проекта.
 * @param settings Настройки (cmake/ninja).
 * @param lang Язык сообщений.
 */
BuildPlan makeBuildPlan(const Project& project, const std::string& project_root, const SdkInfo& sdk,
                        const AppSettings& settings, Lang lang);

/**
 * Построить план запуска.
 *
 * PC: <build>/<run_target>; PS1/Saturn: .cue в настроенном эмуляторе.
 */
RunPlan makeRunPlan(const Project& project, const std::string& project_root, const SdkInfo& sdk,
                    const AppSettings& settings, Lang lang);

/** Найти исполняемый файл PC-проекта (build dir, bin/, game/). */
std::string findPcExecutable(const std::string& build_dir, const std::string& target);

/** Найти .cue в каталоге сборки/проекта (PS1/Saturn). */
std::string findCueFile(const std::string& project_root, const std::string& build_dir);

}  // namespace ui
}  // namespace rpe
