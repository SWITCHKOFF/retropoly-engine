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
# Публичный SDK-интерфейс (ADR-011): <retro_engine/...> + source-style
# "core/..." для внутренних .cpp; internal-заголовки остаются в src/.
target_include_directories(retro_engine_log PUBLIC
    "$<BUILD_INTERFACE:${RPE_PUBLIC_INCLUDE_DIR}>"
    "$<BUILD_INTERFACE:${RPE_PUBLIC_API_INCLUDE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
    "$<INSTALL_INTERFACE:${RPE_INSTALL_INCLUDEDIR}>"
    "$<INSTALL_INTERFACE:${RPE_INSTALL_INCLUDEDIR}/retro_engine>")
target_include_directories(retro_engine_log PRIVATE "${CMAKE_SOURCE_DIR}/src")
# Compile-time специализация формата файлов (log.cpp: FILE-sink на PC,
# no-op на PS1 — см. заголовок log.h). Значение задаётся выбором бэкенда
# без создания platform-ifdef'ов в заголовках ядра.
if(PLATFORM_PC)
    target_compile_definitions(retro_engine_log PRIVATE PLATFORM_PC=1)
elseif(PLATFORM_SATURN)
    target_compile_definitions(retro_engine_log PRIVATE PLATFORM_SATURN=1)
endif()
retro_engine_apply_flags(retro_engine_log)

if(PLATFORM_PC)
    add_subdirectory(hal/pc)
elseif(PLATFORM_PS1)
    add_subdirectory(hal/ps1)
elseif(PLATFORM_SATURN)
    add_subdirectory(hal/saturn)
endif()

include(EmbedShaders) # cmake/EmbedShaders.cmake (шейдеры → generated/headers).

add_library(retro_engine STATIC)

# Математическое ядро (Спринт 1–2): fixed_t 16.16. Дальше — правила SRC LAYOUT.
# ECS (Спринт 3–4): Registry; аллокаторы — header-only (core/memory).
# Рендер-ядро (Спринт 7–8): pipeline + пул текстур; backend — ровно один HAL.
# Ассеты (Спринт 9–10): безопасные загрузчики .mesh/.tex v1 (core/assets).
target_sources(retro_engine PRIVATE
    core/math/fixed.cpp
    core/math/math.cpp
    core/ecs/registry.cpp
    core/memory/memory_profiler.cpp
    render/texture.cpp
    render/renderer.cpp
    assets/mesh_loader.cpp
    assets/texture_loader.cpp
)

# Compile-time специализация платформы (renderer.cpp: readback/hash — PC-only,
# Спринт 15–16 перенесёт на PS1-бэкенд; log.cpp — тот же подход).
if(PLATFORM_PC)
    target_compile_definitions(retro_engine PRIVATE PLATFORM_PC=1)
elseif(PLATFORM_SATURN)
    target_compile_definitions(retro_engine PRIVATE PLATFORM_SATURN=1)
endif()

# Публичные заголовки SDK: <retro_engine/...> из include/ + source-style
# "core/..." из include/retro_engine (тот же public-набор, ADR-011).
target_include_directories(retro_engine PUBLIC
    "$<BUILD_INTERFACE:${RPE_PUBLIC_INCLUDE_DIR}>"
    "$<BUILD_INTERFACE:${RPE_PUBLIC_API_INCLUDE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/include>"
    "$<INSTALL_INTERFACE:${RPE_INSTALL_INCLUDEDIR}>"
    "$<INSTALL_INTERFACE:${RPE_INSTALL_INCLUDEDIR}/retro_engine>")
# Внутренние заголовки (render/*, hal/*/*) и generated — только build-tree.
target_include_directories(retro_engine PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${RPE_GENERATED_INCLUDE_DIR}")

target_link_libraries(retro_engine PUBLIC retro_engine_hal retro_engine_log)
retro_engine_apply_flags(retro_engine)

# --- PS1: NOGPREL-кодогенерация статических библиотек (SDK-паттерн) ---------
# PsN00bSDK компилирует библиотечные объекты БЕЗ gp-относительного доступа
# (-G0 -mno-gpopt; psn00bsdk INTERFACE генекс по свойству PSN00BSDK_TARGET_
# TYPE); PS-EXE retro_ps1 собирается GPREL (-G8) — легко живёт с NOGPREL
# либами (стандартный SDK-вой пример-роль). Свойство ставится ПОСЛЕ создания
# всех трёх библиотек-слоёв.
if(PLATFORM_PS1)
    set_target_properties(retro_engine_log retro_engine_hal retro_engine
        PROPERTIES PSN00BSDK_TARGET_TYPE EXECUTABLE_NOGPREL)
endif()

# --- SDK export: RetroPolyEngine::{retro_engine,hal,log} (S33–34) ----------
set_target_properties(retro_engine_hal PROPERTIES
    EXPORT_NAME hal
    EXPORT_NO_SYSTEM ON)
set_target_properties(retro_engine_log PROPERTIES
    EXPORT_NAME log
    EXPORT_NO_SYSTEM ON)
# Импортированный SDK-таргет не должен превращать public headers в -isystem:
# GCC 8.2 sh-elf (Saturn) ломает C++ linkage на системных заголовках.
set_target_properties(retro_engine PROPERTIES EXPORT_NO_SYSTEM ON)
if(TARGET glad)
    set_target_properties(glad PROPERTIES EXPORT_NO_SYSTEM ON)
endif()
add_library(RetroPolyEngine::retro_engine ALIAS retro_engine)
add_library(RetroPolyEngine::hal ALIAS retro_engine_hal)
add_library(RetroPolyEngine::log ALIAS retro_engine_log)

install(TARGETS retro_engine retro_engine_hal retro_engine_log
        EXPORT RetroPolyEngineTargets
        ARCHIVE DESTINATION "${RPE_SDK_LIBDIR}"
        LIBRARY DESTINATION "${RPE_SDK_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
