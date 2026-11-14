# ============================================================================
# HAL Nintendo 64 — libdragon (pinned commit) + mips64-elf-gcc 16.2.0.
# Реализация: Спринты 39–46 (docs/plans/sprint_39_40.md … sprint_45_46.md),
# ADR-014/ADR-015.
#
# Топология:
#   rpe_n64_libdragon   — imported static libdragon.a (SYSTEM headers);
#   rpe_n64_libdragonsys— imported static libdragonsys.a;
#   retro_engine_hal    — N64 HAL: lifecycle/input/FS/память/render;
#   hello_triangle_n64  — bootable ELF hello_triangle (S45–46);
#   rotating_cube_n64   — bootable ELF rotating_cube/W1 (S45–46);
#   retro_n64.dfs       — read-only DragonFS (mkdfs, deterministic);
#   hello_triangle.z64  — ROM (n64tool + TOC + ELF + DFS);
#   rotating_cube.z64   — ROM (W1-сцена, тот же DFS).
#
# Алиасы совместимости: hello_n64.z64 = hello_triangle.z64,
# retro_n64.z64 = rotating_cube.z64, retro_n64.elf/.map/.sym = rotating.
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
    rsp_tnl.cpp
    n64_render_bridge.c
    n64_bridge.c)

retro_engine_apply_flags(retro_engine_hal)
target_include_directories(retro_engine_hal PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${RPE_PUBLIC_INCLUDE_DIR}"
    "${RPE_PUBLIC_API_INCLUDE_DIR}"
    "${N64_INCLUDEDIR}")
target_compile_definitions(retro_engine_hal PRIVATE PLATFORM_N64=1)
# libdragon/newlib линкуются только финальными ROM-таргетами: static HAL не
# нуждается в них на этапе архива, а install(EXPORT) не должен ссылаться на
# machine-specific imported libdragon. N64 SDK (S45–46) требует внешний
# N64_INST, как PS1 требует PSn00bSDK.
target_link_libraries(retro_engine_hal PRIVATE retro_engine_log)

# --- S43–44: RSP T&L overlay (rsp_tnl.S) -----------------------------------
# Повторяет правило n64.mk для ucode: ассемблировать rsp-файл линкером с
# rsp.ld, вырезать .text/.data в бинарники, обернуть их в ELF-объекты с
# символами <name>_text_start/_end и <name>_data_start/_end (DEFINE_RSP_UCODE).
set(RPE_N64_UCODE_BASE "${CMAKE_CURRENT_BINARY_DIR}/rsp_tnl")
set(RPE_N64_UCODE_ELF "${RPE_N64_UCODE_BASE}.elf")
set(RPE_N64_UCODE_MAP "${RPE_N64_UCODE_BASE}.map")
set(RPE_N64_UCODE_TEXT_BIN "${RPE_N64_UCODE_BASE}.text.bin")
set(RPE_N64_UCODE_DATA_BIN "${RPE_N64_UCODE_BASE}.data.bin")
set(RPE_N64_UCODE_TEXT_OBJ "${RPE_N64_UCODE_BASE}.text.o")
set(RPE_N64_UCODE_DATA_OBJ "${RPE_N64_UCODE_BASE}.data.o")

