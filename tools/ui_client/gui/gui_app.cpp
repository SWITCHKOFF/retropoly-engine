// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/gui/gui_app.cpp
#include "gui_app.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>
#include <glad/gl.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#include "app_core.h"
#include "fs_util.h"
#include "screenshot.h"

#ifndef RPE_UI_SOURCE_DIR
#define RPE_UI_SOURCE_DIR "."
#endif

namespace rpe {
namespace ui {

namespace {

const char* tr_(Lang lang, const char* key)
{
    return tr(lang, key);
}

void copyToBuffer(const std::string& value, char* buffer, size_t size)
{
    if (size == 0) return;
    const size_t length = std::min(value.size(), size - 1);
    std::memcpy(buffer, value.data(), length);
    buffer[length] = '\0';
}

bool inputString(const char* label, std::string* value, size_t capacity = 512)
{
    std::vector<char> buffer(capacity, '\0');
    copyToBuffer(*value, buffer.data(), buffer.size());
    if (ImGui::InputText(label, buffer.data(), buffer.size())) {
        *value = buffer.data();
        return true;
    }
    return false;
}

std::string fontPath()
{
    const char* env = std::getenv("RPE_UI_FONT");
    if (env != nullptr && isRegularFile(env)) return env;
    const std::string exe_dir = executableDirectory();
    const std::string candidates[] = {
        joinPath(exe_dir, "assets/DejaVuSans.ttf"),
        joinPath(exe_dir, "../assets/DejaVuSans.ttf"),
        std::string(RPE_UI_SOURCE_DIR) + "/tools/ui_client/assets/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/Library/Fonts/DejaVuSans.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (isRegularFile(candidates[i])) return candidates[i];
    }
    return std::string();
}

ImU32 severityColor(DiagnosticSeverity severity)
{
    if (severity == DiagnosticSeverity::Error) return IM_COL32(235, 90, 90, 255);
    if (severity == DiagnosticSeverity::Warning) return IM_COL32(235, 190, 70, 255);
    return IM_COL32(110, 210, 120, 255);
}

const char* severityLabel(Lang lang, DiagnosticSeverity severity)
{
    if (severity == DiagnosticSeverity::Error) return tr(lang, "diag.error");
    if (severity == DiagnosticSeverity::Warning) return tr(lang, "diag.warning");
    return tr(lang, "diag.ok");
}

const char* stateLabel(Lang lang, BuildState state)
{
    switch (state) {
        case BuildState::Idle:
            return tr(lang, "build.idle");
        case BuildState::Running:
            return tr(lang, "build.running");
        case BuildState::Succeeded:
            return tr(lang, "build.ok");
        case BuildState::Failed:
            return tr(lang, "common.error");
        case BuildState::Cancelled:
            return tr(lang, "build.cancelled");
    }
    return "";
}

} // namespace

GuiApp::GuiApp(AppCore& core) : core_(core)
{
    settings_edit_ = core_.settings();
    wizard_location_ = homeDirectory();
}

GuiApp::~GuiApp()
{
    if (imgui_ready_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (gl_context_ != nullptr) SDL_GL_DestroyContext(static_cast<SDL_GLContext>(gl_context_));
    if (window_ != nullptr) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool GuiApp::init(std::string* error)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        if (error != nullptr) *error = std::string("SDL_Init: ") + SDL_GetError();
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    window_ =
        SDL_CreateWindow("RetroPolyEngine UI", 1280, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        if (error != nullptr) *error = std::string("SDL_CreateWindow: ") + SDL_GetError();
        return false;
    }
    gl_context_ = SDL_GL_CreateContext(window_);
    if (gl_context_ == nullptr) {
        if (error != nullptr) *error = std::string("SDL_GL_CreateContext: ") + SDL_GetError();
        return false;
    }
    SDL_GL_MakeCurrent(window_, static_cast<SDL_GLContext>(gl_context_));
    SDL_GL_SetSwapInterval(1);
    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) == 0) {
        if (error != nullptr) *error = "gladLoadGL failed";
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    setupFonts();
    applyTheme();
    ImGui_ImplSDL3_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init("#version 330");
    imgui_ready_ = true;
    settings_edit_ = core_.settings();
    return true;
}

void GuiApp::setupFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    const std::string path = fontPath();
    if (!path.empty()) {
        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 2;
        font_ = io.Fonts->AddFontFromFileTTF(path.c_str(), 16.0f, &config);
    }
    if (font_ == nullptr) font_ = io.Fonts->AddFontDefault();
}

void GuiApp::applyTheme()
{
    const bool dark = core_.settings().theme != "light";
    if (dark) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.45f, 0.75f, 0.55f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.42f, 0.70f, 0.75f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.52f, 0.85f, 0.90f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.36f, 0.62f, 1.00f);
}

void GuiApp::openProjectAtStartup(const std::string& path)
{
    std::string error;
    if (core_.openProject(path, &error)) {
        screen_ = Screen::Project;
        settings_edit_ = core_.settings();
    } else {
        error_message_ = error;
    }
}

int GuiApp::run()
{
    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running_ = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window_)) {
                running_ = false;
            }
        }
        pollCore();
        pollDialogs();
        renderFrame(false, std::string());
        SDL_Delay(1);
    }
    return 0;
}

