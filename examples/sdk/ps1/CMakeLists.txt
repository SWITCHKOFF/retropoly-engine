# ============================================================================
# hello_triangle_ps1 — SDK example (S33–34).
#
# Сборка (из распакованного SDK):
#   export PSN00BSDK_LIBS=$HOME/PSn00bSDK/lib/libpsn00b
#   cmake -S examples/hello_triangle_ps1 -B build -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=<sdk>/cmake/ps1-toolchain.cmake \
#         -DCMAKE_PREFIX_PATH=<sdk>
#   cmake --build build
# ============================================================================
cmake_minimum_required(VERSION 3.25)
project(hello_triangle_ps1 LANGUAGES CXX)

find_package(RetroPolyEngine CONFIG REQUIRED)

if(NOT COMMAND psn00bsdk_add_executable)
    message(FATAL_ERROR
        "hello_triangle_ps1: используйте toolchain <sdk>/cmake/ps1-toolchain.cmake")
endif()

psn00bsdk_add_executable(hello_triangle_ps1 GPREL main.cpp)
target_compile_features(hello_triangle_ps1 PRIVATE cxx_std_14)

# Однопроходный mipsel-ld: SDK-архивы добавляются helper'ом ДО движковых,
# поэтому ссылки retro_engine_hal → libpsx* разрешаются только повторным
# сканированием. Группа engine+SDK — тот же приём, что в самом движке.
target_link_libraries(hello_triangle_ps1 PRIVATE RetroPolyEngine::retro_engine)
target_link_libraries(hello_triangle_ps1 PRIVATE
    "-Wl,--start-group"
    $<TARGET_FILE:RetroPolyEngine::retro_engine>
    $<TARGET_FILE:RetroPolyEngine::hal>
    $<TARGET_FILE:RetroPolyEngine::log>
    $<TARGET_FILE:psxgpu_exe_gprel>
    $<TARGET_FILE:psxgte_exe_gprel>
    $<TARGET_FILE:psxspu_exe_gprel>
    $<TARGET_FILE:psxcd_exe_gprel>
    $<TARGET_FILE:psxpress_exe_gprel>
    $<TARGET_FILE:psxsio_exe_gprel>
    $<TARGET_FILE:psxetc_exe_gprel>
    $<TARGET_FILE:psxapi_exe_gprel>
    $<TARGET_FILE:smd_exe_gprel>
    $<TARGET_FILE:lzp_exe_gprel>
    $<TARGET_FILE:c_exe_gprel>
    "-Wl,--end-group")
