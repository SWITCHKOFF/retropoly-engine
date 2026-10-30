# ============================================================================
# RetroPolyEngine — корневой CMakeLists.txt
#
# Нормативные источники:
#   - docs/plans/sprint_00.md §3.5 (Система сборки, каркас)
#   - docs/RetroEngine_Engineering_Documentation_v1.43.md: BUILD PRECEDENCE —
#     «CMakePresets.json is the only canonical build entry point»
#
# Правила:
#   - ровно один бэкенд (PLATFORM_PC XOR PLATFORM_PS1);
#   - hello_triangle / rotating_cube ссылаются только на retro_engine
#     (никогда напрямую на SDL3/OpenGL/PSn00bSDK);
#   - asset_converter — host-only, не собирается компилятором PS1;
#   - все сборки через канонические пресеты CMakePresets.json.
# ============================================================================

cmake_minimum_required(VERSION 3.25)

project(retro_engine
    VERSION 0.1.0
    DESCRIPTION "RetroPolyEngine — deterministic retro engine (PC: SDL3 + OpenGL 3.3; PS1: PSn00bSDK v0.24)"
    LANGUAGES C CXX ASM
)

# --- Опции сборки ------------------------------------------------------------
option(PLATFORM_PC   "Собирать целевой бэкенд PC (SDL3 + OpenGL 3.3)"        OFF)
option(PLATFORM_PS1  "Собирать целевой бэкенд PS1 (PSn00bSDK v0.24, pinned)" OFF)
option(BUILD_TESTS                       "Собирать юнит-тесты (GoogleTest)"               ON)
option(BUILD_EXAMPLES                    "Собирать примеры hello_triangle/rotating_cube"  OFF)
option(BUILD_RENDER_INTEGRATION_TESTS    "Рендер-интеграционные тесты (W0-golden, §3.5)"  OFF)
option(BUILD_BENCHMARKS                  "Собирать benchmarks (пресет pc-bench)"          OFF)
option(ENABLE_ASAN    "AddressSanitizer для целей движка (только PC)"         OFF)
option(ENABLE_COVERAGE "Покрытие кода тестами (только PC, пресет pc-coverage)" OFF)

# --- Guard: ровно один бэкенд -------------------------------------------------
if(PLATFORM_PC AND PLATFORM_PS1)
    message(FATAL_ERROR
        "Конфликт конфигурации: PLATFORM_PC и PLATFORM_PS1 взаимоисключающие (ровно один HAL на сборку).")
endif()
if(NOT PLATFORM_PC AND NOT PLATFORM_PS1)
    message(FATAL_ERROR
        "Не выбран бэкенд: включите PLATFORM_PC=ON или PLATFORM_PS1=ON (см. CMakePresets.json).")
endif()

# --- Канонические флаги компиляции (cmake/CompilerFlags.cmake) ----------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerFlags)

# --- Вендоренные зависимости (§3.6) -------------------------------------------
# SDL3 подключается ДО ядра: HAL (src/hal/pc) линкует target SDL3::SDL3.
if(PLATFORM_PC)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/sdl3/CMakeLists.txt")
        add_subdirectory(src/third_party/sdl3 EXCLUDE_FROM_ALL)
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
endif()

# --- Примеры: зависят только от retro_engine (ни от SDL3/OpenGL/PSn00bSDK) ----
add_subdirectory(examples/hello_triangle)
add_subdirectory(examples/rotating_cube)

if(PLATFORM_PC)
    # Каноническое расположение рантайм-бинарей примеров: build/<preset>/game/
    set_target_properties(hello_triangle rotating_cube PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/game")

    # CTest агрегирует тесты рекурсивно (ctest --preset pc-debug из build/<preset>).
    enable_testing()
    add_subdirectory(tests)
endif()
