# ============================================================================
# src/ — ядро движка retro_engine (Спринт 0, каркас)
#
# Нормативно (docs/RetroEngine_...v1.43: "BUILD PRECEDENCE", sprint_00 §3.5):
# каждый билд содержит ровно один HAL; src/CMakeLists.txt выбирает ровно один
# retro_engine_hal до создания retro_engine. Наполнение ядра — Спринты 1–4
# (core/math, core/memory, core/ecs, core/log), далее render/io/audio/assets.
# ============================================================================

if(PLATFORM_PC)
    add_subdirectory(hal/pc)
elseif(PLATFORM_PS1)
    add_subdirectory(hal/ps1)
endif()

add_library(retro_engine STATIC)

# Математическое ядро (Спринт 1–2): fixed_t 16.16. Дальше — правила SRC LAYOUT.
# ECS (Спринт 3–4): Registry; аллокаторы — header-only (core/memory).
# Логгер (Спринт 7–8, долг Спринтов 3–4): диагностика только через core/log.
target_sources(retro_engine PRIVATE
    core/math/fixed.cpp
    core/math/math.cpp
    core/ecs/registry.cpp
    core/log/log.cpp
)

# Публичные заголовки ядра включаются как "core/math/fixed.h" (корень include — src/).
target_include_directories(retro_engine PUBLIC "${CMAKE_SOURCE_DIR}/src")

# Compile-time специализация логгера: file-sink на PC, no-op на PS1 — без
# platform-ifdef'ов в заголовках ядра (значение задаётся выбором бэкенда).
if(PLATFORM_PC)
    target_compile_definitions(retro_engine PRIVATE PLATFORM_PC=1)
endif()

target_link_libraries(retro_engine PUBLIC retro_engine_hal)
retro_engine_apply_flags(retro_engine)