void GuiApp::pollCore()
{
    std::vector<std::string> build = core_.takeBuildLog();
    if (!build.empty()) {
        build_log_.insert(build_log_.end(), build.begin(), build.end());
        if (build_log_.size() > 5000) {
            build_log_.erase(build_log_.begin(), build_log_.begin() + 1000);
        }
        scroll_build_ = true;
    }
    std::vector<std::string> run = core_.takeRunLog();
    if (!run.empty()) {
        run_log_.insert(run_log_.end(), run.begin(), run.end());
        if (run_log_.size() > 5000) {
            run_log_.erase(run_log_.begin(), run_log_.begin() + 1000);
        }
        scroll_run_ = true;
    }
    std::vector<std::string> messages = core_.takeMessages();
    for (size_t i = 0; i < messages.size(); ++i) {
        info_message_ = messages[i];
    }
}

void GuiApp::pollDialogs()
{
    DialogTarget target = DialogTarget::None;
    std::string path;
    {
        std::lock_guard<std::mutex> lock(dialog_mutex_);
        if (pending_target_ != DialogTarget::None) {
            target = pending_target_;
            path = pending_path_;
            pending_target_ = DialogTarget::None;
            pending_path_.clear();
        }
    }
    if (target == DialogTarget::None || path.empty()) return;
    switch (target) {
        case DialogTarget::Project:
            openProject(path);
            break;
        case DialogTarget::WizardLocation:
            wizard_location_ = path;
            break;
        case DialogTarget::SdkPc:
        case DialogTarget::SdkPs1:
        case DialogTarget::SdkSaturn: {
            const Platform platform = target == DialogTarget::SdkPc    ? Platform::Pc
                                      : target == DialogTarget::SdkPs1 ? Platform::Ps1
                                                                       : Platform::Saturn;
            std::string error;
            if (!core_.setSdkPath(platform, path, &error)) {
                error_message_ = error;
            } else {
                settings_edit_ = core_.settings();
                info_message_ = "SDK: " + path;
            }
            break;
        }
        case DialogTarget::EmulatorPs1:
            settings_edit_.emulator_ps1 = path;
            core_.mutableSettings().emulator_ps1 = path;
            break;
        case DialogTarget::EmulatorSaturn:
            settings_edit_.emulator_saturn = path;
            core_.mutableSettings().emulator_saturn = path;
            break;
        case DialogTarget::ImportAsset:
            importAsset(path);
            break;
        case DialogTarget::None:
            break;
    }
}

void GuiApp::renderFrame(bool capture_frame, const std::string& screenshot_path)
{
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    drawFrame();

    ImGui::Render();
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window_, &width, &height);
    glViewport(0, 0, width, height);
    const ImVec4 clear = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    glClearColor(clear.x, clear.y, clear.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (capture_frame && !screenshot_path.empty()) {
        capture(screenshot_path);
    }
    SDL_GL_SwapWindow(window_);
}

bool GuiApp::capture(const std::string& path)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window_, &width, &height);
    std::string error;
    if (!captureFramebufferPng(path, width, height, &error)) {
        std::fprintf(stderr, "[ui] screenshot failed: %s\n", error.c_str());
        return false;
    }
    std::printf("[ui] screenshot: %s\n", path.c_str());
    return true;
}

