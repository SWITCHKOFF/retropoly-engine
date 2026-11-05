# ============================================================================
# hello_triangle — первый целевой пример (веха M4; Спринты 11–12,
# docs/plans/sprint_11_12.md). Связан ТОЛЬКО с retro_engine — прямые
# ссылки на SDL3/OpenGL/PSn00bSDK запрещены (docs/DEVELOPMENT_PLAN.md §2.1).
# ============================================================================

add_executable(hello_triangle
    main.cpp
)
target_link_libraries(hello_triangle PRIVATE retro_engine)
retro_engine_apply_flags(hello_triangle)
# Слой игры/пример — include от КОРНЯ репозитория (game/*, examples/*).
target_include_directories(hello_triangle PRIVATE "${CMAKE_SOURCE_DIR}")
# Специализация платформы (main.cpp: PC-реализация или PS1-заглушка;
# mirrors retro_engine — spec задаётся выбором бэкенда, не ifdef'ами ядра).
if(PLATFORM_PC)
    target_compile_definitions(hello_triangle PRIVATE PLATFORM_PC=1)
endif()
# Пути ассетов — compile-time (прогон бинаря независимо от CWD;
# mirrors RPE_TEST_SOURCE_DIR в tests/).
target_compile_definitions(hello_triangle PRIVATE
    HT_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/build")
