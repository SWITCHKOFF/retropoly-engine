// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/diagnostics.h
//
// Диагностика окружения UI client (Спринт 35–36, неделя 70):
// CMake/Ninja/SDK/asset_converter/эмулятор/toolchain env.
#pragma once

#include <string>
#include <vector>

#include "i18n.h"
#include "project.h"
#include "sdk.h"
#include "settings.h"

namespace rpe {
namespace ui {

/** Важность диагностики. */
enum class DiagnosticSeverity
{
    Ok,
    Warning,
    Error
};

/** Одна диагностическая запись. */
struct Diagnostic
{
    DiagnosticSeverity severity = DiagnosticSeverity::Ok;
    std::string message;
    std::string documentation; /**< Подсказка раздела документации. */
};

/** Сводная диагностика окружения/проекта. */
struct DiagnosticsReport
{
    bool has_errors = false;
    bool has_warnings = false;
    std::vector<Diagnostic> items;
};

/**
 * Собрать диагностику для текущего проекта/SDK.
 *
 * @param project Проект (может быть nullptr).
 * @param project_root Корень проекта (может быть пустым).
 * @param sdk SDK для платформы проекта (может быть invalid).
 * @param settings Настройки.
 * @param lang Язык.
 */
DiagnosticsReport collectDiagnostics(const Project* project, const std::string& project_root,
                                     const SdkInfo& sdk, const AppSettings& settings, Lang lang);

} // namespace ui
} // namespace rpe
