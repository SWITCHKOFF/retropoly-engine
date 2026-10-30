# ============================================================================
# HAL PC — SDL3 (окно/ввод/аудио/ФС) + OpenGL 3.3 через glad.
# Реализация: Спринты 5–6 (docs/plans/sprint_05_06.md); рендерер — Спринты 7–8.
#
# Контракт сборки (неделя 9):
#   - SDL3 подключается ТОЛЬКО как вендоренный target SDL3::SDL3
#     (add_subdirectory в корневом CMakeLists, submodule §3.6);
#   - OpenGL::GL — системная цель (find_package);
#   - glad — вендоренный static (§3.3), third-party изолирован от
#     канонических флагов движка (-Werror применяется только к HAL).
# ============================================================================

if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR
        "HAL PC (Спринт 5–6) требует SDL3::SDL3: submodule не инициализирован/"
        "не собран. Запустите `git submodule update --init --recursive`.")
endif()

find_package(OpenGL REQUIRED)

# glad (OpenGL-загрузчик, §3.3) — вендоренный, подключается как static.
if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/glad/src/gl.c")
    add_subdirectory("${CMAKE_SOURCE_DIR}/src/third_party/glad" glad
        EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR
        "HAL PC (Спринт 5–6) требует вендоренный glad (src/third_party/glad).")
endif()

add_library(retro_engine_hal STATIC
    platform_pc.cpp
    render_opengl.cpp
    input_sdl.cpp
    audio_sdl.cpp
    filesystem_pc.cpp
)
retro_engine_apply_flags(retro_engine_hal)

# Заголовки движка включаются от корня src/ ("hal/platform.h" и т.п.;
# PUBLIC-путь к ядру выдаётся от retro_engine в src/CMakeLists.txt).
target_include_directories(retro_engine_hal PRIVATE "${CMAKE_SOURCE_DIR}/src")

# Публичные зависимости propagate: examples/tests ссылаются только на
# retro_engine (docs/DEVELOPMENT_PLAN.md §2.1), SDL3/GL/glad не разбрасываются.
target_link_libraries(retro_engine_hal PUBLIC SDL3::SDL3 OpenGL::GL glad)
