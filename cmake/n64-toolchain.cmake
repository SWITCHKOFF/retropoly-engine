# SPDX-License-Identifier: MIT
# ============================================================================
# cmake/n64-toolchain.cmake — Nintendo 64 cross-toolchain (ADR-014).
#
# Нормативные источники:
#   - docs/plans/sprint_39_40.md (недели 77–80);
#   - docs/adr/ADR-014-n64-backend.md;
#   - docs/N64.md.
#
# Toolchain: GNU GCC 16.2.0 для mips64-elf из официального
# libdragon-toolchain release (deb), SHA-256 зафиксирован в
# ci/n64-toolchain.sha256. libdragon НЕ вендорится в репозиторий:
#   N64_INST       — install-prefix (bin/, mips64-elf/lib|include|bin);
#   LIBDRAGON_ROOT — git-checkout libdragon (проверка pinned commit).
#
# Пример:
#   export N64_INST=$HOME/n64-sdk/toolchain/opt/libdragon
#   export LIBDRAGON_ROOT=$HOME/n64-sdk/libdragon
#   cmake --preset n64-release
#
# Сборка проверена на Linux x86_64; Windows — экспериментально
# (mips64-elf-gcc.exe + n64tool.exe из win64 toolchain).
# ============================================================================

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR mips64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# --- N64_INST: toolchain + установленный libdragon -------------------------
if(NOT N64_INST AND DEFINED ENV{N64_INST})
    set(N64_INST "$ENV{N64_INST}" CACHE PATH "libdragon/toolchain install prefix")
endif()
if(NOT N64_INST)
    message(FATAL_ERROR
        "N64_INST не задан. Установите pinned mips64-elf toolchain + libdragon "
        "(docs/BUILDING.md § Nintendo 64) и выполните:\n"
        "  export N64_INST=/path/to/libdragon-toolchain\n"
        "  cmake --preset n64-release")
endif()
get_filename_component(N64_INST "${N64_INST}" ABSOLUTE)

set(N64_TARGET "mips64-elf" CACHE STRING "GNU target triplet (pinned)")
set(N64_BINDIR "${N64_INST}/bin" CACHE PATH "Toolchain tools (n64tool/n64sym)")
set(N64_INCLUDEDIR "${N64_INST}/${N64_TARGET}/include" CACHE PATH "libdragon/newlib headers")
set(N64_LIBDIR "${N64_INST}/${N64_TARGET}/lib" CACHE PATH "libdragon/newlib archives")

# --- Обязательные компоненты pinned toolchain -----------------------------
foreach(_n64_required
        "${N64_BINDIR}/mips64-elf-gcc"
        "${N64_BINDIR}/mips64-elf-g++"
        "${N64_BINDIR}/mips64-elf-ld"
        "${N64_BINDIR}/mips64-elf-strip"
        "${N64_BINDIR}/n64tool"
        "${N64_BINDIR}/n64sym"
        "${N64_BINDIR}/n64elfcompress"
        "${N64_BINDIR}/mkdfs"
        "${N64_BINDIR}/dumpdfs"
        "${N64_INST}/include/n64.mk"
        "${N64_INCLUDEDIR}/libdragon.h"
        "${N64_LIBDIR}/libdragon.a"
        "${N64_LIBDIR}/libdragonsys.a"
        "${N64_LIBDIR}/n64.ld")
    if(NOT EXISTS "${_n64_required}")
        message(FATAL_ERROR
            "N64 toolchain неполон: отсутствует '${_n64_required}'. "
            "N64_INST должен указывать на install-prefix pinned "
            "libdragon-toolchain (bin/ + mips64-elf/{include,lib}). "
            "См. docs/BUILDING.md § Nintendo 64.")
    endif()
endforeach()

# --- Проверка target/версии компилятора ------------------------------------
execute_process(
    COMMAND "${N64_BINDIR}/mips64-elf-gcc" -dumpmachine
    OUTPUT_VARIABLE _n64_machine
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE _n64_machine_err)
if(NOT _n64_machine MATCHES "^mips64")
    message(FATAL_ERROR
        "mips64-elf-gcc -dumpmachine = '${_n64_machine}' (${_n64_machine_err}): "
        "ожидается mips64-elf. Это не N64 cross-toolchain.")
endif()

execute_process(
    COMMAND "${N64_BINDIR}/mips64-elf-gcc" -dumpversion
    OUTPUT_VARIABLE _n64_gcc_version
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _n64_gcc_version STREQUAL "16.2.0")
    message(FATAL_ERROR
        "mips64-elf-gcc ${_n64_gcc_version}: pinned toolchain — GCC 16.2.0 "
        "(ci/n64-toolchain.sha256). Обновление toolchain — отдельная задача с ADR.")