void GuiApp::drawFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    drawMenuBar();
    const float menu_height = ImGui::GetFrameHeight();
    const float status_height = ImGui::GetFrameHeight() + 4.0f;
    ImGui::SetNextWindowPos(ImVec2(0.0f, menu_height));
    ImGui::SetNextWindowSize(
        ImVec2(io.DisplaySize.x, std::max(1.0f, io.DisplaySize.y - menu_height - status_height)));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##main", nullptr, flags);
    if (screen_ == Screen::Welcome) {
        drawWelcome();
    } else {
        drawProject();
    }
    ImGui::End();

    drawWizard();
    drawSettings();
    drawAbout();
    if (show_confirm_remove_) {
        if (!ImGui::IsPopupOpen("##remove_asset")) ImGui::OpenPopup("##remove_asset");
        if (ImGui::BeginPopupModal("##remove_asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(tr_(core_.lang(), "assets.confirm_remove"));
            ImGui::Separator();
            if (ImGui::Button(tr_(core_.lang(), "common.yes"), ImVec2(120.0f, 0.0f))) {
                removeSelectedAsset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(tr_(core_.lang(), "common.no"), ImVec2(120.0f, 0.0f))) {
                show_confirm_remove_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else {
            show_confirm_remove_ = false;
        }
    }
    drawStatusBar();
}

void GuiApp::drawMenuBar()
{
    const Lang lang = core_.lang();
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu(tr_(lang, "menu.file"))) {
        if (ImGui::MenuItem(tr_(lang, "menu.new"), "Ctrl+N")) {
            wizard_location_ = homeDirectory();
            wizard_name_ = "my_project";
            wizard_target_ = 0;
            wizard_template_ = "hello_triangle";
            show_wizard_ = true;
        }
        if (ImGui::MenuItem(tr_(lang, "menu.open"), "Ctrl+O")) requestOpenProjectDialog();
        if (ImGui::MenuItem(tr_(lang, "menu.save"), "Ctrl+S", false, core_.hasProject())) {
            saveProject();
        }
        if (ImGui::MenuItem(tr_(lang, "menu.close"), nullptr, false, core_.hasProject())) {
            core_.closeProject();
            screen_ = Screen::Welcome;
            build_log_.clear();
            run_log_.clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(tr_(lang, "menu.exit"))) running_ = false;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr_(lang, "menu.view"))) {
        if (ImGui::MenuItem(tr_(lang, "menu.settings"))) {
            settings_edit_ = core_.settings();
            settings_error_.clear();
            show_settings_ = true;
        }
        if (ImGui::BeginMenu(tr_(lang, "menu.theme"))) {
            if (ImGui::MenuItem(tr_(lang, "theme.dark"), nullptr,
                                core_.settings().theme == "dark")) {
                core_.mutableSettings().theme = "dark";
                applyTheme();
                std::string error;
                core_.saveSettings(&error);
            }
            if (ImGui::MenuItem(tr_(lang, "theme.light"), nullptr,
                                core_.settings().theme == "light")) {
                core_.mutableSettings().theme = "light";
                applyTheme();
                std::string error;
                core_.saveSettings(&error);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr_(lang, "menu.language"))) {
            if (ImGui::MenuItem("Русский", nullptr, core_.lang() == Lang::Ru)) {
                core_.setLang(Lang::Ru);
                std::string error;
                core_.saveSettings(&error);
            }
            if (ImGui::MenuItem("English", nullptr, core_.lang() == Lang::En)) {
                core_.setLang(Lang::En);
                std::string error;
                core_.saveSettings(&error);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(tr_(lang, "menu.help"))) {
        if (ImGui::MenuItem(tr_(lang, "menu.docs"))) {
            info_message_ = "docs/UI_CLIENT.md";
        }
        if (ImGui::MenuItem(tr_(lang, "menu.about"))) show_about_ = true;
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void GuiApp::drawWelcome()
{
    const Lang lang = core_.lang();
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Dummy(ImVec2(0.0f, io.DisplaySize.y * 0.08f));
    const float width = std::min(760.0f, io.DisplaySize.x - 40.0f);
    ImGui::SetCursorPosX((io.DisplaySize.x - width) * 0.5f);
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.8f);
    ImGui::TextUnformatted(tr_(lang, "welcome.title"));
    ImGui::PopFont();
    ImGui::PushTextWrapPos(width);
    ImGui::TextWrapped("%s", tr_(lang, "welcome.subtitle"));
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    ImGui::Spacing();
    if (ImGui::Button(tr_(lang, "welcome.new"), ImVec2(220.0f, 44.0f))) {
        wizard_location_ = homeDirectory();
        wizard_name_ = "my_project";
        wizard_target_ = 0;
        wizard_template_ = "hello_triangle";
        show_wizard_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr_(lang, "welcome.open"), ImVec2(220.0f, 44.0f))) {
        requestOpenProjectDialog();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr_(lang, "welcome.settings"), ImVec2(180.0f, 44.0f))) {
        settings_edit_ = core_.settings();
        settings_error_.clear();
        show_settings_ = true;
    }
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SeparatorText(tr_(lang, "welcome.recent"));
    const std::vector<RecentProject> recent = core_.recentProjects();
    if (recent.empty()) {
        ImGui::TextDisabled("%s", tr_(lang, "welcome.no_recent"));
    } else {
        for (size_t i = 0; i < recent.size(); ++i) {
            const std::string label =
                recent[i].path +
                (recent[i].exists ? "" : std::string("  (") + tr_(lang, "welcome.missing") + ")");
            if (!recent[i].exists) ImGui::BeginDisabled();
            if (ImGui::Selectable(label.c_str())) openProject(recent[i].path);
            if (!recent[i].exists) ImGui::EndDisabled();
        }
    }
    ImGui::Spacing();
    ImGui::SeparatorText("SDK");
    const std::vector<TargetAvailability>& targets = core_.targets();
    if (!targets.empty() && targets[0].available) {
        const SdkInfo sdk = core_.currentSdk();
        const std::string text =
            sdk.valid ? sdk.path + "  (" + sdk.version + ")" : targets[0].sdk_path;
        ImGui::TextColored(
            ImVec4(0.45f, 0.85f, 0.55f, 1.0f), "%s",
            trf(lang, "welcome.sdk_ready", text.c_str(), platformCode(Platform::Pc)).c_str());
    } else {
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f), "%s", tr_(lang, "welcome.no_sdk"));
    }
    ImGui::EndGroup();
}

void GuiApp::drawProject()
{
    const Lang lang = core_.lang();
    const Project& project = core_.project();
    ImGui::Text("%s", trf(lang, "project.title", project.name.c_str()).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("  %s", core_.projectRootPath().c_str());
    ImGui::SameLine();
    if (ImGui::Button(tr_(lang, "menu.save"))) saveProject();

    ImGui::Separator();

    const float left_width = 380.0f;
    ImGui::BeginChild("##project_left", ImVec2(left_width, 0.0f), ImGuiChildFlags_Borders);
    drawVersionPanel();
    ImGui::SeparatorText(tr_(lang, "diag.title"));
    drawDiagnostics();
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##project_right", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    const char* tab_names[3] = {tr_(lang, "project.assets"), tr_(lang, "project.build"),
                                tr_(lang, "project.run")};
    for (int i = 0; i < 3; ++i) {
        if (i > 0) ImGui::SameLine();
        if (tab_ == i)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
        if (ImGui::Button(tab_names[i])) tab_ = i;
        if (tab_ == i) ImGui::PopStyleColor();
    }
    ImGui::Separator();
    if (tab_ == 0) {
        drawAssetsTab();
    } else if (tab_ == 1) {
        drawBuildTab();
    } else {
        drawRunTab();
    }
    ImGui::EndChild();
}

