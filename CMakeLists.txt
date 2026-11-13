# ============================================================================
# RetroPolyEngine — корневой CMakeLists.txt
#
# Нормативные источники:
#   - docs/plans/sprint_00.md §3.5 (Система сборки, каркас)
#   - docs/plans/sprint_21_22.md (Saturn skeleton, ADR-008)
#   - docs/RetroEngine_Engineering_Documentation_v1.43.md: BUILD PRECEDENCE —
#     «CMakePresets.json is the only canonical build entry point»
#
# Правила:
#   - ровно один бэкенд (PLATFORM_PC XOR PLATFORM_PS1 XOR PLATFORM_SATURN);
#   - hello_triangle / rotating_cube ссылаются только на retro_engine
#     (никогда напрямую на SDL3/OpenGL/PSn00bSDK/Jo Engine);
#   - asset_converter — host-only, не собирается кросс-компиляторами;
#   - все сборки через канонические пресеты CMakePresets.json.
# ============================================================================

cmake_minimum_required(VERSION 3.25)

project(retro_engine
    VERSION 0.2.1
    DESCRIPTION "RetroPolyEngine — deterministic retro engine (PC: SDL3 + OpenGL 3.3; PS1: PSn00bSDK v0.24; Saturn: Jo Engine v2026.2)"
    LANGUAGES C CXX ASM
)

# --- SDK: установка/экспорт (Спринт 33–34, ADR-011) -------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
# SDK layout стабилен на всех дистрибутивах: lib/, не lib64/lib/<triplet>.
set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING
    "SDK install libdir (stable layout: lib/)" FORCE)

if(PLATFORM_PC)
    set(RPE_PLATFORM_NAME "pc")
elseif(PLATFORM_PS1)
    set(RPE_PLATFORM_NAME "ps1")
elseif(PLATFORM_SATURN)
    set(RPE_PLATFORM_NAME "saturn")
endif()

