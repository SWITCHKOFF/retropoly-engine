// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/settings.h
//
// Пользовательские настройки UI client (вне репозитория):
//   Linux:   ~/.config/retro-engine-ui/settings.json
//   macOS:   ~/Library/Application Support/RetroPolyEngine/settings.json
//   Windows: %APPDATA%/RetroPolyEngine/settings.json
// RPE_UI_CONFIG_DIR переопределяет каталог (CI/smoke/E2E).
#pragma once

#include <string>
#include <vector>

#include "i18n.h"

namespace rpe {
namespace ui {

/** Настройки UI client. */
struct AppSettings
{
    int schema = 1;
    std::string sdk_root;       /**< Каталог поиска SDK (pc/, ps1/, saturn/). */
    std::string sdk_pc;         /**< Явный путь к PC SDK. */
    std::string sdk_ps1;        /**< Явный путь к PS1 SDK. */
    std::string sdk_saturn;     /**< Явный путь к Saturn SDK. */
    std::string cmake_path;     /**< Пусто — искать в PATH. */
    std::string ninja_path;     /**< Пусто — искать в PATH. */
    std::string emulator_ps1;   /**< DuckStation (или совместимый). */
    std::string emulator_saturn;/**< Mednafen. */
    std::string theme = "dark"; /**< dark|light. */
    Lang language = Lang::Ru;
    std::vector<std::string> recent_projects; /**< Абсолютные пути к .rpeproject. */
};

/** Путь settings.json. */
std::string settingsFilePath();

/**
 * Загрузить настройки; отсутствие файла — defaults без warning.
 * Повреждённый файл не роняет UI: возвращаются defaults + warning.
 */
AppSettings loadSettings(std::string* warning);

/** Сохранить настройки атомарно. */
bool saveSettings(const AppSettings& settings, std::string* error);

/** Добавить проект в recent (уникально, максимум 10, абсолютный путь). */
void addRecentProject(AppSettings* settings, const std::string& project_file);

/** Удалить проект из recent. */
void removeRecentProject(AppSettings* settings, const std::string& project_file);

/** Явный путь SDK для платформы ("" если не задан). */
std::string sdkPathForPlatform(const AppSettings& settings, int platform_index);

/** Изменить путь SDK для платформы (0=pc, 1=ps1, 2=saturn). */
void setSdkPathForPlatform(AppSettings* settings, int platform_index, const std::string& path);

}  // namespace ui
}  // namespace rpe
