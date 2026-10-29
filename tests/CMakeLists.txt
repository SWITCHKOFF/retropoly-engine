# ============================================================================
# tests/ — юнит- и интеграционные тесты (GoogleTest canonical; FetchContent
# запрещён — §3.6). Нормативно: src/core/math/* (fixed/vec/mat)
# docs/plans/sprint_01_02.md; рендер-интеграционные (W0-golden) — Спринты 7–8.
#
# GoogleTest подключается ТОЛЬКО после вендоринга (submodule инициализирован)
# и изолирован от канонических флагов движка (third-party без -Werror, §3.5).
# ============================================================================

enable_testing()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/googletest/CMakeLists.txt")
    # Стандартные параметры подключения GoogleTest (recommendation upstream).
    set(INSTALL_GTEST OFF)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    add_subdirectory("${CMAKE_SOURCE_DIR}/src/third_party/googletest"
        "${CMAKE_BINARY_DIR}/third_party/googletest" EXCLUDE_FROM_ALL)

    add_executable(retro_engine_tests
        math/fixed_test.cpp
        math/vec2_test.cpp
        math/vec3_test.cpp
        math/vec4_test.cpp
        math/mat4_test.cpp
    )
    target_compile_features(retro_engine_tests PRIVATE cxx_std_14)
    target_link_libraries(retro_engine_tests PRIVATE retro_engine GTest::gtest_main)
    # Канонические флаги применяются только к коду тестов движка.
    retro_engine_apply_flags(retro_engine_tests)

    include(GoogleTest)
    gtest_discover_tests(retro_engine_tests DISCOVERY_TIMEOUT 60)
endif()

# Бенчмарки (пресет pc-bench, BUILD_BENCHMARKS; Спринт 1–2 Неделя 4 [M]):
# tests/benchmark/math_benchmark.cpp (google/benchmark). Подключаются, когда
# benchmark вендорен; сам vendoring — отдельная задача (pinned release).
if(BUILD_BENCHMARKS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/benchmark/CMakeLists.txt")
        add_subdirectory("${CMAKE_SOURCE_DIR}/src/third_party/benchmark"
            "${CMAKE_BINARY_DIR}/third_party/benchmark" EXCLUDE_FROM_ALL)
        add_executable(math_benchmark benchmark/math_benchmark.cpp)
        target_compile_features(math_benchmark PRIVATE cxx_std_14)
        target_link_libraries(math_benchmark PRIVATE retro_engine benchmark::benchmark)
        retro_engine_apply_flags(math_benchmark)
    else()
        message(STATUS "BUILD_BENCHMARKS: src/third_party/benchmark не вендорен — бенчмарки пропущены")
    endif()
endif()