# Нормализованная архитектура для layout lib/<platform>/<arch> (RELEASING §2.1).
set(RPE_PLATFORM_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
if(NOT RPE_PLATFORM_ARCH)
    set(RPE_PLATFORM_ARCH "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()
string(TOLOWER "${RPE_PLATFORM_ARCH}" RPE_PLATFORM_ARCH)
if(RPE_PLATFORM_ARCH MATCHES "^(amd64|x86_64|x64)$")
    set(RPE_PLATFORM_ARCH "x86_64")
elseif(RPE_PLATFORM_ARCH MATCHES "^(aarch64|arm64)$")
    set(RPE_PLATFORM_ARCH "arm64")
elseif(RPE_PLATFORM_ARCH MATCHES "^(mips|mipsel)$")
    set(RPE_PLATFORM_ARCH "mips")
elseif(RPE_PLATFORM_ARCH MATCHES "^(sh|sh2|sh-2)$")
    set(RPE_PLATFORM_ARCH "sh2")
endif()

set(RPE_SDK_LIBDIR "lib/${RPE_PLATFORM_NAME}/${RPE_PLATFORM_ARCH}" CACHE STRING
    "SDK library layout: lib/<platform>/<arch> (RELEASING §2.1)")
set(RPE_INSTALL_CMAKEDIR "lib/cmake/RetroPolyEngine")
set(RPE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")

# Публичный include-корень: <prefix>/include (рекомендуется <retro_engine/...>)
# и <prefix>/include/retro_engine (совместимость с внутренним стилем include
# "core/..." — тот же набор public-заголовков, internal не устанавливаются).
set(RPE_PUBLIC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(RPE_PUBLIC_API_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/retro_engine")
set(RPE_VERSION_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include")
set(RPE_GENERATED_INCLUDE_DIR "${RPE_VERSION_INCLUDE_DIR}")
set(RPE_SDL3_INSTALL_CMAKEDIR "lib/cmake/SDL3")
set(RPE_SDL3_BUILD_CONFIG_DIR "${CMAKE_BINARY_DIR}/src/third_party/sdl3")

if(PLATFORM_PC)
    if(APPLE)
        set(RPE_PKGCONFIG_PRIVATE_LIBS "-lSDL3 -lglad -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo -framework Metal")
    elseif(WIN32)
        set(RPE_PKGCONFIG_PRIVATE_LIBS "-lSDL3 -lglad -lopengl32")
    else()
        set(RPE_PKGCONFIG_PRIVATE_LIBS "-lSDL3 -lglad -lGL -ldl -lpthread -lm")
    endif()
else()
    set(RPE_PKGCONFIG_PRIVATE_LIBS "")
endif()
set(RPE_PKGCONFIG_PLATFORM_LIBS "")

configure_file("${CMAKE_SOURCE_DIR}/cmake/version.h.in"
               "${RPE_VERSION_INCLUDE_DIR}/retro_engine/version.h" @ONLY)

# --- Опции сборки ------------------------------------------------------------
option(PLATFORM_PC     "Собирать целевой бэкенд PC (SDL3 + OpenGL 3.3)"             OFF)
option(PLATFORM_PS1    "Собирать целевой бэкенд PS1 (PSn00bSDK v0.24, pinned)"      OFF)
option(PLATFORM_SATURN "Собирать целевой бэкенд Saturn (Jo Engine v2026.2, pinned)" OFF)
option(BUILD_TESTS                       "Собирать юнит-тесты (GoogleTest)"               ON)
option(BUILD_EXAMPLES                    "Собирать примеры hello_triangle/rotating_cube"  OFF)
option(BUILD_UI_CLIENT                   "Собирать UI client (SDL3 + Dear ImGui, PC host)" OFF)
option(BUILD_RENDER_INTEGRATION_TESTS    "Рендер-интеграционные тесты (W0-golden, §3.5)"  OFF)
option(BUILD_BENCHMARKS                  "Собирать benchmarks (пресет pc-bench)"          OFF)
option(ENABLE_ASAN    "AddressSanitizer для целей движка (только PC)"         OFF)
option(ENABLE_COVERAGE "Покрытие кода тестами (только PC, пресет pc-coverage)" OFF)

# --- Guard: ровно один бэкенд -------------------------------------------------
set(RPE_PLATFORM_COUNT 0)
foreach(_rpe_platform PLATFORM_PC PLATFORM_PS1 PLATFORM_SATURN)
    if(${_rpe_platform})
        math(EXPR RPE_PLATFORM_COUNT "${RPE_PLATFORM_COUNT} + 1")
    endif()
endforeach()
if(NOT RPE_PLATFORM_COUNT EQUAL 1)
    message(FATAL_ERROR
        "Не выбран ровно один бэкенд (PLATFORM_PC/PLATFORM_PS1/PLATFORM_SATURN): "
        "включённых платформ — ${RPE_PLATFORM_COUNT}. См. CMakePresets.json.")
endif()
if(BUILD_UI_CLIENT AND NOT PLATFORM_PC)
    message(FATAL_ERROR "BUILD_UI_CLIENT=ON требует PLATFORM_PC=ON (host UI client).")
endif()

# --- Канонические флаги компиляции (cmake/CompilerFlags.cmake) ----------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerFlags)

# --- Saturn: target-заголовки newlib из pinned Jo Engine SDK ------------------
# Кросс-компилятор не должен видеть host /usr/include. Заголовки newlib
# лежат в bundled-компиляторе Jo Engine (Compiler/WINDOWS/sh-elf/include) и
# include-fixed GCC 8.2.0; подключаются как SYSTEM (не триггерят -Werror).
if(PLATFORM_SATURN)
    if(NOT DEFINED JO_ENGINE_NEWLIB_INCLUDE OR NOT DEFINED JO_ENGINE_GCC_INCLUDE_FIXED)
        message(FATAL_ERROR
            "Saturn toolchain не задал JO_ENGINE_NEWLIB_INCLUDE/JO_ENGINE_GCC_INCLUDE_FIXED. "
            "Сборка — только через пресет saturn-release (cmake/saturn-toolchain.cmake).")
    endif()
    include_directories(SYSTEM
        "${JO_ENGINE_NEWLIB_INCLUDE}"
        "${JO_ENGINE_GCC_INCLUDE_FIXED}")
endif()

# --- Вендоренные зависимости (§3.6) -------------------------------------------
# SDL3 подключается ДО ядра: HAL (src/hal/pc) линкует target SDL3::SDL3.
# SDL — статическая (CI-гейт build-pc: gtest_discover_tests запускает
# тестовый exe в configure-фазе ПОСЛЕ пост-билда; с SDL3-shared.dll не в
# PATH discovery падает «Error running test executable» на MSVC).
# Движок сам static-тополог: SDL3-static вписывается в ту же модель.
if(PLATFORM_PC)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/sdl3/CMakeLists.txt")
        set(SDL_SHARED OFF) # Только static: без DLL-развертывания.
        set(SDL_STATIC ON)
        # SDK (S33–34): SDL3 устанавливается в тот же prefix; публичный API
        # движка SDL не тянет, но PC-библиотекам нужен SDL3 при линковке.
        # Cache FORCE: пресеты уже могли закэшировать SDL_INSTALL=OFF.
        set(SDL_INSTALL ON CACHE BOOL "Install SDL3 into the SDK prefix" FORCE)
        # IBus-заголовки в CI/минимальных окружениях часто отсутствуют;
        # ввод движка от IBus не зависит.
        set(SDL_IBUS OFF CACHE BOOL "Disable SDL3 IBus (optional)" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not install SDL3_test" FORCE)
        set(SDL_INSTALL_TESTS OFF CACHE BOOL "Do not install SDL3 tests" FORCE)
        set(SDL_INSTALL_CMAKEDIR_ROOT "lib/cmake/SDL3" CACHE STRING
            "SDL3 CMake package dir inside the SDK" FORCE)
        add_subdirectory(src/third_party/sdl3)
    else()
        message(FATAL_ERROR
            "src/third_party/sdl3 пуст (submodule не инициализирован), но HAL PC "
            "(Спринт 5–6) требует SDL3::SDL3. Запустите "
            "'git submodule update --init --recursive'.")
    endif()
endif()

# --- Ядро движка: src/CMakeLists.txt выбирает ровно один HAL ------------------
add_subdirectory(src)

if(PLATFORM_PC)
    # asset_converter — host-only инструмент, никогда не компилируется MIPS-тулчейном.
    add_subdirectory(tools/asset_converter)

    # --- Ассеты примеров: конвертация источников (Спринт 9–10, неделя 20, §4.5)
    # Выходы — assets/build/ (детерминированные; коммитятся как versioned‑ассеты
    # для PS1-checkout, Спринт 15–16; PC-сборка перегенерирует при изменении).
    set(RPE_ASSET_SOURCE "${CMAKE_SOURCE_DIR}/assets/source")
    set(RPE_ASSET_BUILD "${CMAKE_SOURCE_DIR}/assets/build")
    if(EXISTS "${RPE_ASSET_SOURCE}/cube.gltf" AND EXISTS "${RPE_ASSET_SOURCE}/texture.png")
        add_custom_command(
            OUTPUT
                "${RPE_ASSET_BUILD}/cube.mesh"
                "${RPE_ASSET_BUILD}/texture.tex"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${RPE_ASSET_BUILD}"
            COMMAND asset_converter
                "${RPE_ASSET_SOURCE}/cube.gltf"
                "${RPE_ASSET_BUILD}/cube.mesh"
            COMMAND asset_converter
                "${RPE_ASSET_SOURCE}/texture.png"
                "${RPE_ASSET_BUILD}/texture.tex"
            DEPENDS
                asset_converter
                "${RPE_ASSET_SOURCE}/cube.gltf"
                "${RPE_ASSET_SOURCE}/texture.png"
            COMMENT "asset_converter: cube.gltf → cube.mesh, texture.png → texture.tex"
            VERBATIM)
        add_custom_target(convert_assets ALL
            DEPENDS
                "${RPE_ASSET_BUILD}/cube.mesh"
                "${RPE_ASSET_BUILD}/texture.tex")
    endif()
    # Ассеты примера rotating_cube (Спринт 17–18, неделя 33): cube.gltf →
    # .mesh v1 и ЧЕТЫРЕ текстуры-варианта (по одной на куб; ≤ 1 текстуры
    # 256×256 на куб — docs/plans/sprint_17_18.md). Меш один на 4 куба:
    # W1-геометрия однородна (±1 куб, 12 треугольников), банк мешей
    # переиспользуется через MeshRenderer::mesh_id — RAM/VRAM-бюджет W1.
    if(EXISTS "${RPE_ASSET_SOURCE}/cube.gltf")
        foreach(rc_variant 0 1 2 3)
            set(rc_tex_src "${RPE_ASSET_SOURCE}/texture_${rc_variant}.png")
            set(rc_tex_out "${RPE_ASSET_BUILD}/texture_${rc_variant}.tex")
            add_custom_command(
                OUTPUT "${rc_tex_out}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${RPE_ASSET_BUILD}"
                COMMAND asset_converter "${rc_tex_src}" "${rc_tex_out}"
                DEPENDS asset_converter "${rc_tex_src}"
                COMMENT "asset_converter: texture_${rc_variant}.png → texture_${rc_variant}.tex"
                VERBATIM)
            list(APPEND RC_TEXTURE_OUTPUTS "${rc_tex_out}")
        endforeach()
        add_custom_target(convert_rotating_cube_assets ALL
            DEPENDS "${RPE_ASSET_BUILD}/cube.mesh" ${RC_TEXTURE_OUTPUTS})
    endif()
    # Ассет примера hello_triangle (Спринт 11–12, неделя 21): triangle.gltf →
    # .mesh v1. Текстура примера — та же texture.tex (256×256 паттерн).
    if(EXISTS "${RPE_ASSET_SOURCE}/triangle.gltf")
        add_custom_command(
            OUTPUT
                "${RPE_ASSET_BUILD}/triangle.mesh"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${RPE_ASSET_BUILD}"
            COMMAND asset_converter
                "${RPE_ASSET_SOURCE}/triangle.gltf"
                "${RPE_ASSET_BUILD}/triangle.mesh"
            DEPENDS
                asset_converter
                "${RPE_ASSET_SOURCE}/triangle.gltf"
            COMMENT "asset_converter: triangle.gltf → triangle.mesh"
            VERBATIM)
        add_custom_target(convert_triangle_asset ALL
            DEPENDS
                "${RPE_ASSET_BUILD}/triangle.mesh")
    endif()
endif()

# --- Примеры: зависят только от retro_engine (ни от SDL3/OpenGL/PSn00bSDK) ---
# PC-only: PS1-конфигурация публикует PS-EXE через HAL
# (src/hal/ps1/CMakeLists.txt: retro_ps1 → retro_disc, Спринты 13–14).
if(PLATFORM_PC)
    add_subdirectory(examples/hello_triangle)
    add_subdirectory(examples/rotating_cube)

    # Каноническое расположение рантайм-бинарей примеров: build/<preset>/game/
    set_target_properties(hello_triangle rotating_cube PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/game")

    # CTest агрегирует тесты рекурсивно (ctest --preset pc-debug из build/<preset>).
    enable_testing()
    add_subdirectory(tests)

    # UI client — отдельный host-target retro_engine_ui (S35–36).
    if(BUILD_UI_CLIENT)
        add_subdirectory(tools/ui_client)
    endif()
endif()

# ============================================================================
# SDK install/export (Спринт 33–34, ADR-011, docs/SDK.md)
# ============================================================================

# Публичные заголовки (internal-заголовки не устанавливаются by design).
install(DIRECTORY "${RPE_PUBLIC_INCLUDE_DIR}/retro_engine"
        DESTINATION "${RPE_INSTALL_INCLUDEDIR}")
install(FILES "${RPE_VERSION_INCLUDE_DIR}/retro_engine/version.h"
        DESTINATION "${RPE_INSTALL_INCLUDEDIR}/retro_engine")

# CMake package: find_package(RetroPolyEngine CONFIG REQUIRED).
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/RetroPolyEngineConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/cmake/RetroPolyEngineConfig.cmake"
    INSTALL_DESTINATION "${RPE_INSTALL_CMAKEDIR}")
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/cmake/RetroPolyEngineConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)
install(FILES
        "${CMAKE_BINARY_DIR}/cmake/RetroPolyEngineConfig.cmake"
        "${CMAKE_BINARY_DIR}/cmake/RetroPolyEngineConfigVersion.cmake"
        DESTINATION "${RPE_INSTALL_CMAKEDIR}")
install(EXPORT RetroPolyEngineTargets
        FILE RetroPolyEngineTargets.cmake
        NAMESPACE RetroPolyEngine::
        DESTINATION "${RPE_INSTALL_CMAKEDIR}")

# Build-tree package: find_package(RetroPolyEngine CONFIG PATHS <build-dir>).
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/RetroPolyEngineBuildTreeConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/RetroPolyEngineConfig.cmake" @ONLY)
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/RetroPolyEngineConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)
export(EXPORT RetroPolyEngineTargets
       NAMESPACE RetroPolyEngine::
       FILE "${CMAKE_BINARY_DIR}/RetroPolyEngineTargets.cmake")

# pkg-config (SDK [H]): заголовки/либы и статические зависимости.
configure_file("${CMAKE_SOURCE_DIR}/cmake/retro_engine.pc.in"
               "${CMAKE_BINARY_DIR}/retro_engine.pc" @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/retro_engine.pc"
        DESTINATION "lib/pkgconfig")

# Примеры и шаблон проекта устанавливаются ИСХОДНИКАМИ и собираются только
# против установленного SDK (standalone CMakeLists с find_package).
if(PLATFORM_PC)
    install(DIRECTORY examples/hello_triangle
            DESTINATION examples
            PATTERN "CMakeLists.txt" EXCLUDE)
    install(DIRECTORY examples/rotating_cube
            DESTINATION examples
            PATTERN "CMakeLists.txt" EXCLUDE)
    install(FILES "${CMAKE_SOURCE_DIR}/cmake/sdk-examples/hello_triangle-CMakeLists.txt"
            DESTINATION examples/hello_triangle RENAME CMakeLists.txt)
    install(FILES "${CMAKE_SOURCE_DIR}/cmake/sdk-examples/rotating_cube-CMakeLists.txt"
            DESTINATION examples/rotating_cube RENAME CMakeLists.txt)
    install(FILES
            "${CMAKE_SOURCE_DIR}/assets/build/triangle.mesh"
            "${CMAKE_SOURCE_DIR}/assets/build/texture.tex"
            DESTINATION examples/hello_triangle/assets)
    install(FILES
            "${CMAKE_SOURCE_DIR}/assets/build/cube.mesh"
            "${CMAKE_SOURCE_DIR}/assets/build/texture_0.tex"
            "${CMAKE_SOURCE_DIR}/assets/build/texture_1.tex"
            "${CMAKE_SOURCE_DIR}/assets/build/texture_2.tex"
            "${CMAKE_SOURCE_DIR}/assets/build/texture_3.tex"
            DESTINATION examples/rotating_cube/assets)
elseif(PLATFORM_PS1)
    install(DIRECTORY examples/sdk/ps1/ DESTINATION examples/hello_triangle_ps1)
elseif(PLATFORM_SATURN)
    install(DIRECTORY examples/sdk/saturn/ DESTINATION examples/hello_triangle_saturn)
endif()

install(DIRECTORY templates/project DESTINATION templates)
install(FILES
        "${CMAKE_SOURCE_DIR}/cmake/ps1-toolchain.cmake"
        "${CMAKE_SOURCE_DIR}/cmake/saturn-toolchain.cmake"
        "${CMAKE_SOURCE_DIR}/cmake/saturn-sdk-toolchain.cmake"
        DESTINATION cmake)

# SDK-архив: cmake --build <build-dir> --target sdk_package
if(PLATFORM_PC)
    set(RPE_SDK_PACKAGE_DEPS retro_engine retro_engine_hal retro_engine_log
        asset_converter hello_triangle rotating_cube)
elseif(PLATFORM_PS1)
    set(RPE_SDK_PACKAGE_DEPS retro_engine retro_engine_hal retro_engine_log
        retro_ps1 retro_disc)
elseif(PLATFORM_SATURN)
    set(RPE_SDK_PACKAGE_DEPS retro_engine retro_engine_hal retro_engine_log
        hello_triangle_saturn)
endif()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(sdk_package
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/package_sdk.py"
                --build-dir "${CMAKE_BINARY_DIR}"
                --platform "${RPE_PLATFORM_NAME}"
                --arch "${RPE_PLATFORM_ARCH}"
                --version "${PROJECT_VERSION}"
                --output-dir "${CMAKE_SOURCE_DIR}/dist"
        DEPENDS ${RPE_SDK_PACKAGE_DEPS}
        COMMENT "SDK: install + deterministic archive + manifest (S33–34)"
        VERBATIM)
endif()