void GuiApp::drawVersionPanel()
{
    const Lang lang = core_.lang();
    const Project& project = core_.project();
    const SdkInfo sdk = core_.currentSdk();
    ImGui::SeparatorText(tr_(lang, "project.version"));

    ImGui::Text("%s: %s", tr_(lang, "version.ui"), uiVersion());
    ImGui::Text("%s: %s", tr_(lang, "project.engine_version"), project.engine_version.c_str());
    ImGui::Text("%s: %s", tr_(lang, "project.platform"),
                tr_(lang, platformNameKey(project.platform)));
    ImGui::Text("%s: %s", tr_(lang, "project.target"), project.target.c_str());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::Text("%s: %s", tr_(lang, "project.sdk"),
                sdk.valid ? sdk.path.c_str() : tr_(lang, "common.not_set"));
    ImGui::PopTextWrapPos();
    if (sdk.valid) {
        ImGui::Text("%s: %s", tr_(lang, "version.sdk"), sdk.version.c_str());
        ImGui::Text("%s: %.12s%s", tr_(lang, "version.engine_commit"), sdk.git_commit.c_str(),
                    sdk.git_dirty ? " (dirty)" : "");
        ImGui::Text("%s: %s/%s", tr_(lang, "version.sdk_platform"), platformCode(sdk.platform),
                    sdk.arch.c_str());
        ImGui::Text("%s: %s %s", tr_(lang, "version.compiler"), sdk.compiler_id.c_str(),
                    sdk.compiler_version.c_str());
        ImGui::Text("%s: %s/%s/%d", tr_(lang, "version.abi"), sdk.abi_cxx.c_str(),
                    sdk.abi_endianness.c_str(), sdk.pointer_bits);
        ImGui::Text("%s: %d", tr_(lang, "version.schema"), sdk.schema);
    }
    std::string detail;
    const VersionCompatibility compatibility = core_.compatibility(&detail);
    if (compatibility == VersionCompatibility::Compatible) {
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.55f, 1.0f), "%s",
                           tr_(lang, "version.compatible"));
    } else if (compatibility == VersionCompatibility::PatchMismatch) {
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f), "%s",
                           trf(lang, "version.patch_warning", detail.c_str()).c_str());
    } else {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s",
                           trf(lang, "version.incompatible", detail.c_str()).c_str());
    }
    // Переключение target — только доступные SDK.
    ImGui::SeparatorText(tr_(lang, "wizard.target"));
    const std::vector<TargetAvailability>& targets = core_.targets();
    for (size_t i = 0; i < targets.size(); ++i) {
        const Platform platform = targets[i].platform;
        const bool selected = project.platform == platform;
        ImGui::BeginDisabled(!targets[i].available);
        if (ImGui::Selectable(tr_(lang, platformNameKey(platform)), selected)) {
            std::string error;
            if (!core_.setProjectTarget(platform, &error)) error_message_ = error;
        }
        ImGui::EndDisabled();
        if (!targets[i].available && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            ImGui::SetTooltip("%s", targets[i].reason_detail.c_str());
        }
    }
}

void GuiApp::drawDiagnostics()
{
    const Lang lang = core_.lang();
    const DiagnosticsReport report = core_.diagnostics();
    for (size_t i = 0; i < report.items.size(); ++i) {
        const Diagnostic& item = report.items[i];
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(ImColor(severityColor(item.severity)).Value, "[%s] %s",
                           severityLabel(lang, item.severity), item.message.c_str());
        ImGui::PopTextWrapPos();
    }
    if (report.items.empty()) {
        ImGui::TextDisabled("%s", tr_(lang, "diag.ok"));
    }
    ImGui::TextDisabled("%s", tr_(lang, "diag.docs"));
}

