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
target_link_libraries(retro_engine PUBLIC retro_engine_hal)
retro_engine_apply_flags(retro_engine)
