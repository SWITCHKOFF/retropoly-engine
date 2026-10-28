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
