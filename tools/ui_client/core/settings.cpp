// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/settings.cpp
#include "settings.h"

#include <algorithm>
#include <cstdlib>

#include "fs_util.h"
#include "json.h"

namespace rpe {
namespace ui {

namespace {

bool getString(const JsonValue& object, const char* key, std::string* out)
{
    const JsonValue* value = object.find(key);
    if (value == nullptr || !value->isString()) return false;
    *out = value->asString();
    return true;
}

std::string defaultSdkFromEnv(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr ? normalizePath(value) : std::string();
}

} // namespace

std::string settingsFilePath()
{
    return joinPath(configDirectory(), "settings.json");
}

AppSettings loadSettings(std::string* warning)
{
    AppSettings settings;
    if (warning != nullptr) warning->clear();
    const std::string path = settingsFilePath();
    if (!isRegularFile(path)) {
        settings.sdk_root = defaultSdkFromEnv("RPE_SDK_ROOT");
        settings.sdk_pc = defaultSdkFromEnv("RPE_SDK_PC");
        settings.sdk_ps1 = defaultSdkFromEnv("RPE_SDK_PS1");
        settings.sdk_saturn = defaultSdkFromEnv("RPE_SDK_SATURN");
        return settings;
    }
    std::string text;
    std::string error;
    if (!readTextFile(path, &text, &error)) {
        if (warning != nullptr) *warning = error;
        return settings;
    }
    JsonValue root;
    JsonParseError parse_error;
    if (!jsonParse(text, &root, &parse_error) || !root.isObject()) {
        if (warning != nullptr) {
            *warning = "settings.json: " + (parse_error.message.empty() ? std::string("не объект")
                                                                        : parse_error.message);
        }
        return settings;
    }

    int64_t schema = 1;
    const JsonValue* schema_value = root.find("schema");
    if (schema_value != nullptr && schema_value->asInt64(&schema)) {
        settings.schema = static_cast<int>(schema);
    }
    getString(root, "sdk_root", &settings.sdk_root);
    getString(root, "sdk_pc", &settings.sdk_pc);
    getString(root, "sdk_ps1", &settings.sdk_ps1);
    getString(root, "sdk_saturn", &settings.sdk_saturn);
    getString(root, "cmake_path", &settings.cmake_path);
    getString(root, "ninja_path", &settings.ninja_path);
    getString(root, "emulator_ps1", &settings.emulator_ps1);
    getString(root, "emulator_saturn", &settings.emulator_saturn);
    getString(root, "theme", &settings.theme);
    std::string language;
    if (getString(root, "language", &language)) settings.language = parseLang(language);

    const JsonValue* recent = root.find("recent_projects");
    if (recent != nullptr && recent->isArray()) {
        for (size_t i = 0; i < recent->array().size(); ++i) {
            if (recent->array()[i].isString()) {
                const std::string path_value = recent->array()[i].asString();
                if (!path_value.empty()) settings.recent_projects.push_back(path_value);
            }
        }
    }
    return settings;
}

bool saveSettings(const AppSettings& settings, std::string* error)
{
    JsonValue root = JsonValue::makeObject();
    root.append("schema", JsonValue::makeInt(1));
    root.append("sdk_root", JsonValue::makeString(settings.sdk_root));
    root.append("sdk_pc", JsonValue::makeString(settings.sdk_pc));
    root.append("sdk_ps1", JsonValue::makeString(settings.sdk_ps1));
    root.append("sdk_saturn", JsonValue::makeString(settings.sdk_saturn));
    root.append("cmake_path", JsonValue::makeString(settings.cmake_path));
    root.append("ninja_path", JsonValue::makeString(settings.ninja_path));
    root.append("emulator_ps1", JsonValue::makeString(settings.emulator_ps1));
    root.append("emulator_saturn", JsonValue::makeString(settings.emulator_saturn));
    root.append("theme", JsonValue::makeString(settings.theme));
    root.append("language", JsonValue::makeString(langCode(settings.language)));
    JsonValue recent = JsonValue::makeArray();
    for (size_t i = 0; i < settings.recent_projects.size(); ++i) {
        recent.push(JsonValue::makeString(settings.recent_projects[i]));
    }
    root.append("recent_projects", recent);
    return writeTextFileAtomic(settingsFilePath(), jsonSerialize(root), error);
}

void addRecentProject(AppSettings* settings, const std::string& project_file)
{
    if (settings == nullptr || project_file.empty()) return;
    const std::string absolute = absolutePath(project_file);
    std::vector<std::string>& recent = settings->recent_projects;
    recent.erase(std::remove(recent.begin(), recent.end(), absolute), recent.end());
    recent.insert(recent.begin(), absolute);
    if (recent.size() > 10) recent.resize(10);
}

void removeRecentProject(AppSettings* settings, const std::string& project_file)
{
    if (settings == nullptr) return;
    const std::string absolute = absolutePath(project_file);
    std::vector<std::string>& recent = settings->recent_projects;
    recent.erase(std::remove(recent.begin(), recent.end(), absolute), recent.end());
}

std::string sdkPathForPlatform(const AppSettings& settings, int platform_index)
{
    switch (platform_index) {
        case 0:
            return settings.sdk_pc;
        case 1:
            return settings.sdk_ps1;
        case 2:
            return settings.sdk_saturn;
        default:
            return std::string();
    }
}

void setSdkPathForPlatform(AppSettings* settings, int platform_index, const std::string& path)
{
    if (settings == nullptr) return;
    switch (platform_index) {
        case 0:
            settings->sdk_pc = path;
            break;
        case 1:
            settings->sdk_ps1 = path;
            break;
        case 2:
            settings->sdk_saturn = path;
            break;
        default:
            break;
    }
}

} // namespace ui
} // namespace rpe