void GuiApp::drawAssetsTab()
{
    const Lang lang = core_.lang();
    const Project& project = core_.project();
    if (ImGui::Button(tr_(lang, "assets.import"))) {
        requestFileDialog(DialogTarget::ImportAsset, tr_(lang, "assets.file_filter"),
                          "*.gltf;*.glb;*.png;*.jpg;*.jpeg");
    }
    ImGui::Separator();

    if (project.assets.empty()) {
        ImGui::TextDisabled("%s", tr_(lang, "assets.empty"));
        return;
    }
    const ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("##assets", 5, flags)) {
        ImGui::TableSetupColumn(tr_(lang, "assets.name"));
        ImGui::TableSetupColumn(tr_(lang, "assets.type"));
        ImGui::TableSetupColumn(tr_(lang, "assets.size"));
        ImGui::TableSetupColumn(tr_(lang, "assets.details"));
        ImGui::TableSetupColumn("##actions");
        ImGui::TableHeadersRow();
        unsigned texture_slot = 0;
        for (size_t i = 0; i < project.assets.size(); ++i) {
            const AssetEntry& asset = project.assets[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(fileName(asset.source).c_str(), selected_asset_ == (int)i,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                selected_asset_ = (int)i;
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(tr_(lang, assetTypeNameKey(asset.type)));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(formatBytes(asset.size).c_str());
            ImGui::TableNextColumn();
            if (asset.type == AssetType::Mesh) {
                ImGui::Text(tr_(lang, "assets.verts"), asset.vertex_count, asset.triangle_count);
                ImGui::TextDisabled(tr_(lang, "assets.texture_id"), asset.texture_id);
            } else {
                ImGui::Text(tr_(lang, "assets.dims"), asset.width, asset.height);
                ImGui::TextDisabled(tr_(lang, "assets.slot"), texture_slot++);
            }
            ImGui::TableNextColumn();
            if (ImGui::SmallButton(
                    (std::string(tr_(lang, "assets.remove")) + "##" + std::to_string(i)).c_str())) {
                selected_asset_ = (int)i;
                show_confirm_remove_ = true;
            }
        }
        ImGui::EndTable();
    }
}

void GuiApp::drawBuildTab()
{
    const Lang lang = core_.lang();
    const BuildStatus status = core_.buildStatus();
    const bool running = status.state == BuildState::Running;
    if (ImGui::Button(tr_(lang, "build.build"))) startBuild();
    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    if (ImGui::Button(tr_(lang, "build.cancel"))) core_.cancelBuild();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr_(lang, "build.clear"))) build_log_.clear();

    ImGui::Spacing();
    ImGui::Text("%s: %s", tr_(lang, "build.status"), stateLabel(lang, status.state));
    if (status.exit_code != 0) ImGui::TextDisabled("exit %d", status.exit_code);
    if (!status.error.empty()) {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", status.error.c_str());
    }
    const std::vector<std::string> preview = core_.buildCommandPreview();
    ImGui::PushTextWrapPos(0.0f);
    for (size_t i = 0; i < preview.size(); ++i) {
        ImGui::TextDisabled("%s", preview[i].c_str());
    }
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    ImGui::BeginChild("##build_log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (size_t i = 0; i < build_log_.size(); ++i) {
        ImGui::TextUnformatted(build_log_[i].c_str());
    }
    if (scroll_build_) {
        ImGui::SetScrollHereY(1.0f);
        scroll_build_ = false;
    }
    ImGui::EndChild();
}

void GuiApp::drawRunTab()
{
    const Lang lang = core_.lang();
    const RunStatus status = core_.runStatus();
    const bool running = status.state == RunState::Running;
    ImGui::BeginDisabled(running);
    if (ImGui::Button(tr_(lang, "run.run"))) startRun();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    if (ImGui::Button(tr_(lang, "run.stop"))) core_.stopRun();
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (status.state == RunState::Running) {
        ImGui::Text(tr_(lang, "run.running"), static_cast<int>(status.pid));
    } else if (status.state == RunState::Exited) {
        if (status.cancelled) {
            ImGui::TextUnformatted(tr_(lang, "run.stopped"));
        } else {
            ImGui::Text(tr_(lang, "run.exited"), status.exit_code);
        }
    } else if (status.state == RunState::Failed) {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", status.error.c_str());
    } else {
        ImGui::TextUnformatted(tr_(lang, "run.idle"));
    }
    const std::string command = core_.runCommandPreview();
    if (!command.empty()) ImGui::TextDisabled("%s", command.c_str());
    ImGui::Separator();
    ImGui::BeginChild("##run_log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (size_t i = 0; i < run_log_.size(); ++i) {
        ImGui::TextUnformatted(run_log_[i].c_str());
    }
    if (scroll_run_) {
        ImGui::SetScrollHereY(1.0f);
        scroll_run_ = false;
    }
    ImGui::EndChild();
}

