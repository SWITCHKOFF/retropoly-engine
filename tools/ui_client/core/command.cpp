// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/command.cpp
#include "command.h"

#include <algorithm>

#include "fs_util.h"

namespace rpe {
namespace ui {

namespace {

const char* executableSuffix()
{
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

std::string firstExisting(const std::vector<std::string>& candidates)
{
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (isRegularFile(candidates[i])) return candidates[i];
    }
    return std::string();
}

std::string findCueRecursive(const std::string& directory, int depth)
{
    if (depth < 0 || !isDirectory(directory)) return std::string();
    const std::vector<std::string> entries = listDirectorySorted(directory);
    // Сначала файлы текущего уровня, потом подкаталоги.
    for (size_t i = 0; i < entries.size(); ++i) {
        const std::string path = joinPath(directory, entries[i]);
        if (isRegularFile(path) && fileExtension(path) == ".cue") return path;
    }
    for (size_t i = 0; i < entries.size(); ++i) {
        const std::string path = joinPath(directory, entries[i]);
        if (isDirectory(path)) {
            const std::string found = findCueRecursive(path, depth - 1);
            if (!found.empty()) return found;
        }
    }
    return std::string();
}

std::string emulatorForPlatform(const Project& project, const AppSettings& settings,
                                Platform platform)
{
    if (!project.emulator_path.empty()) return project.emulator_path;
    if (platform == Platform::Ps1) return settings.emulator_ps1;
    if (platform == Platform::Saturn) return settings.emulator_saturn;
    return std::string();
}

}  // namespace

std::string effectiveBuildDir(const std::string& project_root, const Project& project)
{
    return joinPath(resolveBuildDir(project_root, project), platformCode(project.platform));
}

BuildPlan makeBuildPlan(const Project& project, const std::string& project_root, const SdkInfo& sdk,
                        const AppSettings& settings, Lang lang)
{
    BuildPlan plan;
    plan.target = project.target;
    if (!sdk.valid) {
        plan.error = tr(lang, "err.build_no_sdk");
        return plan;
    }
    const std::string cmake = findExecutable(settings.cmake_path, {"cmake", "cmake.exe"});
    if (cmake.empty()) {
        plan.error = tr(lang, "err.cmake_missing");
        return plan;
    }
    const std::string ninja = findExecutable(settings.ninja_path, {"ninja", "ninja.exe"});
    if (ninja.empty()) {
        plan.error = tr(lang, "err.ninja_missing");
        return plan;
    }

    plan.build_dir = effectiveBuildDir(project_root, project);
    plan.configure.push_back(cmake);
    plan.configure.push_back("-S");
    plan.configure.push_back(project_root);
    plan.configure.push_back("-B");
    plan.configure.push_back(plan.build_dir);
    plan.configure.push_back("-G");
    plan.configure.push_back("Ninja");
    plan.configure.push_back("-DCMAKE_BUILD_TYPE=Release");
    plan.configure.push_back("-DCMAKE_MAKE_PROGRAM=" + ninja);
    plan.configure.push_back("-DCMAKE_PREFIX_PATH=" + sdk.path);
    if (project.platform == Platform::Ps1) {
        plan.configure.push_back("-DCMAKE_TOOLCHAIN_FILE=" +
                                 joinPath(sdk.path, "cmake/ps1-toolchain.cmake"));
    } else if (project.platform == Platform::Saturn) {
        plan.configure.push_back("-DCMAKE_TOOLCHAIN_FILE=" +
                                 joinPath(sdk.path, "cmake/saturn-sdk-toolchain.cmake"));
        const std::string jo_root = environmentValue("JO_ENGINE_ROOT");
        if (!jo_root.empty()) plan.configure.push_back("-DJO_ENGINE_ROOT=" + jo_root);
    }

    plan.build.push_back(cmake);
    plan.build.push_back("--build");
    plan.build.push_back(plan.build_dir);
    plan.build.push_back("--target");
    plan.build.push_back(project.target);
    plan.build.push_back("--config");
    plan.build.push_back("Release");
    plan.ok = true;
    return plan;
}

std::string findPcExecutable(const std::string& build_dir, const std::string& target)
{
    const std::string suffix = executableSuffix();
    return firstExisting({
        joinPath(build_dir, target + suffix),
        joinPath(build_dir, "bin/" + target + suffix),
        joinPath(build_dir, "game/" + target + suffix),
        joinPath(build_dir, "examples/hello_triangle/" + target + suffix),
        joinPath(build_dir, "examples/rotating_cube/" + target + suffix),
    });
}

std::string findCueFile(const std::string& project_root, const std::string& build_dir)
{
    std::string cue = findCueRecursive(build_dir, 4);
    if (!cue.empty()) return cue;
    cue = findCueRecursive(joinPath(project_root, "dist"), 3);
    if (!cue.empty()) return cue;
    return findCueRecursive(project_root, 2);
}

RunPlan makeRunPlan(const Project& project, const std::string& project_root, const SdkInfo& sdk,
                    const AppSettings& settings, Lang lang)
{
    RunPlan plan;
    if (!sdk.valid) {
        plan.error_key = "err.build_no_sdk";
        plan.error = tr(lang, plan.error_key.c_str());
        return plan;
    }
    const std::string build_dir = effectiveBuildDir(project_root, project);
    const std::string target = runTargetName(project);
    if (project.platform == Platform::Pc) {
        const std::string executable = findPcExecutable(build_dir, target);
        if (executable.empty()) {
            plan.error_key = "err.run_no_binary";
            plan.error_detail = joinPath(build_dir, target + executableSuffix());
            plan.error = trf(lang, plan.error_key.c_str(), plan.error_detail.c_str());
            return plan;
        }
        plan.argv.push_back(executable);
        plan.working_directory = project_root;
        plan.ok = true;
        return plan;
    }

    const std::string emulator = emulatorForPlatform(project, settings, project.platform);
    if (emulator.empty()) {
        plan.error_key = "err.run_no_emulator";
        plan.error = tr(lang, plan.error_key.c_str());
        return plan;
    }
    const std::string emulator_path =
        isAbsolutePath(emulator) ? absolutePath(emulator) : joinPath(project_root, emulator);
    if (!isRegularFile(emulator_path)) {
        plan.error_key = "err.run_emulator_missing";
        plan.error_detail = emulator_path;
        plan.error = trf(lang, plan.error_key.c_str(), emulator_path.c_str());
        return plan;
    }
    const std::string cue = findCueFile(project_root, build_dir);
    if (cue.empty()) {
        plan.error_key = "err.run_no_cue";
        plan.error_detail = std::string(tr(lang, platformNameKey(project.platform)));
        plan.error = trf(lang, plan.error_key.c_str(), plan.error_detail.c_str());
        return plan;
    }
    plan.argv.push_back(emulator_path);
    plan.argv.push_back(cue);
    plan.working_directory = project_root;
    plan.uses_emulator = true;
    plan.cue_path = cue;
    plan.ok = true;
    return plan;
}

}  // namespace ui
}  // namespace rpe
