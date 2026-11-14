// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/diagnostics.cpp
#include "diagnostics.h"

#include "fs_util.h"

namespace rpe {
namespace ui {

namespace {

void add(DiagnosticsReport* report, DiagnosticSeverity severity, const std::string& message)
{
    Diagnostic item;
    item.severity = severity;
    item.message = message;
    item.documentation = "docs/UI_CLIENT.md";
    report->items.push_back(item);
    if (severity == DiagnosticSeverity::Error) report->has_errors = true;
    if (severity == DiagnosticSeverity::Warning) report->has_warnings = true;
}

} // namespace

DiagnosticsReport collectDiagnostics(const Project* project, const std::string& project_root,
                                     const SdkInfo& sdk, const AppSettings& settings, Lang lang)
{
    DiagnosticsReport report;
    const std::string cmake = findExecutable(settings.cmake_path, {"cmake", "cmake.exe"});
    if (cmake.empty()) {
        add(&report, DiagnosticSeverity::Error, tr(lang, "diag.missing_cmake"));
    } else {
        add(&report, DiagnosticSeverity::Ok, "CMake: " + cmake);
    }
    const std::string ninja = findExecutable(settings.ninja_path, {"ninja", "ninja.exe"});
    if (ninja.empty()) {
        add(&report, DiagnosticSeverity::Error, tr(lang, "diag.missing_ninja"));
    } else {
        add(&report, DiagnosticSeverity::Ok, "Ninja: " + ninja);
    }
    if (!sdk.valid) {
        add(&report, DiagnosticSeverity::Error, tr(lang, "diag.missing_sdk"));
    } else {
        add(&report, DiagnosticSeverity::Ok,
            "SDK " + sdk.version + " (" + platformCode(sdk.platform) + "/" + sdk.arch +
                "): " + sdk.path);
        if (!sdk.warning.empty()) {
            add(&report, DiagnosticSeverity::Warning, sdk.warning);
        }
        if (project != nullptr && project->platform == Platform::Pc && sdk.converter_path.empty()) {
            add(&report, DiagnosticSeverity::Error, tr(lang, "diag.missing_converter"));
        }
    }

    if (project != nullptr && !project_root.empty()) {
        if (!isDirectory(project_root)) {
            add(&report, DiagnosticSeverity::Error,
                trf(lang, "diag.project_missing", project_root.c_str()));
        }
        for (size_t i = 0; i < project->assets.size(); ++i) {
            const std::string source = resolveAssetSource(project_root, project->assets[i]);
            const std::string output = resolveAssetOutput(project_root, project->assets[i]);
            if (!isRegularFile(source)) {
                add(&report, DiagnosticSeverity::Warning,
                    trf(lang, "err.path_missing", source.c_str()));
            }
            if (!isRegularFile(output)) {
                add(&report, DiagnosticSeverity::Warning,
                    trf(lang, "err.path_missing", output.c_str()));
            }
        }
    }

    if (project != nullptr && project->platform == Platform::Ps1) {
        if (environmentValue("PSN00BSDK_LIBS").empty()) {
            add(&report, DiagnosticSeverity::Error, tr(lang, "diag.psn00bsdk"));
        }
        const std::string emulator =
            project->emulator_path.empty() ? settings.emulator_ps1 : project->emulator_path;
        if (emulator.empty() || !isRegularFile(emulator)) {
            add(&report, DiagnosticSeverity::Warning, trf(lang, "diag.missing_emulator", "PS1"));
        }
    }
    if (project != nullptr && project->platform == Platform::Saturn) {
        if (environmentValue("JO_ENGINE_ROOT").empty()) {
            add(&report, DiagnosticSeverity::Error, tr(lang, "diag.jo_engine"));
        }
        const std::string emulator =
            project->emulator_path.empty() ? settings.emulator_saturn : project->emulator_path;
        if (emulator.empty() || !isRegularFile(emulator)) {
            add(&report, DiagnosticSeverity::Warning, trf(lang, "diag.missing_emulator", "Saturn"));
        }
    }
    return report;
}

} // namespace ui
} // namespace rpe
