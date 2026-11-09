# ============================================================================
# rotating_cube — демо MVP (веха M7; Спринты 17–18,
# docs/plans/sprint_17_18.md). Связан ТОЛЬКО с retro_engine: сцена кубов,
# AABB-коллизии, Gouraud, debug overlay. Release reference target —
# build/<preset>/game/rotating_cube(.exe для Windows).
# ============================================================================

add_executable(rotating_cube
    main.cpp
)
target_link_libraries(rotating_cube PRIVATE retro_engine)
retro_engine_apply_flags(rotating_cube)
# Слой игры/пример — include от КОРНЯ репозитория (game/*, examples/*).
target_include_directories(rotating_cube PRIVATE "${CMAKE_SOURCE_DIR}")
# Специализация платформы (main.cpp: PC-реализация или PS1-заглушка;
# mirrors retro_engine — spec задаётся выбором бэкенда, не ifdef'ами ядра).
if(PLATFORM_PC)
    target_compile_definitions(rotating_cube PRIVATE PLATFORM_PC=1)
endif()
# Пути ассетов — compile-time (прогон бинаря независимо от CWD;
# mirrors HT_ASSET_DIR в hello_triangle).
target_compile_definitions(rotating_cube PRIVATE
    RC_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/build")
