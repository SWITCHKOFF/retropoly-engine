# ============================================================================
# HAL Nintendo 64 — libdragon (pinned commit) + mips64-elf-gcc 16.2.0.
# Реализация: Спринты 39–42 (docs/plans/sprint_39_40.md,
# sprint_41_42.md), ADR-014.
#
# Топология:
#   rpe_n64_libdragon   — imported static libdragon.a (SYSTEM headers);
#   rpe_n64_libdragonsys— imported static libdragonsys.a;
#   retro_engine_hal    — N64 HAL: lifecycle/input/FS/память/render skeleton;
#   retro_n64           — bootable ELF (VI 320×240 + RDPQ + DFS);
#   retro_n64.dfs       — read-only DragonFS (mkdfs, deterministic);
#   hello_n64.z64       — ROM (n64tool + TOC + ELF + DFS).
#
# Сборка — только через пресет n64-release:
#   export N64_INST=/path/to/libdragon-toolchain
#   cmake --preset n64-release && cmake --build --preset n64-release
# ============================================================================

if(NOT DEFINED N64_INST OR NOT DEFINED N64_INCLUDEDIR OR NOT DEFINED N64_LIBDIR)
    message(FATAL_ERROR
        "HAL N64 требует toolchain cmake/n64-toolchain.cmake "
        "(N64_INST/N64_INCLUDEDIR/N64_LIBDIR). Сборка — только через "
        "пресет n64-release (docs/BUILDING.md § Nintendo 64).")
endif()

# --- Imported libdragon/newlib --------------------------------------------
if(NOT TARGET rpe_n64_libdragon)
    add_library(rpe_n64_libdragon STATIC IMPORTED GLOBAL)
    set_target_properties(rpe_n64_libdragon PROPERTIES
        IMPORTED_LOCATION "${N64_LIBDIR}/libdragon.a"
        INTERFACE_INCLUDE_DIRECTORIES "${N64_INCLUDEDIR}")

    add_library(rpe_n64_libdragonsys STATIC IMPORTED GLOBAL)
    set_target_properties(rpe_n64_libdragonsys PROPERTIES
        IMPORTED_LOCATION "${N64_LIBDIR}/libdragonsys.a")
endif()

# --- HAL library -----------------------------------------------------------
add_library(retro_engine_hal STATIC
    platform_n64.cpp
    render_n64.cpp
    n64_assets.cpp
    n64_fs.cpp
    n64_input.cpp
    n64_log_rate.cpp
    n64_memory.cpp
    rdp_command.cpp
    n64_bridge.c)

retro_engine_apply_flags(retro_engine_hal)
target_include_directories(retro_engine_hal PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${RPE_PUBLIC_INCLUDE_DIR}"
    "${RPE_PUBLIC_API_INCLUDE_DIR}"
    "${N64_INCLUDEDIR}")
target_compile_definitions(retro_engine_hal PRIVATE PLATFORM_N64=1)
# libdragon/newlib линкуются только финальным ROM-таргетом: static HAL не
# нуждается в них на этапе архива, а install(EXPORT) не должен ссылаться на
# machine-specific imported libdragon. N64 SDK (S45–46) потребует внешний
# N64_INST, как PS1 требует PSn00bSDK.
target_link_libraries(retro_engine_hal PRIVATE retro_engine_log)

# --- Bootable ELF ----------------------------------------------------------
add_executable(retro_n64 main_n64.cpp)
retro_engine_apply_flags(retro_n64)
target_include_directories(retro_n64 PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${RPE_PUBLIC_INCLUDE_DIR}"
    "${RPE_PUBLIC_API_INCLUDE_DIR}"
    "${N64_INCLUDEDIR}")
target_compile_definitions(retro_n64 PRIVATE PLATFORM_N64=1)
target_link_libraries(retro_n64 PRIVATE
    "-Wl,--start-group"
    retro_engine
    retro_engine_hal
    retro_engine_log
    rpe_n64_libdragon
    rpe_n64_libdragonsys
    c
    m
    gcc
    "-Wl,--end-group")
target_link_directories(retro_n64 PRIVATE "${N64_LIBDIR}")

