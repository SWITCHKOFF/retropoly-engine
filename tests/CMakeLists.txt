# ============================================================================
# tests/ — юнит- и интеграционные тесты (GoogleTest canonical; FetchContent
# запрещён — §3.6). Нормативно: src/core/math/* (fixed/vec/mat),
# src/core/memory/* (linear/pool/stack), src/core/ecs/* (entity/registry) —
# docs/plans/sprint_01_02.md и sprint_03_04.md; рендер-интеграционные
# (W0-golden) — Спринты 7–8.
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
        memory/linear_allocator_test.cpp
        memory/pool_allocator_test.cpp
        memory/stack_allocator_test.cpp
        memory/allocator_integration_test.cpp
        ecs/entity_test.cpp
        ecs/registry_test.cpp
    )
    target_compile_features(retro_engine_tests PRIVATE cxx_std_14)
    target_link_libraries(retro_engine_tests PRIVATE retro_engine GTest::gtest_main)
    # Канонические флаги применяются только к коду тестов движка.
    retro_engine_apply_flags(retro_engine_tests)

    include(GoogleTest)
    gtest_discover_tests(retro_engine_tests DISCOVERY_TIMEOUT 60)
endif()

# Бенчмарки (пресет pc-bench, BUILD_BENCHMARKS):
# - math_benchmark — google/benchmark (подключается после вендоринга);
# - memory_benchmark — самодостаточный baseline латентности аллокаторов
#   (sprint_03_04, [H]); не требует внешней зависимости.
if(BUILD_BENCHMARKS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/benchmark/CMakeLists.txt")
        add_subdirectory("${CMAKE_SOURCE_DIR}/src/third_party/benchmark"
            "${CMAKE_BINARY_DIR}/third_party/benchmark" EXCLUDE_FROM_ALL)
        add_executable(math_benchmark benchmark/math_benchmark.cpp)
        target_compile_features(math_benchmark PRIVATE cxx_std_14)
        target_link_libraries(math_benchmark PRIVATE retro_engine benchmark::benchmark)
        retro_engine_apply_flags(math_benchmark)
    else()
        message(STATUS "BUILD_BENCHMARKS: benchmark не вендорен — math_benchmark пропущен")
    endif()

    add_executable(memory_benchmark benchmark/memory_benchmark.cpp)
    target_compile_features(memory_benchmark PRIVATE cxx_std_14)
    target_link_libraries(memory_benchmark PRIVATE retro_engine)
    retro_engine_apply_flags(memory_benchmark)
endif()
