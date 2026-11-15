# ============================================================================
# hello_triangle_n64 — SDK example (S45–46).
#
# Сборка (из распакованного SDK, pinned libdragon toolchain):
#   export N64_INST=$HOME/n64-sdk/toolchain/opt/libdragon
#   cmake -S examples/hello_triangle_n64 -B build -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=<sdk>/cmake/n64-toolchain.cmake \
#         -DCMAKE_PREFIX_PATH=<sdk>
#   cmake --build build
# Артефакт: build/hello_triangle_n64.z64 (IPL3 + ELF, DFS не требуется).
# ============================================================================
cmake_minimum_required(VERSION 3.25)
project(hello_triangle_n64 LANGUAGES C CXX ASM)

find_package(RetroPolyEngine CONFIG REQUIRED)

if(NOT N64_INST AND DEFINED ENV{N64_INST})
    set(N64_INST "$ENV{N64_INST}")
endif()
if(NOT N64_INST)
    message(FATAL_ERROR
        "hello_triangle_n64: N64_INST не задан; используйте "
        "<sdk>/cmake/n64-toolchain.cmake и pinned libdragon toolchain.")
endif()

set(N64_TARGET "mips64-elf")
set(N64_BINDIR "${N64_INST}/bin")
set(N64_INCLUDEDIR "${N64_INST}/${N64_TARGET}/include")
set(N64_LIBDIR "${N64_INST}/${N64_TARGET}/lib")

if(NOT EXISTS "${N64_INCLUDEDIR}/libdragon.h" OR
   NOT EXISTS "${N64_LIBDIR}/libdragon.a")
    message(FATAL_ERROR
        "hello_triangle_n64: N64_INST не содержит libdragon "
        "(${N64_INCLUDEDIR}/libdragon.h). См. docs/N64.md §3.1.")
endif()

add_library(rpe_sdk_libdragon STATIC IMPORTED GLOBAL)
set_target_properties(rpe_sdk_libdragon PROPERTIES
    IMPORTED_LOCATION "${N64_LIBDIR}/libdragon.a"
    INTERFACE_INCLUDE_DIRECTORIES "${N64_INCLUDEDIR}")
add_library(rpe_sdk_libdragonsys STATIC IMPORTED GLOBAL)
set_target_properties(rpe_sdk_libdragonsys PROPERTIES
    IMPORTED_LOCATION "${N64_LIBDIR}/libdragonsys.a")

add_executable(hello_triangle_n64 main.cpp)
target_compile_features(hello_triangle_n64 PRIVATE cxx_std_14)
target_include_directories(hello_triangle_n64 PRIVATE "${N64_INCLUDEDIR}")
target_link_directories(hello_triangle_n64 PRIVATE "${N64_LIBDIR}")
target_link_libraries(hello_triangle_n64 PRIVATE
    "-Wl,--start-group"
    RetroPolyEngine::retro_engine
    RetroPolyEngine::hal
    RetroPolyEngine::log
    RetroPolyEngine::n64_rsp_tnl
    rpe_sdk_libdragon
    rpe_sdk_libdragonsys
    c
    m
    gcc
    "-Wl,--end-group")
target_link_options(hello_triangle_n64 PRIVATE
    "-Wl,-Tn64.ld"
    "-Wl,--gc-sections"
    "-Wl,--wrap=__do_global_ctors"
    "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/hello_triangle_n64.map")
set_target_properties(hello_triangle_n64 PROPERTIES
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_command(
    TARGET hello_triangle_n64 POST_BUILD
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E env "N64_INST=${N64_INST}"
            "${N64_BINDIR}/n64sym" "hello_triangle_n64.elf" "hello_triangle_n64.sym"
    COMMAND "${CMAKE_COMMAND}" -E copy "hello_triangle_n64.elf"
            "hello_triangle_n64.elf.stripped"
    COMMAND "${N64_BINDIR}/mips64-elf-strip" -s "hello_triangle_n64.elf.stripped"
    COMMAND "${CMAKE_COMMAND}" -E env "N64_INST=${N64_INST}"
            "${N64_BINDIR}/n64elfcompress" -o "." -c 1
            "hello_triangle_n64.elf.stripped"
    COMMAND "${N64_BINDIR}/n64tool"
            --title "RETRO SDK N64"
            --category N
            --region E
            --toc
            --output "hello_triangle_n64.z64"
            --align 256 "hello_triangle_n64.elf.stripped"
            --align 8 "hello_triangle_n64.sym"
            --align 8
    COMMENT "n64: hello_triangle_n64.z64 (SDK consumer ROM)")