set(RPE_N64_ELF "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.elf")
set(RPE_N64_MAP "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.map")
set(RPE_N64_SYM "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.sym")
set(RPE_N64_STRIPPED "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.elf.stripped")
set(RPE_N64_Z64 "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.z64")
set(RPE_N64_Z64_SHA "${RPE_N64_Z64}.sha256")

# --- S41–42: read-only DFS image (mkdfs) -----------------------------------
# Ассеты — committed versioned-артефакты (PS1/Saturn используют те же байты);
# имена на ROM-DFS — UPPERCASE 8.3, как CUBE.MSH/TEX0.TEX в HAL/доках.
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/assets/build/cube.mesh" OR
   NOT EXISTS "${CMAKE_SOURCE_DIR}/assets/build/texture_0.tex" OR
   NOT EXISTS "${CMAKE_SOURCE_DIR}/assets/build/triangle.mesh")
    message(FATAL_ERROR
        "assets/build/cube.mesh|texture_0.tex|triangle.mesh отсутствуют "
        "(versioned-ассеты обязаны присутствовать для N64 DFS).")
endif()

set(RPE_N64_DFS_ROOT "${CMAKE_CURRENT_BINARY_DIR}/dfs")
set(RPE_N64_DFS "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.dfs")
set(RPE_N64_DFS_TXT "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.dfs.txt")
set(RPE_N64_DFS_REPEAT "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.dfs.repeat")
set(RPE_N64_ASSET_SOURCES
    "${CMAKE_SOURCE_DIR}/assets/build/cube.mesh"
    "${CMAKE_SOURCE_DIR}/assets/build/texture_0.tex"
    "${CMAKE_SOURCE_DIR}/assets/build/triangle.mesh")

# mkdfs детерминирован: второй прогон на тот же каталог обязан дать
# побайтово тот же образ (проверка в сборке, не только в CI).
add_custom_command(
    OUTPUT "${RPE_N64_DFS}" "${RPE_N64_DFS_TXT}"
    BYPRODUCTS "${RPE_N64_DFS_REPEAT}"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${RPE_N64_DFS_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RPE_N64_DFS_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_SOURCE_DIR}/assets/build/cube.mesh" "${RPE_N64_DFS_ROOT}/CUBE.MSH"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_SOURCE_DIR}/assets/build/texture_0.tex" "${RPE_N64_DFS_ROOT}/TEX0.TEX"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_SOURCE_DIR}/assets/build/triangle.mesh" "${RPE_N64_DFS_ROOT}/TRIANGLE.MSH"
    COMMAND "${N64_BINDIR}/mkdfs" "retro_n64.dfs" "dfs"
    COMMAND "${N64_BINDIR}/mkdfs" "retro_n64.dfs.repeat" "dfs"
    COMMAND "${CMAKE_COMMAND}" -E compare_files "retro_n64.dfs" "retro_n64.dfs.repeat"
    COMMAND "${CMAKE_COMMAND}" -E remove "retro_n64.dfs.repeat"
    COMMAND "${CMAKE_COMMAND}"
            "-DDUMPDFS=${N64_BINDIR}/dumpdfs"
            "-DDFS=retro_n64.dfs"
            "-DOUT=retro_n64.dfs.txt"
            -P "${CMAKE_SOURCE_DIR}/cmake/dumpdfs.cmake"
    DEPENDS ${RPE_N64_ASSET_SOURCES} "${CMAKE_SOURCE_DIR}/cmake/dumpdfs.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "n64: retro_n64.dfs (deterministic mkdfs) + listing"
    VERBATIM)

set_target_properties(retro_n64 PROPERTIES
    OUTPUT_NAME "retro_n64"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# n64.ld — linker script libdragon; --wrap __do_global_ctors — раскрутка
# static-конструкторов (n64.mk); --gc-sections — компактный ROM.
target_link_options(retro_n64 PRIVATE
    "-Wl,-Tn64.ld"
    "-Wl,--gc-sections"
    "-Wl,--wrap=__do_global_ctors"
    "-Wl,-Map,${RPE_N64_MAP}")

# Map-файл создаёт линкер как побочный продукт; объявляем его явно, чтобы
# Ninja знал правило для alias-цели hello_n64. Копирование туда-обратно
# проверяет существование (pull) и не маскирует сбой линковки.
add_custom_command(
    OUTPUT "${RPE_N64_MAP}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RPE_N64_MAP}" "${RPE_N64_MAP}.tmp"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RPE_N64_MAP}.tmp" "${RPE_N64_MAP}"
    COMMAND "${CMAKE_COMMAND}" -E remove "${RPE_N64_MAP}.tmp"
    DEPENDS retro_n64
    COMMENT "n64: retro_n64.map (linker map)"
    VERBATIM)

