// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/i18n.cpp
#include "i18n.h"

#include <stdarg.h>
#include <stdio.h>

#include <vector>

namespace rpe {
namespace ui {

namespace {

struct Entry
{
    const char* key;
    const char* ru;
    const char* en;
};

const Entry kEntries[] = {
    {"app.title", "RetroPolyEngine — UI client", "RetroPolyEngine — UI client"},
    {"app.version", "Версия %s", "Version %s"},

    {"menu.file", "Файл", "File"},
    {"menu.new", "Новый проект…", "New Project…"},
    {"menu.open", "Открыть проект…", "Open Project…"},
    {"menu.save", "Сохранить проект", "Save Project"},
    {"menu.close", "Закрыть проект", "Close Project"},
    {"menu.exit", "Выход", "Exit"},
    {"menu.view", "Вид", "View"},
    {"menu.theme", "Тема", "Theme"},
    {"menu.language", "Язык", "Language"},
    {"menu.settings", "Настройки…", "Settings…"},
    {"menu.help", "Справка", "Help"},
    {"menu.docs", "Документация UI client", "UI Client docs"},
    {"menu.about", "О программе", "About"},

    {"theme.dark", "Тёмная", "Dark"},
    {"theme.light", "Светлая", "Light"},

    {"welcome.title", "Добро пожаловать в RetroPolyEngine", "Welcome to RetroPolyEngine"},
    {"welcome.subtitle", "Создайте проект, импортируйте ассеты, соберите и запустите target.",
     "Create a project, import assets, build and run a target."},
    {"welcome.new", "Новый проект", "New Project"},
    {"welcome.open", "Открыть проект", "Open Project"},
    {"welcome.recent", "Недавние проекты", "Recent Projects"},
    {"welcome.no_recent", "Пока нет недавних проектов.", "No recent projects yet."},
    {"welcome.missing", "файл не найден", "file not found"},
    {"welcome.settings", "Настройки", "Settings"},
    {"welcome.sdk_ready", "SDK: %s (%s)", "SDK: %s (%s)"},
    {"welcome.no_sdk", "SDK не найден — задайте путь в настройках.",
     "SDK not found — set a path in Settings."},

    {"wizard.title", "Новый проект", "New Project"},
    {"wizard.name", "Имя проекта", "Project name"},
    {"wizard.location", "Каталог", "Location"},
    {"wizard.target", "Target / платформа", "Target / platform"},
    {"wizard.template", "Шаблон", "Template"},
    {"wizard.sdk", "SDK", "SDK"},
    {"wizard.create", "Создать", "Create"},
    {"wizard.cancel", "Отмена", "Cancel"},
    {"wizard.browse", "Обзор…", "Browse…"},
    {"wizard.name_hint", "Буквы, цифры, '_', '-', '.'; без разделителей пути.",
     "Letters, digits, '_', '-', '.'; no path separators."},
    {"wizard.name_invalid", "Имя проекта содержит недопустимые символы.",
     "Project name contains invalid characters."},
    {"wizard.exists", "Каталог проекта уже существует и не пуст.",
     "Project directory already exists and is not empty."},
    {"wizard.no_sdk", "Для выбранного target не найден SDK.",
     "No SDK found for the selected target."},
    {"wizard.template.hello", "hello_triangle (полный пример)", "hello_triangle (full example)"},
    {"wizard.template.empty", "empty (минимальный каркас)", "empty (minimal skeleton)"},
    {"wizard.sdk_auto", "автоопределение", "auto-detect"},
    {"wizard.creating", "Создание проекта…", "Creating project…"},

    {"target.pc", "PC", "PC"},
    {"target.ps1", "PlayStation 1", "PlayStation 1"},
    {"target.saturn", "Sega Saturn", "Sega Saturn"},
    {"target.unavailable", "недоступен", "unavailable"},
    {"target.reason.no_sdk", "SDK для %s не найден. Укажите путь в настройках.",
     "SDK for %s not found. Set the path in Settings."},
    {"target.reason.psn00bsdk", "Не задан PSN00BSDK_LIBS (PSn00bSDK v0.24).",
     "PSN00BSDK_LIBS is not set (PSn00bSDK v0.24)."},
    {"target.reason.jo_engine", "Не задан JO_ENGINE_ROOT (Jo Engine v2026.2).",
     "JO_ENGINE_ROOT is not set (Jo Engine v2026.2)."},
    {"target.reason.converter", "В SDK отсутствует tools/asset_converter.",
     "SDK has no tools/asset_converter."},
    {"target.reason.platform", "SDK собран для другой платформы (%s).",
     "SDK is built for another platform (%s)."},

    {"project.title", "Проект: %s", "Project: %s"},
    {"project.no_project", "Проект не открыт.", "No project open."},
    {"project.assets", "Ассеты", "Assets"},
    {"project.build", "Сборка", "Build"},
    {"project.run", "Запуск", "Run"},
    {"project.diagnostics", "Диагностика", "Diagnostics"},
    {"project.version", "Версия и совместимость", "Version & compatibility"},
    {"project.target", "Target", "Target"},
    {"project.platform", "Платформа", "Platform"},
    {"project.sdk", "SDK", "SDK"},
    {"project.path", "Путь", "Path"},
    {"project.engine_version", "Версия движка", "Engine version"},
    {"project.build_dir", "Каталог сборки", "Build directory"},
    {"project.run_target", "Запускаемый target", "Run target"},
    {"project.save_ok", "Проект сохранён: %s", "Project saved: %s"},
    {"project.save_failed", "Не удалось сохранить проект: %s", "Failed to save project: %s"},

    {"assets.import", "Импортировать ассеты…", "Import assets…"},
    {"assets.reimport", "Повторить импорт", "Re-import"},
    {"assets.remove", "Удалить", "Remove"},
    {"assets.name", "Имя", "Name"},
    {"assets.type", "Тип", "Type"},
    {"assets.size", "Размер", "Size"},
    {"assets.details", "Параметры", "Details"},
    {"assets.source", "Источник", "Source"},
    {"assets.output", "Выход", "Output"},
    {"assets.empty", "Ассеты ещё не импортированы.", "No assets imported yet."},
    {"assets.importing", "Импорт %s…", "Importing %s…"},
    {"assets.imported", "Импортирован: %s → %s (%s)", "Imported: %s → %s (%s)"},
    {"assets.import_failed", "Ошибка импорта %s: %s", "Import failed for %s: %s"},
    {"assets.removed", "Ассет удалён: %s", "Asset removed: %s"},
    {"assets.mesh", "меш", "mesh"},
    {"assets.texture", "текстура", "texture"},
    {"assets.slot", "слот %u", "slot %u"},
    {"assets.verts", "%u вершин / %u тр.", "%u verts / %u tris"},
    {"assets.dims", "%ux%u", "%ux%u"},
    {"assets.texture_id", "texture_id %u", "texture_id %u"},
    {"assets.confirm_remove", "Удалить ассет и его выходной файл?",
     "Remove the asset and its output file?"},
    {"assets.file_filter", "Ассеты RetroPolyEngine", "RetroPolyEngine assets"},

    {"build.build", "Собрать", "Build"},
    {"build.cancel", "Отменить сборку", "Cancel build"},
    {"build.clear", "Очистить лог", "Clear log"},
    {"build.status", "Статус", "Status"},
    {"build.idle", "Готов к сборке", "Ready to build"},
    {"build.running", "Сборка…", "Building…"},
    {"build.ok", "Сборка завершена (exit 0)", "Build finished (exit 0)"},
    {"build.failed", "Сборка завершилась с ошибкой (exit %d)", "Build failed (exit %d)"},
    {"build.cancelled", "Сборка отменена", "Build cancelled"},
    {"build.log", "Лог сборки", "Build log"},
    {"build.command", "Команда", "Command"},
    {"build.configure", "Конфигурация CMake", "CMake configure"},
    {"build.compile", "Компиляция", "Compile"},
    {"build.no_project", "Откройте проект перед сборкой.", "Open a project before building."},
    {"build.no_sdk", "SDK не найден: сборка невозможна.", "SDK not found: cannot build."},

    {"run.run", "Запустить", "Run"},
    {"run.stop", "Остановить", "Stop"},
    {"run.status", "Статус", "Status"},
    {"run.idle", "Не запущено", "Not running"},
    {"run.running", "Запущено (pid %d)", "Running (pid %d)"},
    {"run.started", "Запуск…", "Starting…"},
    {"run.exited", "Процесс завершён (exit %d)", "Process exited (exit %d)"},
    {"run.stopped", "Остановлено пользователем", "Stopped by user"},
    {"run.command", "Команда запуска", "Run command"},
    {"run.log", "Вывод процесса", "Process output"},
    {"run.no_project", "Откройте проект перед запуском.", "Open a project before running."},
    {"run.no_binary", "Исполняемый файл не найден: %s. Сначала соберите проект.",
     "Executable not found: %s. Build the project first."},
    {"run.no_emulator", "Эмулятор не настроен. Укажите путь в настройках.",
     "Emulator is not configured. Set the path in Settings."},
    {"run.no_cue", "В каталоге сборки не найден .cue образ для %s.",
     "No .cue image found for %s in the build directory."},
    {"run.emulator_missing", "Файл эмулятора не найден: %s", "Emulator file not found: %s"},

    {"diag.title", "Диагностика окружения", "Environment diagnostics"},
    {"diag.ok", "OK", "OK"},
    {"diag.warning", "Предупреждение", "Warning"},
    {"diag.error", "Ошибка", "Error"},
    {"diag.missing_cmake", "CMake не найден. Установите CMake ≥ 3.25 (docs/BUILDING.md).",
     "CMake not found. Install CMake ≥ 3.25 (docs/BUILDING.md)."},
    {"diag.missing_ninja", "Ninja не найден. Установите Ninja (docs/BUILDING.md).",
     "Ninja not found. Install Ninja (docs/BUILDING.md)."},
    {"diag.missing_sdk", "SDK не выбран или не найден.", "SDK is not selected or not found."},
    {"diag.missing_converter",
     "В SDK отсутствует tools/asset_converter — импорт ассетов недоступен.",
     "SDK has no tools/asset_converter — asset import is unavailable."},
    {"diag.missing_emulator", "Эмулятор для %s не настроен.", "Emulator for %s is not configured."},
    {"diag.psn00bsdk", "PSN00BSDK_LIBS не задан: PS1-сборка недоступна.",
     "PSN00BSDK_LIBS is not set: PS1 build unavailable."},
    {"diag.jo_engine", "JO_ENGINE_ROOT не задан: Saturn-сборка недоступна.",
     "JO_ENGINE_ROOT is not set: Saturn build unavailable."},
    {"diag.project_missing", "Каталог проекта не существует: %s",
     "Project directory does not exist: %s"},
    {"diag.docs", "См. docs/UI_CLIENT.md и docs/BUILDING.md",
     "See docs/UI_CLIENT.md and docs/BUILDING.md"},

    {"version.ui", "UI", "UI"},
    {"version.sdk", "SDK", "SDK"},
    {"version.engine_commit", "Commit движка", "Engine commit"},
    {"version.compiler", "Компилятор", "Compiler"},
    {"version.abi", "ABI", "ABI"},
    {"version.schema", "Схема manifest", "Manifest schema"},
    {"version.sdk_platform", "Платформа SDK", "SDK platform"},
    {"version.compatible", "SDK совместим с UI.", "SDK is compatible with the UI."},
    {"version.patch_warning", "Предупреждение: %s", "Warning: %s"},
    {"version.incompatible", "Несовместимый SDK: %s", "Incompatible SDK: %s"},
    {"version.no_sdk", "SDK не выбран.", "No SDK selected."},

    {"settings.title", "Настройки", "Settings"},
    {"settings.sdk_root", "Корень поиска SDK", "SDK search root"},
    {"settings.sdk_pc", "SDK PC", "PC SDK"},
    {"settings.sdk_ps1", "SDK PS1", "PS1 SDK"},
    {"settings.sdk_saturn", "SDK Saturn", "Saturn SDK"},
    {"settings.cmake", "CMake", "CMake"},
    {"settings.ninja", "Ninja", "Ninja"},
    {"settings.emulator_ps1", "Эмулятор PS1 (DuckStation)", "PS1 emulator (DuckStation)"},
    {"settings.emulator_saturn", "Эмулятор Saturn (Mednafen)", "Saturn emulator (Mednafen)"},
    {"settings.theme", "Тема", "Theme"},
    {"settings.language", "Язык", "Language"},
    {"settings.save", "Сохранить", "Save"},
    {"settings.cancel", "Отмена", "Cancel"},
    {"settings.browse", "Обзор…", "Browse…"},
    {"settings.auto", "авто", "auto"},
    {"settings.detected", "найден", "found"},
    {"settings.not_found", "не найден", "not found"},
    {"settings.config_path", "Конфигурация: %s", "Configuration: %s"},
    {"settings.saved", "Настройки сохранены.", "Settings saved."},

    {"common.ok", "ОК", "OK"},
    {"common.error", "Ошибка", "Error"},
    {"common.warning", "Предупреждение", "Warning"},
    {"common.cancel", "Отмена", "Cancel"},
    {"common.close", "Закрыть", "Close"},
    {"common.save", "Сохранить", "Save"},
    {"common.browse", "Обзор…", "Browse…"},
    {"common.yes", "Да", "Yes"},
    {"common.no", "Нет", "No"},
    {"common.unknown", "неизвестно", "unknown"},
    {"common.none", "—", "—"},
    {"common.not_set", "не задано", "not set"},
    {"common.enabled", "включено", "enabled"},
    {"common.disabled", "выключено", "disabled"},

    {"status.ready", "Готово", "Ready"},
    {"status.building", "Сборка…", "Building…"},
    {"status.running", "Запущено", "Running"},

    {"err.project_not_found", "Файл проекта не найден: %s", "Project file not found: %s"},
    {"err.project_read", "Не удалось прочитать проект: %s", "Failed to read project: %s"},
    {"err.project_json", "Ошибка JSON в %s:%d:%d — %s", "JSON error in %s:%d:%d — %s"},
    {"err.root_type", "Корень .rpeproject должен быть JSON-объектом.",
     ".rpeproject root must be a JSON object."},
    {"err.field_required", "Отсутствует обязательное поле '%s'.",
     "Required field '%s' is missing."},
    {"err.field_unknown", "Неизвестное поле '%s'.", "Unknown field '%s'."},
    {"err.field_type", "Поле '%s' имеет неверный тип.", "Field '%s' has an invalid type."},
    {"err.schema", "Неподдерживаемая схема проекта: %d (ожидается 1).",
     "Unsupported project schema: %d (expected 1)."},
    {"err.platform", "Неизвестная платформа '%s' (pc/ps1/saturn).",
     "Unknown platform '%s' (pc/ps1/saturn)."},
    {"err.version", "Некорректная версия движка '%s'.", "Invalid engine version '%s'."},
    {"err.name", "Некорректное имя проекта '%s'.", "Invalid project name '%s'."},
    {"err.path_unsafe", "Небезопасный путь '%s' (ожидается относительный путь внутри проекта).",
     "Unsafe path '%s' (expected a relative path inside the project)."},
    {"err.path_missing", "Путь не найден: %s", "Path not found: %s"},
    {"err.asset_type", "Неизвестный тип ассета '%s'.", "Unknown asset type '%s'."},
    {"err.asset_duplicate", "Дублирующийся выход ассета '%s'.", "Duplicate asset output '%s'."},
    {"err.asset_ext", "Неподдерживаемое расширение '%s' (.gltf/.glb/.png/.jpg/.jpeg).",
     "Unsupported extension '%s' (.gltf/.glb/.png/.jpg/.jpeg)."},
    {"err.sdk_manifest_missing", "sdk_manifest.json не найден в %s",
     "sdk_manifest.json not found in %s"},
    {"err.sdk_manifest_json", "Ошибка JSON в sdk_manifest.json: %s",
     "JSON error in sdk_manifest.json: %s"},
    {"err.sdk_manifest_invalid", "Некорректный sdk_manifest.json: %s",
     "Invalid sdk_manifest.json: %s"},
    {"err.sdk_schema", "Неподдерживаемая схема SDK: %d.", "Unsupported SDK schema: %d."},
    {"err.sdk_component", "В SDK отсутствует компонент '%s'.", "SDK component '%s' is missing."},
    {"err.sdk_version", "SDK несовместим с UI: %s", "SDK is incompatible with the UI: %s"},
    {"err.cmake_missing", "CMake не найден (settings.cmake или PATH).",
     "CMake not found (settings.cmake or PATH)."},
    {"err.ninja_missing", "Ninja не найден (settings.ninja или PATH).",
     "Ninja not found (settings.ninja or PATH)."},
    {"err.converter_missing", "asset_converter не найден в SDK: %s",
     "asset_converter not found in SDK: %s"},
    {"err.source_missing", "Исходный ассет не найден: %s", "Source asset not found: %s"},
    {"err.import_failed", "asset_converter завершился с кодом %d.",
     "asset_converter exited with code %d."},
    {"err.import_output", "Выход asset_converter не прошёл валидацию: %s",
     "asset_converter output failed validation: %s"},
    {"err.project_exists", "Каталог проекта уже существует и не пуст: %s",
     "Project directory already exists and is not empty: %s"},
    {"err.template_missing", "В SDK не найден шаблон/пример: %s",
     "Template/example not found in SDK: %s"},
    {"err.write", "Не удалось записать '%s': %s", "Failed to write '%s': %s"},
    {"err.copy", "Не удалось скопировать '%s': %s", "Failed to copy '%s': %s"},
    {"err.no_project", "Проект не открыт.", "No project open."},
    {"err.build_running", "Сборка уже выполняется.", "A build is already running."},
    {"err.run_running", "Процесс уже запущен.", "A process is already running."},
    {"err.build_no_sdk", "SDK не найден: сборка невозможна.", "SDK not found: cannot build."},
    {"err.build_no_project", "Откройте проект перед сборкой.", "Open a project before building."},
    {"err.run_no_project", "Откройте проект перед запуском.", "Open a project before running."},
    {"err.run_no_binary", "Исполняемый файл не найден: %s", "Executable not found: %s"},
    {"err.run_no_emulator", "Эмулятор не настроен.", "Emulator is not configured."},
    {"err.run_no_cue", "Не найден .cue образ для %s.", "No .cue image found for %s."},
    {"err.run_emulator_missing", "Файл эмулятора не найден: %s", "Emulator file not found: %s"},
    {"err.process_start", "Не удалось запустить процесс '%s'.", "Failed to start process '%s'."},
    {"err.project_unsaved", "Проект не сохранён.", "Project is not saved."},

    {"about.text",
     "RetroPolyEngine UI client %s\nSDL3 + Dear ImGui (MIT)\n\nСоздание проекта, target, импорт "
     "ассетов, сборка и запуск.",
     "RetroPolyEngine UI client %s\nSDL3 + Dear ImGui (MIT)\n\nProject creation, target selection, "
     "asset import, build and run."},
};

} // namespace

Lang parseLang(const std::string& code)
{
    if (code == "en" || code == "EN" || code == "En") return Lang::En;
    return Lang::Ru;
}

const char* langCode(Lang lang)
{
    return lang == Lang::En ? "en" : "ru";
}

const char* tr(Lang lang, const char* key)
{
    if (key == nullptr) return "";
    for (size_t i = 0; i < sizeof(kEntries) / sizeof(kEntries[0]); ++i) {
        if (kEntries[i].key != nullptr && std::string(kEntries[i].key) == key) {
            return lang == Lang::En ? kEntries[i].en : kEntries[i].ru;
        }
    }
    return key;
}

std::string trf(Lang lang, const char* key, ...)
{
    const char* format = tr(lang, key);
    va_list args;
    va_start(args, key);
    char stack_buffer[1024];
    va_list copy;
    va_copy(copy, args);
    const int needed = vsnprintf(stack_buffer, sizeof(stack_buffer), format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return format;
    }
    if (static_cast<size_t>(needed) < sizeof(stack_buffer)) {
        va_end(args);
        return std::string(stack_buffer, static_cast<size_t>(needed));
    }
    std::vector<char> buffer(static_cast<size_t>(needed) + 1u);
    vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);
    return std::string(buffer.data(), static_cast<size_t>(needed));
}

} // namespace ui
} // namespace rpe
