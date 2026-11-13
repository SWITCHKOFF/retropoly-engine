// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/app_core.cpp
#include "app_core.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "fs_util.h"

namespace rpe {
namespace ui {

namespace fs = std::filesystem;

namespace {

const char* const kGenericMain =
    "// SPDX-License-Identifier: MIT\n"
    "// RetroPolyEngine project skeleton (UI client).\n"
    "#include <retro_engine/retro_engine.h>\n"
    "\n"
    "int main()\n"
    "{\n"
    "    if (!Platform_Init()) return 1;\n"
    "    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {\n"
    "        Platform_Shutdown();\n"
    "        return 1;\n"
    "    }\n"
    "    while (Platform_PumpEvents()) {\n"
    "        Render_BeginFrame();\n"
    "        Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G,\n"
    "                     RENDER_CLEAR_DEFAULT_B);\n"
    "        Render_OverlayText(8, 8, 255, 255, 255, 1, \"RETRO SDK\");\n"
    "        Render_EndFrame();\n"
    "        Platform_SwapBuffers();\n"
    "    }\n"
    "    Render_Shutdown();\n"
    "    Platform_Shutdown();\n"
    "    return 0;\n"
    "}\n";

const char* const kGenericMainSaturn =
    "// SPDX-License-Identifier: MIT\n"
    "// RetroPolyEngine project skeleton (UI client, Saturn).\n"
    "#include <retro_engine/retro_engine.h>\n"
    "\n"
    "extern \"C\" void rpe_example_add_callback(void (*callback)(void));\n"
    "extern \"C\" void rpe_example_run(void);\n"
    "\n"
    "namespace {\n"
    "void frame_tick(void)\n"
    "{\n"
    "    if (!Platform_PumpEvents()) return;\n"
    "    Render_BeginFrame();\n"
    "    Render_Clear(RENDER_CLEAR_DEFAULT_R, RENDER_CLEAR_DEFAULT_G,\n"
    "                 RENDER_CLEAR_DEFAULT_B);\n"
    "    Render_OverlayText(8, 8, 255, 255, 255, 1, \"RETRO SDK\");\n"
    "    Render_EndFrame();\n"
    "    Platform_SwapBuffers();\n"
    "}\n"
    "} // namespace\n"
    "\n"
    "extern \"C\" void jo_main(void)\n"
    "{\n"
    "    Platform_Init();\n"
    "    Render_Init(320u, 224u);\n"
    "    rpe_example_add_callback(frame_tick);\n"
    "    rpe_example_run();\n"
    "}\n";

std::string replaceAll(std::string text, const std::string& from, const std::string& to)
{
    if (from.empty()) return text;
    size_t position = 0;
    while ((position = text.find(from, position)) != std::string::npos) {
        text.replace(position, from.size(), to);
        position += to.size();
    }
    return text;
}

bool validProjectName(const std::string& name)
{
    if (name.empty() || name.size() > 64 || name == "." || name == "..") return false;
    if (name.front() == '.') return false;
    for (size_t i = 0; i < name.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (std::isalnum(c) == 0 && c != '_' && c != '-' && c != '.') return false;
    }
    return true;
}

std::string exampleDirectory(const SdkInfo& sdk)
{
    return joinPath(sdk.path, "examples/" + exampleNameForPlatform(sdk.platform));
}

std::string patchedCMake(std::string text, Platform platform)
{
    if (platform == Platform::Pc) {
        text = replaceAll(text, "add_executable(hello_triangle main.cpp)",
                          "add_executable(hello_triangle src/main.cpp)");
        text = replaceAll(text, "HT_ASSET_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\"",
                          "HT_ASSET_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets/build\"");
        text = replaceAll(text,
                          "target_include_directories(hello_triangle PRIVATE "
                          "\"${CMAKE_CURRENT_SOURCE_DIR}/../..\")",
                          "target_include_directories(hello_triangle PRIVATE "
                          "\"${CMAKE_CURRENT_SOURCE_DIR}/src\")");
    } else if (platform == Platform::Ps1) {
        text = replaceAll(text, "add_executable(hello_triangle_ps1 GPREL main.cpp)",
                          "add_executable(hello_triangle_ps1 GPREL src/main.cpp)");
    } else {
        text = replaceAll(text, "add_executable(hello_triangle_saturn main.cpp jo_bridge.c)",
                          "add_executable(hello_triangle_saturn src/main.cpp src/jo_bridge.c)");
    }
    return text;
}

std::string projectReadme(const Project& project)
{
    std::ostringstream out;
    out << "# " << project.name << "\n\n";
    out << "Проект RetroPolyEngine, созданный UI client.\n\n";
    out << "- target: `" << project.target << "`\n";
    out << "- platform: `" << platformCode(project.platform) << "`\n";
    out << "- engine/SDK: `" << project.engine_version << "`\n";
    out << "- SDK path: `" << project.sdk_path << "`\n\n";
    out << "## Сборка\n\n";
    out << "```sh\ncmake -S . -B " << project.build_dir << "/" << platformCode(project.platform)
        << " -G Ninja " << "-DCMAKE_PREFIX_PATH=<sdk>\n";
    out << "cmake --build " << project.build_dir << "/" << platformCode(project.platform)
        << " --target " << project.target << "\n";
    out << "```\n\n";
    out << "Для PS1/Saturn добавьте `-DCMAKE_TOOLCHAIN_FILE=<sdk>/cmake/...` "
           "(UI client делает это автоматически).\n";
    return out.str();
}

int runLines(const std::vector<std::string>& argv, const std::string& working_directory,
             const std::string& step, std::atomic<bool>* cancel_flag,
             std::atomic<ProcessRunner*>* process_slot, std::mutex* log_mutex,
             std::vector<std::string>* log, bool* was_cancelled)
{
    if (was_cancelled != nullptr) *was_cancelled = false;
    ProcessRunner runner;
    ProcessSpec spec;
    spec.argv = argv;
    spec.working_directory = working_directory;
    std::string start_error;
    {
        std::lock_guard<std::mutex> lock(*log_mutex);
        log->push_back("[" + step + "] $ " + commandLineToString(argv));
    }
    if (!runner.start(spec, &start_error)) {
        std::lock_guard<std::mutex> lock(*log_mutex);
        log->push_back("[" + step + "] не удалось запустить процесс: " + start_error);
        return -1;
    }
    process_slot->store(&runner);
    while (runner.running()) {
        if (cancel_flag->load()) runner.cancel();
        std::vector<std::string> lines = runner.takeLines();
        if (!lines.empty()) {
            std::lock_guard<std::mutex> lock(*log_mutex);
            log->insert(log->end(), lines.begin(), lines.end());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::vector<std::string> rest = runner.takeLines();
    if (!rest.empty()) {
        std::lock_guard<std::mutex> lock(*log_mutex);
        log->insert(log->end(), rest.begin(), rest.end());
    }
    runner.wait(0);
    process_slot->store(nullptr);
    if (was_cancelled != nullptr) *was_cancelled = runner.cancelled();
    {
        std::lock_guard<std::mutex> lock(*log_mutex);
        log->push_back("[" + step + "] exit " + std::to_string(runner.exitCode()));
    }
    return runner.exitCode();
}

} // namespace

std::string defaultTargetForPlatform(Platform platform)
{
    switch (platform) {
        case Platform::Pc:
            return "hello_triangle";
        case Platform::Ps1:
            return "hello_triangle_ps1";
        case Platform::Saturn:
            return "hello_triangle_saturn";
    }
    return "hello_triangle";
}

AppCore::AppCore()
{
}

AppCore::~AppCore()
{
    build_cancel_.store(true);
    run_cancel_.store(true);
    ProcessRunner* build_process = build_process_.load();
    if (build_process != nullptr) build_process->cancel();
    ProcessRunner* run_process = run_process_.load();
    if (run_process != nullptr) run_process->cancel();
    waitThreads();
}

bool AppCore::initialize(std::string* error)
{
    std::string warning;
    settings_ = loadSettings(&warning);
    lang_ = settings_.language;
    if (!warning.empty()) appendMessage(warning);
    refreshTargets();
    if (error != nullptr) error->clear();
    return true;
}

void AppCore::setLang(Lang lang)
{
    lang_ = lang;
    settings_.language = lang;
    refreshTargets();
}

bool AppCore::saveSettings(std::string* error)
{
    const bool ok = rpe::ui::saveSettings(settings_, error);
    if (ok) refreshTargets();
    return ok;
}

void AppCore::refreshTargets()
{
    targets_ = discoverTargets(settings_, lang_);
}

bool AppCore::setSdkPath(Platform platform, const std::string& path, std::string* error)
{
    const SdkLoadResult loaded = loadSdk(path, lang_);
    if (!loaded.ok) {
        if (error != nullptr) *error = loaded.error;
        return false;
    }
    if (loaded.sdk.platform != platform) {
        if (error != nullptr) {
            *error = trf(
                lang_, "err.sdk_manifest_invalid",
                (std::string("platform mismatch: ") + platformCode(loaded.sdk.platform)).c_str());
        }
        return false;
    }
    setSdkPathForPlatform(&settings_, static_cast<int>(platform), loaded.sdk.path);
    if (project_open_ && project_.platform == platform) {
        project_.sdk_path = loaded.sdk.path;
        project_.engine_version = loaded.sdk.version;
    }
    refreshTargets();
    std::string save_error;
    saveSettings(&save_error);
    return true;
}

std::string AppCore::projectFile() const
{
    if (!project_open_) return std::string();
    return projectFilePath(project_root_);
}

bool AppCore::newProject(const NewProjectOptions& options, std::string* error)
{
    if (!validProjectName(options.name)) {
        if (error != nullptr) *error = trf(lang_, "err.name", options.name.c_str());
        return false;
    }
    const std::string parent = options.location.empty() ? "." : options.location;
    if (!createDirectories(parent, error)) return false;

    SdkInfo sdk;
    if (!options.sdk_path.empty()) {
        const SdkLoadResult loaded = loadSdk(options.sdk_path, lang_);
        if (!loaded.ok) {
            if (error != nullptr) *error = loaded.error;
            return false;
        }
        sdk = loaded.sdk;
    } else {
        sdk = findSdkForPlatform(settings_, options.platform, lang_);
    }
    if (!sdk.valid || sdk.platform != options.platform) {
        if (error != nullptr) {
            *error = sdk.error.empty() ? trf(lang_, "target.reason.no_sdk",
                                             tr(lang_, platformNameKey(options.platform)))
                                       : sdk.error;
        }
        return false;
    }

    const std::string project_dir = joinPath(parent, options.name);
    if (pathExists(project_dir) && !listDirectorySorted(project_dir).empty()) {
        if (error != nullptr) *error = trf(lang_, "err.project_exists", project_dir.c_str());
        return false;
    }
    if (!createDirectories(project_dir, error) ||
        !createDirectories(joinPath(project_dir, "assets/source"), error) ||
        !createDirectories(joinPath(project_dir, "assets/build"), error) ||
        !createDirectories(joinPath(project_dir, "src"), error) ||
        !createDirectories(joinPath(project_dir, "build"), error)) {
        return false;
    }

    const std::string example = exampleDirectory(sdk);
    std::string cmake_text;
    std::string read_error;
    if (!readTextFile(joinPath(example, "CMakeLists.txt"), &cmake_text, &read_error)) {
        if (error != nullptr) *error = trf(lang_, "err.template_missing", example.c_str());
        return false;
    }
    if (!writeTextFileAtomic(joinPath(project_dir, "CMakeLists.txt"),
                             patchedCMake(cmake_text, options.platform), error)) {
        return false;
    }

    const bool empty_template = options.template_name == "empty";
    if (empty_template) {
        const std::string main_text =
            options.platform == Platform::Saturn ? kGenericMainSaturn : kGenericMain;
        if (!writeTextFileAtomic(joinPath(project_dir, "src/main.cpp"), main_text, error)) {
            return false;
        }
    } else {
        if (!copyFile(joinPath(example, "main.cpp"), joinPath(project_dir, "src/main.cpp"),
                      error)) {
            if (error != nullptr) *error = trf(lang_, "err.template_missing", example.c_str());
            return false;
        }
        if (options.platform == Platform::Pc) {
            std::string main_text;
            if (readTextFile(joinPath(project_dir, "src/main.cpp"), &main_text, &read_error)) {
                main_text = replaceAll(main_text, "#include \"examples/hello_triangle/scene.h\"",
                                       "#include \"scene.h\"");
                writeTextFileAtomic(joinPath(project_dir, "src/main.cpp"), main_text, nullptr);
            }
            const std::string scene = joinPath(example, "scene.h");
            if (isRegularFile(scene)) {
                copyFile(scene, joinPath(project_dir, "src/scene.h"), nullptr);
            }
            const std::string assets = joinPath(example, "assets");
            if (isDirectory(assets)) {
                const std::vector<std::string> files = listDirectorySorted(assets);
                for (size_t i = 0; i < files.size(); ++i) {
                    const std::string source = joinPath(assets, files[i]);
                    if (isRegularFile(source)) {
                        copyFile(source, joinPath(project_dir, "assets/build/" + files[i]),
                                 nullptr);
                    }
                }
            }
        }
    }
    if (options.platform == Platform::Saturn) {
        if (!copyFile(joinPath(example, "jo_bridge.c"), joinPath(project_dir, "src/jo_bridge.c"),
                      error)) {
            return false;
        }
    }

    const std::string target = defaultTargetForPlatform(options.platform);
    Project project =
        makeProjectTemplate(options.name, sdk.version, sdk.path, options.platform, target, target);
    project.assets.clear();
    if (!writeTextFileAtomic(joinPath(project_dir, "README.md"), projectReadme(project), error)) {
        return false;
    }
    if (!rpe::ui::saveProject(projectFilePath(project_dir), project, error, lang_)) return false;

    project_ = project;
    project_root_ = project_dir;
    project_open_ = true;
    addRecentProject(&settings_, projectFilePath(project_dir));
    std::string save_error;
    saveSettings(&save_error);
    refreshTargets();
    return true;
}

bool AppCore::openProject(const std::string& path, std::string* error)
{
    const ProjectLoadResult loaded = loadProject(path, lang_);
    if (!loaded.ok) {
        if (error != nullptr) *error = loaded.error;
        return false;
    }
    project_ = loaded.project;
    project_root_ = projectRoot(path);
    project_open_ = true;

    const SdkInfo sdk = findSdkForPlatform(settings_, project_.platform, lang_);
    if (sdk.valid) {
        const std::string stored = resolveSdkPath(project_root_, project_);
        if (project_.sdk_path.empty() || !isDirectory(stored)) {
            project_.sdk_path = sdk.path;
            project_.engine_version = sdk.version;
        }
    }
    addRecentProject(&settings_, projectFilePath(project_root_));
    std::string save_error;
    saveSettings(&save_error);
    return true;
}

bool AppCore::saveProject(std::string* error)
{
    if (!project_open_) {
        if (error != nullptr) *error = tr(lang_, "err.no_project");
        return false;
    }
    if (!rpe::ui::saveProject(projectFile(), project_, error, lang_)) return false;
    addRecentProject(&settings_, projectFile());
    std::string save_error;
    saveSettings(&save_error);
    return true;
}

void AppCore::closeProject()
{
    build_cancel_.store(true);
    run_cancel_.store(true);
    ProcessRunner* build_process = build_process_.load();
    if (build_process != nullptr) build_process->cancel();
    ProcessRunner* run_process = run_process_.load();
    if (run_process != nullptr) run_process->cancel();
    waitThreads();
    project_open_ = false;
    project_ = Project();
    project_root_.clear();
}

bool AppCore::setProjectTarget(Platform platform, std::string* error)
{
    if (!project_open_) {
        if (error != nullptr) *error = tr(lang_, "err.no_project");
        return false;
    }
    const SdkInfo sdk = findSdkForPlatform(settings_, platform, lang_);
    if (!sdk.valid) {
        if (error != nullptr) {
            *error = sdk.error.empty()
                         ? trf(lang_, "target.reason.no_sdk", tr(lang_, platformNameKey(platform)))
                         : sdk.error;
        }
        return false;
    }
    if (platform != Platform::Pc) {
        const char* env = platform == Platform::Ps1 ? "PSN00BSDK_LIBS" : "JO_ENGINE_ROOT";
        if (environmentValue(env).empty()) {
            if (error != nullptr)
                *error = tr(lang_, platform == Platform::Ps1 ? "target.reason.psn00bsdk"
                                                             : "target.reason.jo_engine");
            return false;
        }
    }
    project_.platform = platform;
    project_.sdk_path = sdk.path;
    project_.engine_version = sdk.version;
    project_.target = defaultTargetForPlatform(platform);
    project_.run_target = project_.target;
    appendMessage("target: " + project_.target + " (" + platformCode(platform) +
                  "); "
                  "пересоберите проект с нуля, если CMakeLists не поддерживает платформу");
    return saveProject(nullptr);
}

AssetImportResult AppCore::importAsset(const std::string& source_path)
{
    AssetImportResult result;
    if (!project_open_) {
        result.error = tr(lang_, "err.no_project");
        return result;
    }
    SdkInfo sdk = currentSdk();
    if (!sdk.valid || sdk.converter_path.empty()) {
        sdk = findSdkForPlatform(settings_, Platform::Pc, lang_);
    }
    result = rpe::ui::importAsset(project_root_, &project_, sdk, source_path, lang_);
    if (result.ok) {
        std::string save_error;
        saveProject(&save_error);
    }
    return result;
}

bool AppCore::removeAsset(size_t index, bool delete_output, std::string* error)
{
    if (!project_open_) {
        if (error != nullptr) *error = tr(lang_, "err.no_project");
        return false;
    }
    if (!rpe::ui::removeAsset(project_root_, &project_, index, delete_output, error, lang_)) {
        return false;
    }
    return saveProject(error);
}

bool AppCore::startBuild(std::string* error)
{
    if (!project_open_) {
        if (error != nullptr) *error = tr(lang_, "err.build_no_project");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        if (build_status_.state == BuildState::Running) {
            if (error != nullptr) *error = tr(lang_, "err.build_running");
            return false;
        }
    }
    if (build_thread_.joinable()) build_thread_.join();

    const SdkInfo sdk = currentSdk();
    if (!sdk.valid) {
        if (error != nullptr) *error = tr(lang_, "err.build_no_sdk");
        return false;
    }
    BuildPlan plan = makeBuildPlan(project_, project_root_, sdk, settings_, lang_);
    if (!plan.ok) {
        if (error != nullptr) *error = plan.error;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        build_status_ = BuildStatus();
        build_status_.state = BuildState::Running;
        build_status_.step = "configure";
        build_log_.clear();
        active_build_plan_ = plan;
    }
    build_cancel_.store(false);
    build_thread_ = std::thread(&AppCore::runBuildThread, this, plan);
    return true;
}

void AppCore::runBuildThread(BuildPlan plan)
{
    bool cancelled = false;
    int code = runLines(plan.configure, project_root_, "configure", &build_cancel_, &build_process_,
                        &build_mutex_, &build_log_, &cancelled);
    if (build_cancel_.load() || cancelled) {
        std::lock_guard<std::mutex> lock(build_mutex_);
        build_status_.state = BuildState::Cancelled;
        build_status_.exit_code = code;
        build_status_.step = "cancelled";
        return;
    }
    if (code != 0) {
        std::lock_guard<std::mutex> lock(build_mutex_);
        build_status_.state = BuildState::Failed;
        build_status_.exit_code = code;
        build_status_.step = "configure";
        build_status_.error = trf(lang_, "build.failed", code);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        build_status_.step = "build";
    }
    code = runLines(plan.build, project_root_, "build", &build_cancel_, &build_process_,
                    &build_mutex_, &build_log_, &cancelled);
    std::lock_guard<std::mutex> lock(build_mutex_);
    build_status_.exit_code = code;
    if (build_cancel_.load() || cancelled) {
        build_status_.state = BuildState::Cancelled;
        build_status_.step = "cancelled";
    } else if (code == 0) {
        build_status_.state = BuildState::Succeeded;
        build_status_.step = "done";
    } else {
        build_status_.state = BuildState::Failed;
        build_status_.step = "build";
        build_status_.error = trf(lang_, "build.failed", code);
    }
}

void AppCore::cancelBuild()
{
    build_cancel_.store(true);
    ProcessRunner* process = build_process_.load();
    if (process != nullptr) process->cancel();
}

BuildStatus AppCore::buildStatus() const
{
    std::lock_guard<std::mutex> lock(build_mutex_);
    return build_status_;
}

std::vector<std::string> AppCore::takeBuildLog()
{
    std::lock_guard<std::mutex> lock(build_mutex_);
    std::vector<std::string> result;
    result.swap(build_log_);
    return result;
}

std::vector<std::string> AppCore::buildCommandPreview() const
{
    std::vector<std::string> result;
    std::lock_guard<std::mutex> lock(build_mutex_);
    if (!active_build_plan_.configure.empty()) {
        result.push_back(commandLineToString(active_build_plan_.configure));
    }
    if (!active_build_plan_.build.empty()) {
        result.push_back(commandLineToString(active_build_plan_.build));
    }
    return result;
}

bool AppCore::startRun(std::string* error)
{
    if (!project_open_) {
        if (error != nullptr) *error = tr(lang_, "err.run_no_project");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(run_mutex_);
        if (run_status_.state == RunState::Running) {
            if (error != nullptr) *error = tr(lang_, "err.run_running");
            return false;
        }
    }
    if (run_thread_.joinable()) run_thread_.join();

    const SdkInfo sdk = currentSdk();
    RunPlan plan = makeRunPlan(project_, project_root_, sdk, settings_, lang_);
    if (!plan.ok) {
        if (error != nullptr) *error = plan.error;
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(run_mutex_);
        run_status_ = RunStatus();
        run_status_.state = RunState::Running;
        run_log_.clear();
        active_run_argv_ = plan.argv;
        active_run_cwd_ = plan.working_directory;
    }
    run_cancel_.store(false);
    run_thread_ = std::thread(&AppCore::runRunThread, this, plan.argv, plan.working_directory);
    return true;
}

void AppCore::runRunThread(std::vector<std::string> argv, std::string working_directory)
{
    ProcessRunner runner;
    ProcessSpec spec;
    spec.argv = argv;
    spec.working_directory = working_directory;
    std::string start_error;
    {
        std::lock_guard<std::mutex> lock(run_mutex_);
        run_log_.push_back("$ " + commandLineToString(argv));
    }
    if (!runner.start(spec, &start_error)) {
        std::lock_guard<std::mutex> lock(run_mutex_);
        run_status_.state = RunState::Failed;
        run_status_.error = start_error;
        run_log_.push_back("run: " + start_error);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(run_mutex_);
        run_status_.pid = runner.pid();
    }
    run_process_.store(&runner);
    while (runner.running()) {
        if (run_cancel_.load()) runner.cancel();
        std::vector<std::string> lines = runner.takeLines();
        if (!lines.empty()) {
            std::lock_guard<std::mutex> lock(run_mutex_);
            run_log_.insert(run_log_.end(), lines.begin(), lines.end());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::vector<std::string> rest = runner.takeLines();
    if (!rest.empty()) {
        std::lock_guard<std::mutex> lock(run_mutex_);
        run_log_.insert(run_log_.end(), rest.begin(), rest.end());
    }
    runner.wait(0);
    run_process_.store(nullptr);
    std::lock_guard<std::mutex> lock(run_mutex_);
    run_status_.state = RunState::Exited;
    run_status_.exit_code = runner.exitCode();
    run_status_.cancelled = runner.cancelled();
}

void AppCore::stopRun()
{
    run_cancel_.store(true);
    ProcessRunner* process = run_process_.load();
    if (process != nullptr) process->cancel();
}

RunStatus AppCore::runStatus() const
{
    std::lock_guard<std::mutex> lock(run_mutex_);
    return run_status_;
}

std::vector<std::string> AppCore::takeRunLog()
{
    std::lock_guard<std::mutex> lock(run_mutex_);
    std::vector<std::string> result;
    result.swap(run_log_);
    return result;
}

std::string AppCore::runCommandPreview() const
{
    std::lock_guard<std::mutex> lock(run_mutex_);
    return commandLineToString(active_run_argv_);
}

SdkInfo AppCore::currentSdk() const
{
    if (!project_open_) return SdkInfo();
    return findSdkForPlatform(settings_, project_.platform, lang_);
}

DiagnosticsReport AppCore::diagnostics() const
{
    return collectDiagnostics(project_open_ ? &project_ : nullptr, project_root_, currentSdk(),
                              settings_, lang_);
}

VersionCompatibility AppCore::compatibility(std::string* detail) const
{
    if (!project_open_) {
        if (detail != nullptr) *detail = tr(lang_, "version.no_sdk");
        return VersionCompatibility::Invalid;
    }
    const SdkInfo sdk = currentSdk();
    if (!sdk.valid) {
        if (detail != nullptr) *detail = tr(lang_, "version.no_sdk");
        return VersionCompatibility::Invalid;
    }
    return checkVersionCompatibility(project_.engine_version, sdk.version, detail);
}

std::vector<RecentProject> AppCore::recentProjects() const
{
    std::vector<RecentProject> result;
    for (size_t i = 0; i < settings_.recent_projects.size(); ++i) {
        RecentProject recent;
        recent.path = settings_.recent_projects[i];
        recent.exists = isRegularFile(recent.path);
        result.push_back(recent);
    }
    return result;
}

std::vector<std::string> AppCore::takeMessages()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.swap(messages_);
    return result;
}

void AppCore::appendMessage(const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    messages_.push_back(message);
}

void AppCore::waitThreads()
{
    if (build_thread_.joinable()) build_thread_.join();
    if (run_thread_.joinable()) run_thread_.join();
}

} // namespace ui
} // namespace rpe