# --- ROM: n64sym → strip → n64elfcompress → n64tool ------------------------
# Повторяет n64.mk (pinned libdragon) без Makefile-обвязки. N64_INST нужен
# n64sym/n64elfcompress для поиска toolchain (они не имеют --prefix).
add_custom_command(
    OUTPUT "${RPE_N64_Z64}"
    BYPRODUCTS "${RPE_N64_SYM}" "${RPE_N64_STRIPPED}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E env "N64_INST=${N64_INST}"
            "${N64_BINDIR}/n64sym" "retro_n64.elf" "retro_n64.sym"
    COMMAND "${CMAKE_COMMAND}" -E copy "retro_n64.elf" "retro_n64.elf.stripped"
    COMMAND "${N64_BINDIR}/mips64-elf-strip" -s "retro_n64.elf.stripped"
    COMMAND "${CMAKE_COMMAND}" -E env "N64_INST=${N64_INST}"
            "${N64_BINDIR}/n64elfcompress" -o "." -c 1 "retro_n64.elf.stripped"
    COMMAND "${N64_BINDIR}/n64tool"
            --title "RETRO POLYENGINE"
            --category N
            --region E
            --toc
            --output "retro_n64.z64"
            --align 256 "retro_n64.elf.stripped"
            --align 8 "retro_n64.sym"
            --align 16 "retro_n64.dfs"
            --align 8
    DEPENDS retro_n64 "${RPE_N64_DFS}"
    COMMENT "n64: hello_n64.z64 (IPL3 + ELF + TOC + DFS)"
    VERBATIM)

add_custom_command(
    OUTPUT "${RPE_N64_Z64_SHA}"
    COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${RPE_N64_Z64}"
            "-DOUTPUT=${RPE_N64_Z64_SHA}"
            -P "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
    DEPENDS "${RPE_N64_Z64}" "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
    COMMENT "n64: retro_n64.z64.sha256"
    VERBATIM)

# --- hello_n64.* alias (release/demo naming, docs/N64.md) ------------------
set(RPE_N64_HELLO_Z64 "${CMAKE_CURRENT_BINARY_DIR}/hello_n64.z64")
set(RPE_N64_HELLO_MAP "${CMAKE_CURRENT_BINARY_DIR}/hello_n64.map")
set(RPE_N64_HELLO_SHA "${RPE_N64_HELLO_Z64}.sha256")

add_custom_command(
    OUTPUT "${RPE_N64_HELLO_Z64}" "${RPE_N64_HELLO_MAP}" "${RPE_N64_HELLO_SHA}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RPE_N64_Z64}" "${RPE_N64_HELLO_Z64}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RPE_N64_MAP}" "${RPE_N64_HELLO_MAP}"
    COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${RPE_N64_HELLO_Z64}"
            "-DOUTPUT=${RPE_N64_HELLO_SHA}"
            -P "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
    DEPENDS "${RPE_N64_Z64}" "${RPE_N64_MAP}" "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
    COMMENT "n64: hello_n64.z64/.map/.sha256 alias"
    VERBATIM)

add_custom_target(n64_rom ALL
    DEPENDS "${RPE_N64_Z64}" "${RPE_N64_Z64_SHA}"
            "${RPE_N64_HELLO_Z64}" "${RPE_N64_HELLO_MAP}" "${RPE_N64_HELLO_SHA}"
            "${RPE_N64_DFS}" "${RPE_N64_DFS_TXT}")
add_custom_target(hello_n64 DEPENDS "${RPE_N64_HELLO_Z64}")

# Артефакты:
#   build/n64-release/src/hal/n64/retro_n64.elf
#   build/n64-release/src/hal/n64/retro_n64.map
#   build/n64-release/src/hal/n64/retro_n64.z64 (+ .sha256)
#   build/n64-release/src/hal/n64/retro_n64.dfs (+ .txt listing)
#   build/n64-release/src/hal/n64/hello_n64.z64/.map/.sha256
