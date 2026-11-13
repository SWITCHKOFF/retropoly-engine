// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/gui/gui_app.h
//
// SDL3 + Dear ImGui GUI. Вся логика — в AppCore; GUI только отображает
// состояние и вызывает команды (Спринт 35–36, недели 69–71).
#pragma once

#include <stdint.h>

#include <mutex>
#include <string>
#include <vector>

#include "project.h"
#include "settings.h"

struct SDL_Window;
struct ImFont;

namespace rpe {
namespace ui {

class AppCore;

/** Параметры headless/скриптованного smoke-прогона. */
struct SmokeOptions
{
    Platform platform = Platform::Pc;
    std::string work_dir;
    std::string samples_dir;
    std::string screenshot_dir;
    int run_seconds = 2;
    bool do_run = true;
    bool keep_project = false;
};

/** GUI-приложение UI client. */
class GuiApp {
public:
    explicit GuiApp(AppCore& core);
    ~GuiApp();

    GuiApp(const GuiApp&) = delete;
    GuiApp& operator=(const GuiApp&) = delete;

    /** Создать окно/GL/ImGui. @return false с текстом ошибки. */
    bool init(std::string* error);

    /** Интерактивный цикл; возвращает код выхода. */
    int run();

    /** Скриптованный smoke: welcome → new project → import → build → run. */
    int runSmoke(const SmokeOptions& options);

    /** Открыть проект до первого кадра. */
    void openProjectAtStartup(const std::string& path);

private:
    enum class Screen
    {
        Welcome,
        Project
    };

    enum class DialogTarget
    {
        None,
        Project,
        SdkPc,
        SdkPs1,
        SdkSaturn,
        EmulatorPs1,
        EmulatorSaturn,
        ImportAsset,
        WizardLocation
    };

    void applyTheme();
    void setupFonts();
    void drawFrame();
    void drawMenuBar();
    void drawWelcome();
    void drawProject();
    void drawAssetsTab();
    void drawBuildTab();
    void drawRunTab();
    void drawDiagnostics();
    void drawVersionPanel();
    void drawWizard();
    void drawSettings();
    void drawAbout();
    void drawStatusBar();
    void pollCore();
    void pollDialogs();

    void renderFrame(bool capture, const std::string& screenshot_path);
    bool capture(const std::string& path);
    void consumePendingDialog();

    void requestOpenProjectDialog();
    void requestFolderDialog(DialogTarget target);
    void requestFileDialog(DialogTarget target, const char* filter_name, const char* pattern);

    void openProject(const std::string& path);
    void saveProject();
    void createProjectFromWizard();
    void importAsset(const std::string& path);
    void removeSelectedAsset();
    void startBuild();
    void startRun();

    static void fileDialogCallback(void* userdata, const char* const* filelist, int filter);

    AppCore& core_;
    SDL_Window* window_ = nullptr;
    void* gl_context_ = nullptr;
    ImFont* font_ = nullptr;
    bool imgui_ready_ = false;
    bool running_ = true;
    Screen screen_ = Screen::Welcome;

    std::vector<std::string> build_log_;
    std::vector<std::string> run_log_;
    bool scroll_build_ = false;
    bool scroll_run_ = false;
    bool show_wizard_ = false;
    bool show_settings_ = false;
    bool show_about_ = false;
    bool show_confirm_remove_ = false;
    int tab_ = 0;

    // Поля wizard/settings (редактируются в модальных окнах).
    std::string wizard_name_;
    std::string wizard_location_;
    std::string wizard_template_ = "hello_triangle";
    int wizard_target_ = 0;
    AppSettings settings_edit_;
    std::string settings_error_;

    std::string error_message_;
    std::string info_message_;

    // Асинхронный callback SDL-диалога может прийти из другого потока.
    mutable std::mutex dialog_mutex_;
    DialogTarget pending_target_ = DialogTarget::None;
    std::string pending_path_;
    DialogTarget requested_target_ = DialogTarget::None;
    std::string requested_filter_name_;
    std::string requested_filter_pattern_;

    std::string screenshot_dir_;
    int run_seconds_ = 2;
    int selected_asset_ = -1;
};

}  // namespace ui
}  // namespace rpe
