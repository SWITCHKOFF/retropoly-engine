# ============================================================================
# hello_triangle_saturn — SDK example (S33–34).
#
# Сборка (из распакованного SDK):
#   export JO_ENGINE_ROOT=$HOME/joengine
#   cmake -S examples/hello_triangle_saturn -B build -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=<sdk>/cmake/saturn-sdk-toolchain.cmake \
#         -DCMAKE_PREFIX_PATH=<sdk>
#   cmake --build build
# ============================================================================
cmake_minimum_required(VERSION 3.25)
project(hello_triangle_saturn LANGUAGES C CXX ASM)

find_package(RetroPolyEngine CONFIG REQUIRED)

add_executable(hello_triangle_saturn main.cpp jo_bridge.c)
target_compile_features(hello_triangle_saturn PRIVATE cxx_std_14)
target_include_directories(hello_triangle_saturn PRIVATE "${JO_ENGINE_INCLUDE_DIR}")
# GCC 8.2 sh-elf + public C++ headers: -isystem для SDK-заголовков ломает
# linkage (template with C linkage). SDK include — обычные -I.
set_target_properties(hello_triangle_saturn PROPERTIES
    NO_SYSTEM_FROM_IMPORTED TRUE)

# Jo Engine start.S/IP-заголовки должны попасть в ELF целиком; движковые
# архивы — в --start-group (однопроходный ld).
target_link_libraries(hello_triangle_saturn PRIVATE RetroPolyEngine::retro_engine)
target_link_libraries(hello_triangle_saturn PRIVATE
    "-Wl,--whole-archive" "$<TARGET_FILE:RetroPolyEngine::jo_engine>" "-Wl,--no-whole-archive"
    "-Wl,--start-group"
    "$<TARGET_FILE:RetroPolyEngine::retro_engine>"
    "$<TARGET_FILE:RetroPolyEngine::hal>"
    "$<TARGET_FILE:RetroPolyEngine::log>"
    "-Wl,--end-group"
    "-Wl,--start-group" "-lc" "-lnosys" "-lgcc" "-Wl,--end-group")
target_link_directories(hello_triangle_saturn PRIVATE
    "${JO_ENGINE_TARGET_LIB_DIR}"
    "${JO_ENGINE_LIBGCC_DIR}")
target_link_options(hello_triangle_saturn PRIVATE
    "LINKER:-T,${JO_ENGINE_LINKER_SCRIPT}"
    "LINKER:-Map,${CMAKE_CURRENT_BINARY_DIR}/hello_triangle_saturn.map"
    "LINKER:-e,___Start"
    "LINKER:--relax")
set_target_properties(hello_triangle_saturn PROPERTIES SUFFIX ".elf")
