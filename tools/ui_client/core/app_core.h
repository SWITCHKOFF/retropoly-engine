// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/app_core.h
//
// Не-GUI ядро UI client: проект, target, ассеты, сборка/запуск.
// GUI и headless smoke используют один и тот же AppCore (Спринт 35–36).
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "asset_import.h"
#include "command.h"
#include "diagnostics.h"
#include "i18n.h"
#include "process_runner.h"
#include "project.h"
#include "sdk.h"
#include "settings.h"
#include "version.h"

namespace rpe {
namespace ui {

/** Параметры создания проекта. */
struct NewProjectOptions
{
    std::string name;
    std::string location; /**< Родительский каталог. */
    Platform platform = Platform::Pc;
    std::string template_name = "hello_triangle"; /**< hello_triangle|empty. */
    std::string sdk_path; /**< Пусто — автоопределение. */
};

/** Запись recent projects. */
struct RecentProject
{
    std::string path;
    bool exists = false;
};

/** Состояние сборки. */
enum class BuildState
{
    Idle,
    Running,
    Succeeded,
    Failed,
    Cancelled
};

/** Снимок статуса сборки. */
struct BuildStatus
{
    BuildState state = BuildState::Idle;
    int exit_code = 0;
    std::string step;
    std::string error;
};

/** Состояние запуска. */
enum class RunState
{
    Idle,
    Running,
    Exited,
    Failed
};

/** Снимок статуса запуска. */
struct RunStatus
{
    RunState state = RunState::Idle;
    int exit_code = 0;
    uint64_t pid = 0;
    bool cancelled = false;
    std::string error;
};

/** Ядро UI client. */
class AppCore {
public:
    AppCore();
    ~AppCore();

    AppCore(const AppCore&) = delete;
    AppCore& operator=(const AppCore&) = delete;

    /** Загрузить настройки и определить SDK. */
    bool initialize(std::string* error);

    Lang lang() const { return lang_; }
    void setLang(Lang lang);

    const AppSettings& settings() const { return settings_; }
    AppSettings& mutableSettings() { return settings_; }
    bool saveSettings(std::string* error);
    void refreshTargets();

    const std::vector<TargetAvailability>& targets() const { return targets_; }
    /** Явно задать SDK для платформы; сохраняет в настройках. */
    bool setSdkPath(Platform platform, const std::string& path, std::string* error);

    bool hasProject() const { return project_open_; }
    const Project& project() const { return project_; }
    const std::string& projectRootPath() const { return project_root_; }
    std::string projectFile() const;

    bool newProject(const NewProjectOptions& options, std::string* error);
    bool openProject(const std::string& path, std::string* error);
    bool saveProject(std::string* error);
    void closeProject();
    bool setProjectTarget(Platform platform, std::string* error);

    AssetImportResult importAsset(const std::string& source_path);
    bool removeAsset(size_t index, bool delete_output, std::string* error);

    bool startBuild(std::string* error);
    void cancelBuild();
    BuildStatus buildStatus() const;
    std::vector<std::string> takeBuildLog();
    std::vector<std::string> buildCommandPreview() const;

    bool startRun(std::string* error);
    void stopRun();
    RunStatus runStatus() const;
    std::vector<std::string> takeRunLog();
    std::string runCommandPreview() const;

    SdkInfo currentSdk() const;
    DiagnosticsReport diagnostics() const;
    VersionCompatibility compatibility(std::string* detail) const;
    std::vector<RecentProject> recentProjects() const;

    /** Забрать накопленные сообщения ядра (ошибки/предупреждения). */
    std::vector<std::string> takeMessages();

    /** Внутренний лог (для headless smoke). */
    void appendMessage(const std::string& message);

private:
    void runBuildThread(BuildPlan plan);
    void runRunThread(std::vector<std::string> argv, std::string working_directory);
    void waitThreads();

    mutable std::mutex mutex_;
    AppSettings settings_;
    Lang lang_ = Lang::Ru;
    std::vector<TargetAvailability> targets_;

    bool project_open_ = false;
    Project project_;
    std::string project_root_;

    mutable std::mutex build_mutex_;
    BuildStatus build_status_;
    std::vector<std::string> build_log_;
    std::thread build_thread_;
    std::atomic<bool> build_cancel_{false};
    std::atomic<ProcessRunner*> build_process_{nullptr};
    BuildPlan active_build_plan_;

    mutable std::mutex run_mutex_;
    RunStatus run_status_;
    std::vector<std::string> run_log_;
    std::thread run_thread_;
    std::atomic<bool> run_cancel_{false};
    std::atomic<ProcessRunner*> run_process_{nullptr};
    std::vector<std::string> active_run_argv_;
    std::string active_run_cwd_;

    std::vector<std::string> messages_;
};

/** Имя CMake-цели/примера для платформы. */
std::string defaultTargetForPlatform(Platform platform);

}  // namespace ui
}  // namespace rpe
