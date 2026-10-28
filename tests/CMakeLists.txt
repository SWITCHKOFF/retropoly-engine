# ============================================================================
# tests/ — юнит- и интеграционные тесты (GoogleTest canonical; FetchContent
# запрещён — §3.6). Спринт 0: только каркас; первые тесты — Спринты 1–2
# (tests/math/*: fixed, vec, mat; docs/plans/sprint_01_02.md).
# Рендер-интеграционные тесты (W0-golden) — Спринты 7–8.
# ============================================================================

enable_testing()

# GoogleTest подключается только после вендоринга (submodule инициализирован).
if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/googletest/CMakeLists.txt")
    set(INSTALL_GTEST OFF)
    add_subdirectory("${CMAKE_SOURCE_DIR}/src/third_party/googletest"
        "${CMAKE_BINARY_DIR}/third_party/googletest")
endif()
