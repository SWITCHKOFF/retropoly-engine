// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/main.cpp
//
// UI client entry point (Спринт 35–36).
//   retro_engine_ui [--project PATH] [--sdk PATH] [--lang ru|en]
//                   [--theme dark|light] [--screenshot DIR]
//   retro_engine_ui --smoke --work-dir DIR [--samples DIR]
//                   [--screenshot-dir DIR] [--run-seconds N] [--no-run]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "app_core.h"
#include "gui_app.h"
#include "i18n.h"
#include "version.h"

using namespace rpe::ui;

namespace {

void printUsage(const char* exe)
{
    std::printf(
        "RetroPolyEngine UI client %s\n\n"
        "Usage: %s [options]\n\n"
        "  --project PATH       открыть .rpeproject при старте\n"
        "  --sdk PATH           PC SDK (sdk_manifest.json)\n"
        "  --lang ru|en         язык интерфейса\n"
        "  --theme dark|light   тема\n"
        "  --smoke              headless smoke: new project → import → build → run\n"
        "  --run-project        открыть --project и запустить target/эмулятор\n"
        "  --emulator PATH      эмулятор для --run-project (PS1/Saturn)\n"
        "  --smoke-platform P   target для smoke: pc|ps1|saturn (default pc)\n"
        "  --work-dir DIR       каталог smoke-проекта\n"
        "  --samples DIR        каталог с cube.gltf/texture.png для импорта\n"
        "  --screenshot-dir DIR сохранить скриншоты smoke-сценария\n"
        "  --run-seconds N      длительность smoke-запуска (по умолчанию 2)\n"
        "  --no-run             не запускать target в smoke\n"
        "  --version            версия UI client\n"
        "  --help               эта справка\n",
        uiVersion(), exe);
}

bool takeValue(int argc, char** argv, int* index, const char* name, std::string* out)
{
    if (*index + 1 >= argc) {
        std::fprintf(stderr, "%s: требуется значение\n", name);
        return false;
    }
    *out = argv[++(*index)];
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    std::string project_path;
    std::string sdk_path;
    std::string lang_code;
    std::string theme;
    std::string work_dir;
    std::string samples_dir;
    std::string screenshot_dir;
    std::string smoke_platform_code;
    Platform smoke_platform = Platform::Pc;
    std::string emulator_path;
    int run_seconds = 2;
    bool run_project = false;
    bool smoke = false;
    bool do_run = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "--version") {
            std::printf("retro-engine-ui %s (SDL %d.%d.%d)\n", uiVersion(), SDL_MAJOR_VERSION,
                        SDL_MINOR_VERSION, SDL_MICRO_VERSION);
            return 0;
        }
        if (arg == "--project") {
            if (!takeValue(argc, argv, &i, "--project", &project_path)) return 2;
        } else if (arg == "--sdk") {
            if (!takeValue(argc, argv, &i, "--sdk", &sdk_path)) return 2;
        } else if (arg == "--lang") {
            if (!takeValue(argc, argv, &i, "--lang", &lang_code)) return 2;
        } else if (arg == "--theme") {
            if (!takeValue(argc, argv, &i, "--theme", &theme)) return 2;
        } else if (arg == "--smoke") {
            smoke = true;
        } else if (arg == "--run-project") {
            run_project = true;
        } else if (arg == "--emulator") {
            if (!takeValue(argc, argv, &i, "--emulator", &emulator_path)) return 2;
        } else if (arg == "--work-dir") {
            if (!takeValue(argc, argv, &i, "--work-dir", &work_dir)) return 2;
        } else if (arg == "--samples") {
            if (!takeValue(argc, argv, &i, "--samples", &samples_dir)) return 2;
        } else if (arg == "--screenshot-dir") {
            if (!takeValue(argc, argv, &i, "--screenshot-dir", &screenshot_dir)) return 2;
        } else if (arg == "--run-seconds") {
            std::string value;
            if (!takeValue(argc, argv, &i, "--run-seconds", &value)) return 2;
            run_seconds = std::atoi(value.c_str());
        } else if (arg == "--smoke-platform") {
            if (!takeValue(argc, argv, &i, "--smoke-platform", &smoke_platform_code)) {
                return 2;
            }
            if (!parsePlatform(smoke_platform_code, &smoke_platform)) {
                std::fprintf(stderr, "unknown smoke platform: %s\n", smoke_platform_code.c_str());
                return 2;
            }
        } else if (arg == "--no-run") {
            do_run = false;
        } else {
            std::fprintf(stderr, "unknown option: %s\n", arg.c_str());
            printUsage(argv[0]);
            return 2;
        }
    }

    AppCore core;
    std::string error;
    core.initialize(&error);
    if (!error.empty()) std::fprintf(stderr, "ui: %s\n", error.c_str());

    if (!lang_code.empty()) core.setLang(parseLang(lang_code));
    if (!theme.empty()) core.mutableSettings().theme = theme;
    if (!sdk_path.empty()) {
        std::string sdk_error;
        const Platform sdk_platform = smoke ? smoke_platform : Platform::Pc;
        if (!core.setSdkPath(sdk_platform, sdk_path, &sdk_error)) {
            std::fprintf(stderr, "ui: SDK: %s\n", sdk_error.c_str());
        }
    }

    GuiApp app(core);
    if (!app.init(&error)) {
        std::fprintf(stderr, "ui: %s\n", error.c_str());
        return 1;
    }
    if (!project_path.empty()) app.openProjectAtStartup(project_path);
    if (!emulator_path.empty() && core.hasProject()) {
        if (core.project().platform == Platform::Ps1) {
            core.mutableSettings().emulator_ps1 = emulator_path;
        } else if (core.project().platform == Platform::Saturn) {
            core.mutableSettings().emulator_saturn = emulator_path;
        }
    }

    if (run_project) {
        app.setScreenshotDir(screenshot_dir);
        return app.runProjectOnce(run_seconds);
    }
    if (smoke) {
        SmokeOptions options;
        options.platform = smoke_platform;
        options.work_dir = work_dir;
        options.samples_dir = samples_dir;
        options.screenshot_dir = screenshot_dir;
        options.run_seconds = run_seconds;
        options.do_run = do_run;
        return app.runSmoke(options);
    }
    return app.run();
}
