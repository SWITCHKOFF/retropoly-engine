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
