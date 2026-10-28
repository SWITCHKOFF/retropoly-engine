# ============================================================================
# HAL PC — SDL3 (окно/ввод/аудио/ФС) + OpenGL 3.3 через glad.
# Реализация: Спринты 5–6 (HAL PC), 7–8 (рендерер; docs/plans/sprint_05_06.md).
# ============================================================================

add_library(retro_engine_hal STATIC
    platform_pc.cpp
)
retro_engine_apply_flags(retro_engine_hal)

# glad (OpenGL-загрузчик, §3.3) подключается, когда вендорен; third-party
# заголовки изолируются от канонических флагов движка.
if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/glad/src/gl.c")
    add_subdirectory("${CMAKE_SOURCE_DIR}/src/third_party/glad" glad EXCLUDE_FROM_ALL)
    target_link_libraries(retro_engine_hal PRIVATE glad)
endif()
