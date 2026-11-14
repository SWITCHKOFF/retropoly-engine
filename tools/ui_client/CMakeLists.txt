# ============================================================================
# RetroPolyEngine UI client — Спринт 35–36 (недели 69–72).
#
# Отдельный host-target retro_engine_ui (SDL3 + Dear ImGui, MIT).
# Не линкует demo-примеры и игровое дерево: работает с установленным SDK
# через sdk_manifest.json, asset_converter и CMake-процессы.
#
# Пресеты: ui-debug (тесты), ui-release (пакет).
# ============================================================================

if(NOT PLATFORM_PC)
    message(FATAL_ERROR "UI client собирается только для PLATFORM_PC (host).")
endif()

find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)

set(RPE_UI_IMGUI_DIR "${CMAKE_SOURCE_DIR}/src/third_party/imgui")
if(NOT EXISTS "${RPE_UI_IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR
        "src/third_party/imgui пуст (submodule не инициализирован). "
        "Запустите 'git submodule update --init --recursive'.")
endif()

execute_process(
    COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse --short HEAD
    OUTPUT_VARIABLE RPE_UI_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT RPE_UI_GIT_COMMIT)
    set(RPE_UI_GIT_COMMIT "unknown")
endif()

# --- Не-GUI ядро: project/sdk/commands/process/assets/diagnostics ------------
add_library(retro_engine_ui_core STATIC
    core/app_core.cpp
    core/asset_import.cpp
    core/command.cpp
    core/diagnostics.cpp
    core/fs_util.cpp
    core/i18n.cpp
    core/json.cpp
    core/process.cpp
    core/project.cpp
    core/sdk.cpp
    core/settings.cpp
    core/version.cpp
)
target_compile_features(retro_engine_ui_core PUBLIC cxx_std_17)
target_include_directories(retro_engine_ui_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/core")
target_link_libraries(retro_engine_ui_core PUBLIC Threads::Threads)
target_compile_definitions(retro_engine_ui_core PRIVATE
    RPE_UI_VERSION="${PROJECT_VERSION}"
    RPE_UI_GIT_COMMIT="${RPE_UI_GIT_COMMIT}")
retro_engine_apply_flags(retro_engine_ui_core)

# --- Dear ImGui (third-party: без -Werror) -----------------------------------
add_library(rpe_imgui STATIC
    "${RPE_UI_IMGUI_DIR}/imgui.cpp"
    "${RPE_UI_IMGUI_DIR}/imgui_draw.cpp"
    "${RPE_UI_IMGUI_DIR}/imgui_tables.cpp"
    "${RPE_UI_IMGUI_DIR}/imgui_widgets.cpp"
    "${RPE_UI_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${RPE_UI_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_compile_features(rpe_imgui PUBLIC cxx_std_17)
target_include_directories(rpe_imgui SYSTEM PUBLIC
    "${RPE_UI_IMGUI_DIR}"
    "${RPE_UI_IMGUI_DIR}/backends")
target_link_libraries(rpe_imgui PUBLIC SDL3::SDL3 OpenGL::GL)
target_compile_definitions(rpe_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

# --- GUI-executable ----------------------------------------------------------
add_executable(retro_engine_ui
    main.cpp
    gui/gui_app.cpp
    gui/screenshot.cpp
)
target_compile_features(retro_engine_ui PRIVATE cxx_std_17)
target_include_directories(retro_engine_ui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/gui"
    "${RPE_UI_IMGUI_DIR}"
    "${RPE_UI_IMGUI_DIR}/backends")
target_include_directories(retro_engine_ui SYSTEM PRIVATE
    "${CMAKE_SOURCE_DIR}/src/third_party/stb")
target_link_libraries(retro_engine_ui PRIVATE
    retro_engine_ui_core
    rpe_imgui
    SDL3::SDL3
    OpenGL::GL
    glad)
target_compile_definitions(retro_engine_ui PRIVATE
    RPE_UI_VERSION="${PROJECT_VERSION}"
    RPE_UI_GIT_COMMIT="${RPE_UI_GIT_COMMIT}"
    RPE_UI_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
retro_engine_apply_flags(retro_engine_ui)

set_target_properties(retro_engine_ui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ui")
if(WIN32)
    set_target_properties(retro_engine_ui PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

install(TARGETS retro_engine_ui RUNTIME DESTINATION bin)

# --- Unit-тесты ядра (без GUI) ----------------------------------------------
if(BUILD_TESTS AND TARGET GTest::gtest_main)
    add_executable(retro_engine_ui_tests
        tests/app_core_test.cpp
        tests/asset_import_test.cpp
        tests/command_test.cpp
        tests/json_test.cpp
        tests/process_test.cpp
        tests/project_test.cpp
        tests/sdk_test.cpp
        tests/settings_test.cpp
        tests/version_test.cpp
    )
    target_compile_features(retro_engine_ui_tests PRIVATE cxx_std_17)
    target_link_libraries(retro_engine_ui_tests PRIVATE
        retro_engine_ui_core GTest::gtest_main)
    add_dependencies(retro_engine_ui_tests asset_converter)
    target_compile_definitions(retro_engine_ui_tests PRIVATE
        RPE_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        RPE_TEST_ASSET_CONVERTER="$<TARGET_FILE:asset_converter>")
    retro_engine_apply_flags(retro_engine_ui_tests)
    include(GoogleTest)
    gtest_discover_tests(retro_engine_ui_tests DISCOVERY_TIMEOUT 60)
endif()
