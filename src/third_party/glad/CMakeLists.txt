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
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