void GuiApp::drawWizard()
{
    const Lang lang = core_.lang();
    if (show_wizard_) {
        if (!ImGui::IsPopupOpen("##new_project")) ImGui::OpenPopup("##new_project");
    }
    ImGui::SetNextWindowSize(ImVec2(620.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("##new_project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        show_wizard_ = false;
        return;
    }
    {
        ImGui::TextUnformatted(tr_(lang, "wizard.title"));
        ImGui::Separator();
        inputString(tr_(lang, "wizard.name"), &wizard_name_);
        ImGui::TextDisabled("%s", tr_(lang, "wizard.name_hint"));
        inputString(tr_(lang, "wizard.location"), &wizard_location_);
        ImGui::SameLine();
        if (ImGui::Button((std::string(tr_(lang, "common.browse")) + "##loc").c_str())) {
            requestFolderDialog(DialogTarget::WizardLocation);
        }

        ImGui::TextUnformatted(tr_(lang, "wizard.target"));
        const std::vector<TargetAvailability>& targets = core_.targets();
        for (size_t i = 0; i < targets.size(); ++i) {
            const bool disabled = !targets[i].available;
            ImGui::BeginDisabled(disabled);
            if (ImGui::RadioButton(tr_(lang, platformNameKey(targets[i].platform)),
                                   wizard_target_ == static_cast<int>(i))) {
                wizard_target_ = static_cast<int>(i);
            }
            ImGui::EndDisabled();
            if (disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                ImGui::SetTooltip("%s", targets[i].reason_detail.c_str());
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::TextUnformatted(tr_(lang, "wizard.template"));
        if (ImGui::RadioButton(tr_(lang, "wizard.template.hello"),
                               wizard_template_ == "hello_triangle")) {
            wizard_template_ = "hello_triangle";
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(tr_(lang, "wizard.template.empty"), wizard_template_ == "empty")) {
            wizard_template_ = "empty";
        }
        const Platform selected_platform =
            wizard_target_ >= 0 && wizard_target_ < static_cast<int>(targets.size())
                ? targets[static_cast<size_t>(wizard_target_)].platform
                : Platform::Pc;
        const SdkInfo sdk = findSdkForPlatform(core_.settings(), selected_platform, lang);
        ImGui::Text("%s: %s", tr_(lang, "wizard.sdk"),
                    sdk.valid ? sdk.path.c_str() : tr_(lang, "wizard.sdk_auto"));
        if (!sdk.valid) {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", tr_(lang, "wizard.no_sdk"));
        }
        ImGui::Separator();
        if (ImGui::Button(tr_(lang, "wizard.create"), ImVec2(160.0f, 0.0f))) {
            createProjectFromWizard();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr_(lang, "wizard.cancel"), ImVec2(120.0f, 0.0f))) {
            show_wizard_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (!error_message_.empty()) {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", error_message_.c_str());
        }
        ImGui::EndPopup();
    }
}

void GuiApp::drawSettings()
{
    const Lang lang = core_.lang();
    if (show_settings_ && !ImGui::IsPopupOpen("##settings")) ImGui::OpenPopup("##settings");
    ImGui::SetNextWindowSize(ImVec2(720.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("##settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        show_settings_ = false;
        return;
    }
    ImGui::TextUnformatted(tr_(lang, "settings.title"));
    ImGui::Separator();
    inputString(tr_(lang, "settings.sdk_root"), &settings_edit_.sdk_root);
    inputString(tr_(lang, "settings.sdk_pc"), &settings_edit_.sdk_pc);
    ImGui::SameLine();
    if (ImGui::Button((std::string(tr_(lang, "common.browse")) + "##sdpc").c_str())) {
        requestFolderDialog(DialogTarget::SdkPc);
    }
    inputString(tr_(lang, "settings.sdk_ps1"), &settings_edit_.sdk_ps1);
    ImGui::SameLine();
    if (ImGui::Button((std::string(tr_(lang, "common.browse")) + "##sdps1").c_str())) {
        requestFolderDialog(DialogTarget::SdkPs1);
    }
    inputString(tr_(lang, "settings.sdk_saturn"), &settings_edit_.sdk_saturn);
    ImGui::SameLine();
    if (ImGui::Button((std::string(tr_(lang, "common.browse")) + "##sdsat").c_str())) {
        requestFolderDialog(DialogTarget::SdkSaturn);
    }
    ImGui::Separator();
    inputString(tr_(lang, "settings.cmake"), &settings_edit_.cmake_path);
    inputString(tr_(lang, "settings.ninja"), &settings_edit_.ninja_path);
    inputString(tr_(lang, "settings.emulator_ps1"), &settings_edit_.emulator_ps1);
    ImGui::SameLine();
    if (ImGui::Button((std::string(tr_(lang, "common.browse")) + "##emups1").c_str())) {
        requestFileDialog(DialogTarget::EmulatorPs1, "Emulator", "*");
    }
    inputString(tr_(lang, "settings.emulator_saturn"), &settings_edit_.emulator_saturn);
    ImGui::SameLine();
    if (ImGui::Button((std::string(tr_(lang, "common.browse")) + "##emusat").c_str())) {
        requestFileDialog(DialogTarget::EmulatorSaturn, "Emulator", "*");
    }
    ImGui::Separator();
    if (ImGui::BeginCombo(tr_(lang, "settings.theme"), settings_edit_.theme == "light"
                                                           ? tr_(lang, "theme.light")
                                                           : tr_(lang, "theme.dark"))) {
        if (ImGui::Selectable(tr_(lang, "theme.dark"), settings_edit_.theme == "dark")) {
            settings_edit_.theme = "dark";
        }
        if (ImGui::Selectable(tr_(lang, "theme.light"), settings_edit_.theme == "light")) {
            settings_edit_.theme = "light";
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginCombo(tr_(lang, "settings.language"),
                          settings_edit_.language == Lang::En ? "English" : "Русский")) {
        if (ImGui::Selectable("Русский", settings_edit_.language == Lang::Ru)) {
            settings_edit_.language = Lang::Ru;
        }
        if (ImGui::Selectable("English", settings_edit_.language == Lang::En)) {
            settings_edit_.language = Lang::En;
        }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled(tr_(lang, "settings.config_path"), settingsFilePath().c_str());
    if (!settings_error_.empty()) {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", settings_error_.c_str());
    }
    ImGui::Separator();
    if (ImGui::Button(tr_(lang, "settings.save"), ImVec2(160.0f, 0.0f))) {
        core_.mutableSettings() = settings_edit_;
        core_.setLang(settings_edit_.language);
        core_.refreshTargets();
        applyTheme();
        if (core_.saveSettings(&settings_error_)) {
            show_settings_ = false;
            info_message_ = tr_(lang, "settings.saved");
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr_(lang, "settings.cancel"), ImVec2(120.0f, 0.0f))) {
        settings_edit_ = core_.settings();
        show_settings_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void GuiApp::drawAbout()
{
    const Lang lang = core_.lang();
    if (show_about_ && !ImGui::IsPopupOpen("##about")) ImGui::OpenPopup("##about");
    if (ImGui::BeginPopupModal("##about", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(tr_(lang, "menu.about"));
        ImGui::Separator();
        ImGui::Text(tr_(lang, "about.text"), uiVersion());
        ImGui::TextDisabled("SDL3 %d.%d.%d; Dear ImGui v1.93.0", SDL_MAJOR_VERSION,
                            SDL_MINOR_VERSION, SDL_MICRO_VERSION);
        if (ImGui::Button(tr_(lang, "common.close"))) {
            show_about_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else {
        show_about_ = false;
    }
}

void GuiApp::drawStatusBar()
{
    const Lang lang = core_.lang();
    const ImGuiIO& io = ImGui::GetIO();
    const float height = ImGui::GetFrameHeight() + 4.0f;
    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - height));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, height));
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::Begin("##status", nullptr, flags);
    const BuildStatus build = core_.buildStatus();
    const RunStatus run = core_.runStatus();
    ImGui::Text("%s: %s", tr_(lang, "build.status"), stateLabel(lang, build.state));
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    const bool run_running = run.state == RunState::Running;
    ImGui::Text("%s: %s", tr_(lang, "run.status"),
                run_running ? tr_(lang, "status.running") : tr_(lang, "run.idle"));
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (!error_message_.empty()) {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", error_message_.c_str());
    } else if (!info_message_.empty()) {
        ImGui::TextUnformatted(info_message_.c_str());
    } else {
        ImGui::TextDisabled("%s %s", "RetroPolyEngine UI", uiVersion());
    }
    ImGui::End();
}

void GuiApp::requestOpenProjectDialog()
{
    requestFileDialog(DialogTarget::Project, "RetroPolyEngine project", "*.rpeproject");
}

void GuiApp::requestFolderDialog(DialogTarget target)
{
    {
        std::lock_guard<std::mutex> lock(dialog_mutex_);
        requested_target_ = target;
    }
    SDL_ShowOpenFolderDialog(&GuiApp::fileDialogCallback, this, window_, nullptr, false);
}

void GuiApp::requestFileDialog(DialogTarget target, const char* filter_name, const char* pattern)
{
    {
        std::lock_guard<std::mutex> lock(dialog_mutex_);
        requested_target_ = target;
    }
    SDL_DialogFileFilter filter;
    filter.name = filter_name;
    filter.pattern = pattern;
    SDL_ShowOpenFileDialog(&GuiApp::fileDialogCallback, this, window_, &filter, 1, nullptr, false);
}

void GuiApp::fileDialogCallback(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    GuiApp* app = static_cast<GuiApp*>(userdata);
    if (app == nullptr || filelist == nullptr || filelist[0] == nullptr) return;
    std::lock_guard<std::mutex> lock(app->dialog_mutex_);
    app->pending_target_ = app->requested_target_;
    app->pending_path_ = filelist[0];
}

void GuiApp::openProject(const std::string& path)
{
    std::string error;
    if (!core_.openProject(path, &error)) {
        error_message_ = error;
        return;
    }
    screen_ = Screen::Project;
    settings_edit_ = core_.settings();
    selected_asset_ = -1;
    build_log_.clear();
    run_log_.clear();
    error_message_.clear();
    info_message_ = path;
}

void GuiApp::saveProject()
{
    std::string error;
    if (core_.saveProject(&error)) {
        info_message_ = trf(core_.lang(), "project.save_ok", core_.projectFile().c_str());
    } else {
        error_message_ = trf(core_.lang(), "project.save_failed", error.c_str());
    }
}

void GuiApp::createProjectFromWizard()
{
    NewProjectOptions options;
    options.name = wizard_name_;
    options.location = wizard_location_;
    const std::vector<TargetAvailability>& targets = core_.targets();
    if (wizard_target_ >= 0 && wizard_target_ < static_cast<int>(targets.size())) {
        options.platform = targets[static_cast<size_t>(wizard_target_)].platform;
    }
    options.template_name = wizard_template_;
    std::string error;
    if (!core_.newProject(options, &error)) {
        error_message_ = error;
        return;
    }
    show_wizard_ = false;
    screen_ = Screen::Project;
    settings_edit_ = core_.settings();
    selected_asset_ = -1;
    build_log_.clear();
    run_log_.clear();
    error_message_.clear();
    info_message_ = core_.projectRootPath();
    if (ImGui::IsPopupOpen("##new_project")) ImGui::CloseCurrentPopup();
}

void GuiApp::importAsset(const std::string& path)
{
    const AssetImportResult result = core_.importAsset(path);
    if (result.ok) {
        info_message_ = trf(core_.lang(), "assets.imported", fileName(result.entry.source).c_str(),
                            result.entry.output.c_str(), formatBytes(result.entry.size).c_str());
        error_message_.clear();
    } else {
        error_message_ =
            trf(core_.lang(), "assets.import_failed", fileName(path).c_str(), result.error.c_str());
    }
}

void GuiApp::removeSelectedAsset()
{
    if (selected_asset_ < 0) return;
    std::string error;
    if (core_.removeAsset(static_cast<size_t>(selected_asset_), true, &error)) {
        info_message_ = tr_(core_.lang(), "assets.removed");
        selected_asset_ = -1;
    } else {
        error_message_ = error;
    }
    show_confirm_remove_ = false;
}

void GuiApp::startBuild()
{
    tab_ = 1;
    build_log_.clear();
    std::string error;
    if (!core_.startBuild(&error)) {
        error_message_ = error;
    } else {
        error_message_.clear();
        info_message_ = tr_(core_.lang(), "build.running");
    }
}

void GuiApp::startRun()
{
    tab_ = 2;
    run_log_.clear();
    std::string error;
    if (!core_.startRun(&error)) {
        error_message_ = error;
    } else {
        error_message_.clear();
        info_message_ = tr_(core_.lang(), "run.started");
    }
}

int GuiApp::runSmoke(const SmokeOptions& options)
{
    screenshot_dir_ = options.screenshot_dir;
    run_seconds_ = options.run_seconds;
    if (!screenshot_dir_.empty()) createDirectories(screenshot_dir_, nullptr);

    renderFrame(!screenshot_dir_.empty(),
                screenshot_dir_.empty() ? "" : joinPath(screenshot_dir_, "01_welcome.png"));

    std::string work_dir = options.work_dir;
    if (work_dir.empty()) work_dir = joinPath(tempDirectory(), "rpe_ui_smoke");
    createDirectories(work_dir, nullptr);
    std::string project_dir = joinPath(work_dir, "smoke_project");
    for (int attempt = 0; isDirectory(project_dir) && !listDirectorySorted(project_dir).empty();
         ++attempt) {
        project_dir = joinPath(work_dir, "smoke_project_" + std::to_string(attempt + 1));
    }

    NewProjectOptions project_options;
    project_options.name = fileName(project_dir);
    project_options.location = parentPath(project_dir);
    project_options.platform = options.platform;
    project_options.template_name = "hello_triangle";
    std::string error;
    if (!core_.newProject(project_options, &error)) {
        std::fprintf(stderr, "[smoke] new project failed: %s\n", error.c_str());
        return 1;
    }
    screen_ = Screen::Project;
    settings_edit_ = core_.settings();
    std::printf("[smoke] target: %s (%s)\n", core_.project().target.c_str(),
                platformCode(core_.project().platform));
    std::printf("[smoke] project: %s\n", core_.projectRootPath().c_str());
    renderFrame(!screenshot_dir_.empty(),
                screenshot_dir_.empty() ? "" : joinPath(screenshot_dir_, "02_project.png"));

    int imported = 0;
    if (!options.samples_dir.empty() && isDirectory(options.samples_dir)) {
        const std::vector<std::string> files = listDirectorySorted(options.samples_dir);
        for (size_t i = 0; i < files.size(); ++i) {
            const std::string extension = fileExtension(files[i]);
            if (extension == ".gltf" || extension == ".glb" || extension == ".png" ||
                extension == ".jpg" || extension == ".jpeg") {
                importAsset(joinPath(options.samples_dir, files[i]));
                if (error_message_.empty()) ++imported;
            }
        }
    }
    std::printf("[smoke] assets imported: %d\n", imported);
    tab_ = 0;
    renderFrame(!screenshot_dir_.empty(),
                screenshot_dir_.empty() ? "" : joinPath(screenshot_dir_, "03_assets.png"));

    tab_ = 1;
    startBuild();
    if (!error_message_.empty()) {
        std::fprintf(stderr, "[smoke] build start failed: %s\n", error_message_.c_str());
        return 1;
    }
    bool captured_build = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(600);
    while (core_.buildStatus().state == BuildState::Running) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            ImGui_ImplSDL3_ProcessEvent(&event);
        pollCore();
        if (!captured_build) {
            renderFrame(!screenshot_dir_.empty(),
                        screenshot_dir_.empty() ? "" : joinPath(screenshot_dir_, "04_build.png"));
            captured_build = true;
        } else {
            renderFrame(false, std::string());
        }
        if (std::chrono::steady_clock::now() > deadline) {
            std::fprintf(stderr, "[smoke] build timeout\n");
            core_.cancelBuild();
            return 1;
        }
        SDL_Delay(20);
    }
    pollCore();
    const BuildStatus build = core_.buildStatus();
    if (build.state != BuildState::Succeeded) {
        std::fprintf(stderr, "[smoke] build failed (state=%d, exit=%d)\n",
                     static_cast<int>(build.state), build.exit_code);
        const size_t begin = build_log_.size() > 20 ? build_log_.size() - 20 : 0;
        for (size_t i = begin; i < build_log_.size(); ++i) {
            std::fprintf(stderr, "%s\n", build_log_[i].c_str());
        }
        return 1;
    }
    std::printf("[smoke] build: ok (exit 0)\n");
    renderFrame(!screenshot_dir_.empty(),
                screenshot_dir_.empty() ? "" : joinPath(screenshot_dir_, "05_build_done.png"));

    if (options.do_run) {
        tab_ = 2;
        startRun();
        if (!error_message_.empty()) {
            std::fprintf(stderr, "[smoke] run failed: %s\n", error_message_.c_str());
            return 1;
        }
        const auto run_deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(run_seconds_);
        while (std::chrono::steady_clock::now() < run_deadline) {
            SDL_Event event;
            while (SDL_PollEvent(&event))
                ImGui_ImplSDL3_ProcessEvent(&event);
            pollCore();
            renderFrame(false, std::string());
            SDL_Delay(20);
        }
        renderFrame(!screenshot_dir_.empty(),
                    screenshot_dir_.empty() ? "" : joinPath(screenshot_dir_, "06_run.png"));
        core_.stopRun();
        for (int i = 0; i < 100 && core_.runStatus().state == RunState::Running; ++i) {
            pollCore();
            SDL_Delay(20);
        }
        pollCore();
        std::printf("[smoke] run: started and stopped\n");
    }

    std::printf("[smoke] PASS\n");
    if (!options.keep_project) {
        core_.closeProject();
    }
    return 0;
}

} // namespace ui
} // namespace rpe
