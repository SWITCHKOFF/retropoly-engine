# ============================================================================
# Вендоренный glad (OpenGL Core 3.3, loader, extensions=all; §3.3).
# Third-party: канонические флаги движка (-Werror и т.п.) НЕ применяются,
# заголовки экспортируются как SYSTEM.
# Хэш/версия генерации: VERSION.txt → [glad].
# ============================================================================

add_library(glad STATIC
    src/gl.c
)
target_include_directories(glad SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# glad поставляется внутри PC SDK: retro_engine_hal линкует его PUBLIC.
install(TARGETS glad EXPORT RetroPolyEngineTargets
        ARCHIVE DESTINATION "${RPE_SDK_LIBDIR}")
install(DIRECTORY include/glad include/KHR
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
