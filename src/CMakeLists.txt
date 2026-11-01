# ============================================================================
# src/ — ядро движка retro_engine (Спринт 0, каркас)
#
# Нормативно (docs/RetroEngine_...v1.43: "BUILD PRECEDENCE", sprint_00 §3.5):
# каждый билд содержит ровно один HAL; src/CMakeLists.txt выбирает ровно один
# retro_engine_hal до создания retro_engine. Наполнение ядра — Спринты 1–4
# (core/math, core/memory, core/ecs, core/log), далее render/io/audio/assets.
#
# Слоёная топология статических библиотек (линия линковки без циклов):
#   retro_engine_log — core/log (диагностика; PRIVATE-зависимость HAL);
#   retro_engine_hal — ровно один HAL (PUBLIC SDL3/GL/glad);
#   retro_engine     — ядро (math/ecs/render; PUBLIC hal + log).
# ============================================================================

# Логгер — независимый слой (render_opengl.cpp пишет через core/log;
# ядро рендера — тоже: единая точка диагностики STyle guide §3).
add_library(retro_engine_log STATIC core/log/log.cpp)
target_include_directories(retro_engine_log PUBLIC "${CMAKE_SOURCE_DIR}/src")
# Compile-time специализация формата файлов (log.cpp: FILE-sink на PC,
# no-op на PS1 — см. заголовок log.h). Значение задаётся выбором бэкенда
# без создания platform-ifdef'ов в заголовках ядра.
if(PLATFORM_PC)
    target_compile_definitions(retro_engine_log PRIVATE PLATFORM_PC=1)
endif()
retro_engine_apply_flags(retro_engine_log)

if(PLATFORM_PC)
    add_subdirectory(hal/pc)
elseif(PLATFORM_PS1)
    add_subdirectory(hal/ps1)
endif()

include(EmbedShaders) # cmake/EmbedShaders.cmake (шейдеры → generated/headers).

add_library(retro_engine STATIC)

# Математическое ядро (Спринт 1–2): fixed_t 16.16. Дальше — правила SRC LAYOUT.
# ECS (Спринт 3–4): Registry; аллокаторы — header-only (core/memory).
# Рендер-ядро (Спринт 7–8): pipeline + пул текстур; backend — ровно один HAL.
target_sources(retro_engine PRIVATE
    core/math/fixed.cpp
    core/math/math.cpp
    core/ecs/registry.cpp
    render/texture.cpp
    render/renderer.cpp
)

# Compile-time специализация платформы (renderer.cpp: readback/hash — PC-only,
# Спринт 15–16 перенесёт на PS1-бэкенд; log.cpp — тот же подход).
if(PLATFORM_PC)
    target_compile_definitions(retro_engine PRIVATE PLATFORM_PC=1)
endif()

# Публичные заголовки ядра включаются как "core/math/fixed.h" (корень include — src/).
target_include_directories(retro_engine PUBLIC "${CMAKE_SOURCE_DIR}/src")

# Сгенерированный каталог (shaders_embedded.h) — приватные детали ядра и HAL.
target_include_directories(retro_engine PRIVATE "${RPE_GENERATED_INCLUDE_DIR}")

target_link_libraries(retro_engine PUBLIC retro_engine_hal retro_engine_log)
retro_engine_apply_flags(retro_engine)
