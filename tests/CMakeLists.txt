# ============================================================================
# tests/ — юнит- и интеграционные тесты (GoogleTest canonical; FetchContent
# запрещён — §3.6). Нормативно: src/core/math/* (fixed/vec/mat),
# src/core/memory/* (linear/pool/stack), src/core/ecs/* (entity/registry),
# src/hal/* (platform/input/audio/filesystem — sprint_05_06, недели 10–12);
# рендер-интеграционные (W0-golden) — Спринты 7–8.
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
        ecs/collision_test.cpp
        ecs/lighting_test.cpp
        log/log_test.cpp
        render/texture_test.cpp
        render/command_buffer_test.cpp
        render/contract_test.cpp
        integration/hal_test.cpp
        integration/mesh_loader_test.cpp
        integration/texture_loader_test.cpp
    )
    target_compile_features(retro_engine_tests PRIVATE cxx_std_14)
    target_link_libraries(retro_engine_tests PRIVATE retro_engine GTest::gtest_main)
    # Канонические флаги применяются только к коду тестов движка.
    retro_engine_apply_flags(retro_engine_tests)
    # game/* и examples/* — include от КОРНЯ репозитория (mirrors примеры).
    target_include_directories(retro_engine_tests PRIVATE "${CMAKE_SOURCE_DIR}")
    # Путь к пайплайн-артефактам (assets/build/*) — compile-time, CWD-независимо.
    # PLATFORM_PC — специализация платформы для тестов HAL (tests/ добавляются
    # только при PLATFORM_PC; mirrors retro_engine / retro_engine_log).
    target_compile_definitions(retro_engine_tests PRIVATE
        RPE_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        PLATFORM_PC=1)

    include(GoogleTest)
    gtest_discover_tests(retro_engine_tests DISCOVERY_TIMEOUT 60)

    # -----------------------------------------------------------------------
    # Saturn HAL host-tests (S23–26). Собираются host-компилятором в PC-
    # пресете с PLATFORM_SATURN=1, но БЕЗ SH-2/Jo Engine: C-мост заменён
    # fake_saturn_bridge.cpp, а HAL-модули (memory/input/FS/command/render/
    # scu_sync/slave_worker/parallel_workload) линкуются напрямую.
    # retro_engine не линкуется, чтобы не дублировать platform-специфичные
    # HAL-символы.
    # -----------------------------------------------------------------------
    add_executable(saturn_hal_tests
        saturn/fake_saturn_bridge.cpp
        saturn/saturn_command_buffer_test.cpp
        saturn/saturn_fs_path_test.cpp
        saturn/saturn_fs_test.cpp
        saturn/saturn_input_test.cpp
        saturn/saturn_log_test.cpp
        saturn/saturn_memory_test.cpp
        saturn/saturn_parallel_workload_test.cpp
        saturn/saturn_render_test.cpp
        saturn/saturn_scu_sync_test.cpp
        saturn/saturn_slave_worker_test.cpp
        saturn/triangle_to_quad_test.cpp
        saturn/vdp1_command_list_test.cpp
        saturn/vdp1_texture_test.cpp
        saturn/vdp1_vram_test.cpp
        saturn/vdp2_backend_test.cpp
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/platform_saturn.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/filesystem_saturn.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/render_saturn.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/saturn_command_buffer.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/saturn_fs.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/saturn_fs_path.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/saturn_input.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/saturn_memory.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/saturn_parallel_workload.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/saturn_render_utils.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/scu_sync.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/slave_worker.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/triangle_to_quad.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/vdp1_backend.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/vdp2_backend.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/vdp1_command_list.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/vdp1_texture.cpp"
        "${CMAKE_SOURCE_DIR}/src/hal/saturn/vdp1_vram.cpp"
        "${CMAKE_SOURCE_DIR}/src/core/log/log.cpp"
        "${CMAKE_SOURCE_DIR}/src/core/math/fixed.cpp"
        "${CMAKE_SOURCE_DIR}/src/core/math/math.cpp"
        "${CMAKE_SOURCE_DIR}/src/core/memory/memory_profiler.cpp"
        "${CMAKE_SOURCE_DIR}/src/render/renderer.cpp"
        "${CMAKE_SOURCE_DIR}/src/render/texture.cpp")
    target_compile_features(saturn_hal_tests PRIVATE cxx_std_14)
    target_include_directories(saturn_hal_tests PRIVATE
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(saturn_hal_tests PRIVATE GTest::gtest_main)
    target_compile_definitions(saturn_hal_tests PRIVATE
        PLATFORM_SATURN=1
        RPE_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    retro_engine_apply_flags(saturn_hal_tests)
    gtest_discover_tests(saturn_hal_tests DISCOVERY_TIMEOUT 60)

    # Рендер-интеграционные (W0-golden, неделя 16) — пресет pc-debug-render
    # (BUILD_RENDER_INTEGRATION_TESTS=ON). Требуют дисплей/GL: headless-CI
    # пропускает кейсы через GTEST_SKIP (см. render_golden_test.cpp).
    if(BUILD_RENDER_INTEGRATION_TESTS)
        add_executable(retro_engine_visual_tests
            visual/render_golden_test.cpp
            visual/golden_protocol.cpp
            visual/hello_triangle_golden_test.cpp
            visual/rotating_cube_golden_test.cpp
            visual/overlay_golden_test.cpp
            visual/w0_scene.cpp
        )
        target_compile_features(retro_engine_visual_tests PRIVATE cxx_std_14)
        target_include_directories(retro_engine_visual_tests PRIVATE
            "${CMAKE_SOURCE_DIR}")
        target_link_libraries(retro_engine_visual_tests PRIVATE
            retro_engine GTest::gtest_main)
        retro_engine_apply_flags(retro_engine_visual_tests)
        # Путь эталонов (golden reference) — compile-time, getenv-fallback.
        # HT_ASSET_DIR — пути ассетов примера (scene.h), mirrors main.cpp.
        target_compile_definitions(retro_engine_visual_tests PRIVATE
            RPE_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
            HT_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/build"
            RC_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/build")
        gtest_discover_tests(retro_engine_visual_tests DISCOVERY_TIMEOUT 60)
    endif()
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