endif()

# --- libdragon: проверка pinned commit (если задан git-checkout) -----------
get_filename_component(_n64_repo_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
file(STRINGS "${_n64_repo_root}/ci/n64-libdragon.commit" _n64_libdragon_lines)
set(_n64_libdragon_pin "")
foreach(_n64_line IN LISTS _n64_libdragon_lines)
    string(STRIP "${_n64_line}" _n64_line)
    string(LENGTH "${_n64_line}" _n64_len)
    if(_n64_len EQUAL 40 AND _n64_line MATCHES "^[0-9a-f]+$")
        set(_n64_libdragon_pin "${_n64_line}")
    endif()
endforeach()
if(NOT _n64_libdragon_pin)
    message(FATAL_ERROR "ci/n64-libdragon.commit: не найден 40-hex commit")
endif()
set(N64_LIBDRAGON_COMMIT "${_n64_libdragon_pin}"
    CACHE STRING "Pinned libdragon commit (ci/n64-libdragon.commit)")
if(DEFINED LIBDRAGON_ROOT AND NOT LIBDRAGON_ROOT STREQUAL "")
    get_filename_component(LIBDRAGON_ROOT "${LIBDRAGON_ROOT}" ABSOLUTE)
    if(EXISTS "${LIBDRAGON_ROOT}/.git")
        execute_process(
            COMMAND git -C "${LIBDRAGON_ROOT}" rev-parse HEAD
            OUTPUT_VARIABLE _n64_libdragon_head
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _n64_libdragon_git_result)
        if(_n64_libdragon_git_result EQUAL 0 AND
           NOT "${_n64_libdragon_head}" STREQUAL "${N64_LIBDRAGON_COMMIT}")
            message(FATAL_ERROR
                "LIBDRAGON_ROOT HEAD=${_n64_libdragon_head}, ожидается pinned "
                "commit ${N64_LIBDRAGON_COMMIT}. Обновление libdragon — "
                "отдельная задача с ADR (ADR-014).")
        endif()
    else()
        message(STATUS
            "N64: LIBDRAGON_ROOT без .git — commit не проверяется "
            "(для CI задавайте git-checkout).")
    endif()
    set(LIBDRAGON_ROOT "${LIBDRAGON_ROOT}" CACHE PATH "libdragon source checkout")
endif()

# --- Компиляторы и утилиты ------------------------------------------------
set(CMAKE_C_COMPILER "${N64_BINDIR}/mips64-elf-gcc" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${N64_BINDIR}/mips64-elf-g++" CACHE FILEPATH "")
set(CMAKE_ASM_COMPILER "${N64_BINDIR}/mips64-elf-gcc" CACHE FILEPATH "")
set(CMAKE_LINKER "${N64_BINDIR}/mips64-elf-ld" CACHE FILEPATH "")
set(CMAKE_OBJCOPY "${N64_BINDIR}/mips64-elf-objcopy" CACHE FILEPATH "")
set(CMAKE_AR "${N64_BINDIR}/mips64-elf-gcc-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${N64_BINDIR}/mips64-elf-gcc-ranlib" CACHE FILEPATH "")
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# --- Флаги (повторяют N64_CFLAGS/N64_CXXFLAGS libdragon n64.mk) -------------
set(_n64_common
    "-march=vr4300"
    "-mtune=vr4300"
    "-mabi=o64"
    "-falign-functions=32"
    "-ffunction-sections"
    "-fdata-sections"
    "-ffast-math"
    "-ftrapping-math"
    "-fno-associative-math"
    "-DN64"
    "-DPLATFORM_N64=1")
string(REPLACE ";" " " _n64_common_str "${_n64_common}")

set(CMAKE_C_FLAGS_INIT "${_n64_common_str} -O2 -g")
set(CMAKE_CXX_FLAGS_INIT
    "${_n64_common_str} -O2 -g -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-unwind-tables -fno-asynchronous-unwind-tables")
set(CMAKE_ASM_FLAGS_INIT "-march=vr4300 -mtune=vr4300 -mabi=o64 -g")
set(CMAKE_C_FLAGS_RELEASE "-O2" CACHE STRING "N64 Release C flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O2" CACHE STRING "N64 Release C++ flags" FORCE)
set(CMAKE_ASM_FLAGS_RELEASE "" CACHE STRING "N64 Release ASM flags" FORCE)

# Хост-утилиты (Python/git/ninja) не ищем в target sysroot.
set(CMAKE_FIND_ROOT_PATH "${N64_INST}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)

message(STATUS "N64 toolchain: ${N64_BINDIR}/mips64-elf-gcc ${_n64_gcc_version}")
message(STATUS "N64 libdragon: ${N64_LIBDIR}/libdragon.a")