add_custom_command(
    OUTPUT "${RPE_N64_UCODE_TEXT_OBJ}" "${RPE_N64_UCODE_DATA_OBJ}"
    BYPRODUCTS "${RPE_N64_UCODE_ELF}" "${RPE_N64_UCODE_MAP}"
               "${RPE_N64_UCODE_TEXT_BIN}" "${RPE_N64_UCODE_DATA_BIN}"
    COMMAND "${N64_BINDIR}/mips64-elf-gcc"
            -march=mips1 -mabi=32 -Wa,--fatal-warnings
            "-I${N64_INCLUDEDIR}"
            -nostartfiles -Wl,-Trsp.ld -Wl,--gc-sections
            "-Wl,-Map=${RPE_N64_UCODE_MAP}"
            -o "rsp_tnl.elf" "${CMAKE_CURRENT_SOURCE_DIR}/rsp_tnl.S"
    COMMAND "${N64_BINDIR}/mips64-elf-objcopy" -O binary -j .text
            "rsp_tnl.elf" "rsp_tnl.text.bin"
    COMMAND "${N64_BINDIR}/mips64-elf-objcopy" -O binary -j .data
            "rsp_tnl.elf" "rsp_tnl.data.bin"
    COMMAND "${N64_BINDIR}/mips64-elf-objcopy" -I binary -O elf32-bigmips -B mips4300
            --redefine-sym _binary_rsp_tnl_text_bin_start=rpe_tnl_text_start
            --redefine-sym _binary_rsp_tnl_text_bin_end=rpe_tnl_text_end
            --redefine-sym _binary_rsp_tnl_text_bin_size=rpe_tnl_text_size
            --set-section-alignment .data=8 --rename-section .text=.data
            "rsp_tnl.text.bin" "rsp_tnl.text.o"
    COMMAND "${N64_BINDIR}/mips64-elf-objcopy" -I binary -O elf32-bigmips -B mips4300
            --redefine-sym _binary_rsp_tnl_data_bin_start=rpe_tnl_data_start
            --redefine-sym _binary_rsp_tnl_data_bin_end=rpe_tnl_data_end
            --redefine-sym _binary_rsp_tnl_data_bin_size=rpe_tnl_data_size
            --set-section-alignment .data=8 --rename-section .text=.data
            "rsp_tnl.data.bin" "rsp_tnl.data.o"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/rsp_tnl.S"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "n64: RSP T&L overlay (rsp_tnl.S → .text/.data objects)"
    VERBATIM)

set_source_files_properties("${RPE_N64_UCODE_TEXT_OBJ}" "${RPE_N64_UCODE_DATA_OBJ}"
    PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)

# SDK (S45–46): RSP overlay поставляется отдельной статической библиотекой —
# consumer-пример линкует RetroPolyEngine::n64_rsp_tnl, не зная деталей
# генерации ucode. Символы rpe_tnl_{text,data}_start/_end глобальны.
add_library(retro_engine_n64_ucode STATIC
    "${RPE_N64_UCODE_TEXT_OBJ}" "${RPE_N64_UCODE_DATA_OBJ}")
add_library(RetroPolyEngine::n64_rsp_tnl ALIAS retro_engine_n64_ucode)
set_target_properties(retro_engine_n64_ucode PROPERTIES
    OUTPUT_NAME retro_engine_n64_ucode
    EXPORT_NAME n64_rsp_tnl
    LINKER_LANGUAGE C)
install(TARGETS retro_engine_n64_ucode
        EXPORT RetroPolyEngineTargets
        ARCHIVE DESTINATION "${RPE_SDK_LIBDIR}")
# Исходник/карта overlay — для аудита SDK; MIPS-инструменты не требуются.
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/rsp_tnl.S"
              "${RPE_N64_UCODE_MAP}"
        DESTINATION "${RPE_SDK_LIBDIR}/rsp")
install(FILES "${RPE_N64_UCODE_ELF}" DESTINATION "${RPE_SDK_LIBDIR}/rsp")

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

# --- Demo executables: hello_triangle + rotating_cube (S45–46) --------------
# Один main_n64.cpp, две специализации: RPE_N64_DEMO_HELLO=1 — TRIANGLE.MSH
# с интерактивной камерой; =0 — W1-сцена 4 куба + RSP-lighting.
function(rpe_n64_add_demo demo_target demo_hello elf_name rom_name)
    add_executable(${demo_target} main_n64.cpp
        "${RPE_N64_UCODE_TEXT_OBJ}" "${RPE_N64_UCODE_DATA_OBJ}")
    retro_engine_apply_flags(${demo_target})
    target_include_directories(${demo_target} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${RPE_PUBLIC_INCLUDE_DIR}"
        "${RPE_PUBLIC_API_INCLUDE_DIR}"
        "${N64_INCLUDEDIR}")
    target_compile_definitions(${demo_target} PRIVATE
        PLATFORM_N64=1
        RPE_N64_DEMO_HELLO=${demo_hello})
    target_link_libraries(${demo_target} PRIVATE
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
    target_link_directories(${demo_target} PRIVATE "${N64_LIBDIR}")

    set_target_properties(${demo_target} PROPERTIES
        OUTPUT_NAME "${elf_name}"
        SUFFIX ".elf"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

    # n64.ld — linker script libdragon; --wrap __do_global_ctors — раскрутка
    # static-конструкторов (n64.mk); --gc-sections — компактный ROM.
    set(map "${CMAKE_CURRENT_BINARY_DIR}/${elf_name}.map")
    target_link_options(${demo_target} PRIVATE
        "-Wl,-Tn64.ld"
        "-Wl,--gc-sections"
        "-Wl,--wrap=__do_global_ctors"
        "-Wl,-Map,${map}")

    # Map-файл создаёт линкер как побочный продукт; объявляем его явно, чтобы
    # Ninja знал правило; pull-by-copy проверяет существование.
    add_custom_command(
        OUTPUT "${map}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${map}" "${map}.tmp"
        COMMAND "${CMAKE_COMMAND}" -E copy "${map}.tmp" "${map}"
        COMMAND "${CMAKE_COMMAND}" -E remove "${map}.tmp"
        DEPENDS ${demo_target}
        COMMENT "n64: ${elf_name}.map (linker map)"
        VERBATIM)

    set(elf "${CMAKE_CURRENT_BINARY_DIR}/${elf_name}.elf")
    set(sym "${CMAKE_CURRENT_BINARY_DIR}/${elf_name}.sym")
    set(stripped "${CMAKE_CURRENT_BINARY_DIR}/${elf_name}.elf.stripped")
    set(rom "${CMAKE_CURRENT_BINARY_DIR}/${rom_name}")
    set(rom_sha "${rom}.sha256")

    # ROM: n64sym → strip → n64elfcompress → n64tool (TOC + ELF + DFS).
    add_custom_command(
        OUTPUT "${rom}"
        BYPRODUCTS "${sym}" "${stripped}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E env "N64_INST=${N64_INST}"
                "${N64_BINDIR}/n64sym" "${elf_name}.elf" "${elf_name}.sym"
        COMMAND "${CMAKE_COMMAND}" -E copy "${elf_name}.elf" "${elf_name}.elf.stripped"
        COMMAND "${N64_BINDIR}/mips64-elf-strip" -s "${elf_name}.elf.stripped"
        COMMAND "${CMAKE_COMMAND}" -E env "N64_INST=${N64_INST}"
                "${N64_BINDIR}/n64elfcompress" -o "." -c 1 "${elf_name}.elf.stripped"
        COMMAND "${N64_BINDIR}/n64tool"
                --title "RETRO POLYENGINE"
                --category N
                --region E
                --toc
                --output "${rom_name}"
                --align 256 "${elf_name}.elf.stripped"
                --align 8 "${elf_name}.sym"
                --align 16 "retro_n64.dfs"
                --align 8
        DEPENDS ${demo_target} "${RPE_N64_DFS}"
        COMMENT "n64: ${rom_name} (IPL3 + ELF + TOC + DFS)"
        VERBATIM)

    add_custom_command(
        OUTPUT "${rom_sha}"
        COMMAND "${CMAKE_COMMAND}"
                "-DINPUT=${rom}"
                "-DOUTPUT=${rom_sha}"
                -P "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
        DEPENDS "${rom}" "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
        COMMENT "n64: ${rom_name}.sha256"
        VERBATIM)

    set(RPE_N64_ALL_ARTIFACTS ${RPE_N64_ALL_ARTIFACTS}
        "${rom}" "${rom_sha}" "${map}" PARENT_SCOPE)
endfunction()

rpe_n64_add_demo(hello_triangle_n64 1 hello_triangle_n64 hello_triangle.z64)
rpe_n64_add_demo(rotating_cube_n64 0 rotating_cube_n64 rotating_cube.z64)

# --- Back-compat aliases (docs/N64.md §3.2 / CI S39–44) ---------------------
set(RPE_N64_HELLO_Z64 "${CMAKE_CURRENT_BINARY_DIR}/hello_triangle.z64")
set(RPE_N64_HELLO_MAP "${CMAKE_CURRENT_BINARY_DIR}/hello_triangle_n64.map")
set(RPE_N64_HELLO_ALIAS "${CMAKE_CURRENT_BINARY_DIR}/hello_n64.z64")
set(RPE_N64_HELLO_ALIAS_MAP "${CMAKE_CURRENT_BINARY_DIR}/hello_n64.map")
set(RPE_N64_HELLO_ALIAS_SHA "${RPE_N64_HELLO_ALIAS}.sha256")

add_custom_command(
    OUTPUT "${RPE_N64_HELLO_ALIAS}" "${RPE_N64_HELLO_ALIAS_MAP}"
           "${RPE_N64_HELLO_ALIAS_SHA}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RPE_N64_HELLO_Z64}" "${RPE_N64_HELLO_ALIAS}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RPE_N64_HELLO_MAP}" "${RPE_N64_HELLO_ALIAS_MAP}"
    COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${RPE_N64_HELLO_ALIAS}"
            "-DOUTPUT=${RPE_N64_HELLO_ALIAS_SHA}"
            -P "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
    DEPENDS "${RPE_N64_HELLO_Z64}" "${RPE_N64_HELLO_MAP}"
            "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
    COMMENT "n64: hello_n64.z64/.map/.sha256 alias (hello_triangle)"
    VERBATIM)

set(RPE_N64_CUBE_Z64 "${CMAKE_CURRENT_BINARY_DIR}/rotating_cube.z64")
set(RPE_N64_CUBE_MAP "${CMAKE_CURRENT_BINARY_DIR}/rotating_cube_n64.map")
set(RPE_N64_ENGINE_ALIAS "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.z64")
set(RPE_N64_ENGINE_ALIAS_MAP "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.map")
set(RPE_N64_ENGINE_ALIAS_SHA "${RPE_N64_ENGINE_ALIAS}.sha256")
set(RPE_N64_ENGINE_ELF "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.elf")
set(RPE_N64_ENGINE_SYM "${CMAKE_CURRENT_BINARY_DIR}/retro_n64.sym")

add_custom_command(
    OUTPUT "${RPE_N64_ENGINE_ALIAS}" "${RPE_N64_ENGINE_ALIAS_MAP}"
           "${RPE_N64_ENGINE_ALIAS_SHA}" "${RPE_N64_ENGINE_ELF}"
           "${RPE_N64_ENGINE_SYM}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RPE_N64_CUBE_Z64}" "${RPE_N64_ENGINE_ALIAS}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RPE_N64_CUBE_MAP}" "${RPE_N64_ENGINE_ALIAS_MAP}"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/rotating_cube_n64.elf" "${RPE_N64_ENGINE_ELF}"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/rotating_cube_n64.sym" "${RPE_N64_ENGINE_SYM}"
    COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${RPE_N64_ENGINE_ALIAS}"
            "-DOUTPUT=${RPE_N64_ENGINE_ALIAS_SHA}"
            -P "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
    DEPENDS "${RPE_N64_CUBE_Z64}" "${RPE_N64_CUBE_MAP}"
            "${CMAKE_CURRENT_BINARY_DIR}/rotating_cube_n64.elf"
            "${CMAKE_CURRENT_BINARY_DIR}/rotating_cube_n64.sym"
            "${CMAKE_SOURCE_DIR}/cmake/sha256_file.cmake"
    COMMENT "n64: retro_n64.* aliases (rotating_cube)"
    VERBATIM)

add_custom_target(n64_rom ALL
    DEPENDS ${RPE_N64_ALL_ARTIFACTS}
            "${RPE_N64_HELLO_ALIAS}" "${RPE_N64_HELLO_ALIAS_MAP}"
            "${RPE_N64_HELLO_ALIAS_SHA}"
            "${RPE_N64_ENGINE_ALIAS}" "${RPE_N64_ENGINE_ALIAS_MAP}"
            "${RPE_N64_ENGINE_ALIAS_SHA}" "${RPE_N64_ENGINE_ELF}"
            "${RPE_N64_ENGINE_SYM}"
            "${RPE_N64_DFS}" "${RPE_N64_DFS_TXT}")
add_custom_target(hello_n64 DEPENDS "${RPE_N64_HELLO_Z64}")
add_custom_target(rotating_cube DEPENDS "${RPE_N64_CUBE_Z64}")
add_custom_target(hello_triangle DEPENDS "${RPE_N64_HELLO_Z64}")

# Артефакты:
#   build/n64-release/src/hal/n64/hello_triangle_n64.elf
#   build/n64-release/src/hal/n64/rotating_cube_n64.elf
#   build/n64-release/src/hal/n64/hello_triangle.z64 (+ .sha256)
#   build/n64-release/src/hal/n64/rotating_cube.z64 (+ .sha256)
#   build/n64-release/src/hal/n64/hello_n64.z64 / retro_n64.z64 (aliases)
#   build/n64-release/src/hal/n64/retro_n64.dfs (+ .txt listing)
