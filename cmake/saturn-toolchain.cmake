# SPDX-License-Identifier: MIT
# ============================================================================
# cmake/saturn-toolchain.cmake — Sega Saturn cross-toolchain (ADR-008).
#
# Нормативные источники:
#   - docs/plans/sprint_21_22.md (недели 41–44);
#   - docs/adr/ADR-008-saturn-backend.md;
#   - docs/SATURN.md.
#
# Toolchain: GNU GCC 8.2.0 для sh-none-elf из pinned Jo Engine v2026.2
# (коммит 5e99944). Jo Engine НЕ вендорится в репозиторий: путь задаётся
# переменной окружения JO_ENGINE_ROOT (или -DJO_ENGINE_ROOT=...).
#
# Пример:
#   export JO_ENGINE_ROOT=$HOME/joengine
#   cmake --preset saturn-release
#
# Проверено на Linux; Windows-ветка использует bundled sh-elf-gcc.exe и
# документирована как экспериментальная (CI — Linux).
# ============================================================================

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR sh)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# --- Корень Jo Engine -------------------------------------------------------
if(NOT JO_ENGINE_ROOT AND DEFINED ENV{JO_ENGINE_ROOT})
    set(JO_ENGINE_ROOT "$ENV{JO_ENGINE_ROOT}" CACHE PATH "Jo Engine v2026.2 SDK/compiler root")
endif()
if(NOT JO_ENGINE_ROOT)
    message(FATAL_ERROR
        "JO_ENGINE_ROOT не задан. Скачайте Jo Engine v2026.2 (коммит "
        "5e99944238030821a48e17a40abefd77e81953c2) и выполните:\n"
        "  export JO_ENGINE_ROOT=/path/to/joengine\n"
        "  cmake --preset saturn-release\n"
        "См. docs/BUILDING.md § Saturn.")
endif()
get_filename_component(JO_ENGINE_ROOT "${JO_ENGINE_ROOT}" ABSOLUTE)

# --- Выбор платформенного каталога bundled-компилятора ----------------------
if(EXISTS "${JO_ENGINE_ROOT}/Compiler/LINUX/bin/sh-none-elf-gcc-8.2.0")
    set(JO_HOST_COMPILER_DIR "${JO_ENGINE_ROOT}/Compiler/LINUX")
    set(JO_ENGINE_CC "${JO_HOST_COMPILER_DIR}/bin/sh-none-elf-gcc-8.2.0")
    set(JO_ENGINE_OBJCOPY "${JO_HOST_COMPILER_DIR}/bin/sh-none-elf-objcopy")
    set(JO_ENGINE_GCC_INCLUDE_DIR "${JO_HOST_COMPILER_DIR}/lib/gcc/sh-none-elf/8.2.0/include")
    set(JO_ENGINE_GCC_INCLUDE_FIXED "${JO_HOST_COMPILER_DIR}/lib/gcc/sh-none-elf/8.2.0/include-fixed")
    set(JO_ENGINE_LIBGCC_DIR "${JO_HOST_COMPILER_DIR}/lib/gcc/sh-none-elf/8.2.0")
    set(JO_ENGINE_TARGET_LIB_DIR "${JO_HOST_COMPILER_DIR}/sh-none-elf/lib")
    set(JO_ENGINE_NEWLIB_INCLUDE "${JO_ENGINE_ROOT}/Compiler/WINDOWS/sh-elf/include")
elseif(EXISTS "${JO_ENGINE_ROOT}/Compiler/WINDOWS/bin/sh-elf-gcc.exe")
    set(JO_HOST_COMPILER_DIR "${JO_ENGINE_ROOT}/Compiler/WINDOWS")
    set(JO_ENGINE_CC "${JO_HOST_COMPILER_DIR}/bin/sh-elf-gcc.exe")
    set(JO_ENGINE_OBJCOPY "${JO_HOST_COMPILER_DIR}/bin/sh-elf-objcopy.exe")
    set(JO_ENGINE_GCC_INCLUDE_DIR "${JO_HOST_COMPILER_DIR}/lib/gcc/sh-elf/8.4.0/include")
    set(JO_ENGINE_GCC_INCLUDE_FIXED "${JO_HOST_COMPILER_DIR}/lib/gcc/sh-elf/8.4.0/include-fixed")
    set(JO_ENGINE_LIBGCC_DIR "${JO_HOST_COMPILER_DIR}/lib/gcc/sh-elf/8.4.0")
    set(JO_ENGINE_TARGET_LIB_DIR "${JO_HOST_COMPILER_DIR}/sh-elf/lib")
    set(JO_ENGINE_NEWLIB_INCLUDE "${JO_HOST_COMPILER_DIR}/sh-elf/include")
else()
    message(FATAL_ERROR
        "В JO_ENGINE_ROOT='${JO_ENGINE_ROOT}' нет bundled sh-elf-gcc. "
        "Ожидался Compiler/LINUX/bin/sh-none-elf-gcc-8.2.0 (Linux) или "
        "Compiler/WINDOWS/bin/sh-elf-gcc.exe (Windows).")
endif()

foreach(_jo_required
        "${JO_ENGINE_CC}"
        "${JO_ENGINE_OBJCOPY}"
        "${JO_ENGINE_GCC_INCLUDE_DIR}"
        "${JO_ENGINE_GCC_INCLUDE_FIXED}"
        "${JO_ENGINE_LIBGCC_DIR}"
        "${JO_ENGINE_TARGET_LIB_DIR}"
        "${JO_ENGINE_NEWLIB_INCLUDE}"
        "${JO_ENGINE_ROOT}/Compiler/COMMON/jo.linker"
        "${JO_ENGINE_ROOT}/jo_engine/jo/jo.h")
    if(NOT EXISTS "${_jo_required}")
        message(FATAL_ERROR
            "Saturn toolchain неполон: отсутствует '${_jo_required}'. "
            "Скачайте полный Jo Engine v2026.2 (не sparse-копию без Compiler/LINUX).")
    endif()
endforeach()

set(JO_ENGINE_LINKER_SCRIPT "${JO_ENGINE_ROOT}/Compiler/COMMON/jo.linker" CACHE FILEPATH "")
set(JO_ENGINE_INCLUDE_DIR "${JO_ENGINE_ROOT}/jo_engine" CACHE PATH "")

set(CMAKE_C_COMPILER "${JO_ENGINE_CC}" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${JO_ENGINE_CC}" CACHE FILEPATH "")
set(CMAKE_ASM_COMPILER "${JO_ENGINE_CC}" CACHE FILEPATH "")
set(CMAKE_OBJCOPY "${JO_ENGINE_OBJCOPY}" CACHE FILEPATH "")
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Хост-инструменты не ищем в target sysroot.
set(CMAKE_FIND_ROOT_PATH "")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)

# --- Флаги (Jo Engine Makefile + freestanding runtime) ----------------------
# -fplan9-extensions нужен для анонимных typedef-полей Jo Engine (C);
# C++ HAL видит те же заголовки, поэтому для C++ достаточно -fms-extensions.
set(_jo_common_flags
    "-m2"
    "-ffast-math"
    "-fno-common"
    "-fno-builtin"
    "-fno-ident"
    "-fno-isolate-erroneous-paths-dereference"
    "-fms-extensions")

string(REPLACE ";" " " _jo_common_flags_str "${_jo_common_flags}")

set(CMAKE_C_FLAGS_INIT
    "${_jo_common_flags_str} -std=gnu99 -fplan9-extensions")
set(CMAKE_CXX_FLAGS_INIT
    "${_jo_common_flags_str} -std=gnu++14 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -fno-unwind-tables -fno-asynchronous-unwind-tables")
set(CMAKE_ASM_FLAGS_INIT "-m2")
# -O2, не -O3: у SH-2 кэш 4 KiB; рекомендация Jo Engine Makefile.
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Saturn Release flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Saturn Release flags" FORCE)
set(CMAKE_ASM_FLAGS_RELEASE "" CACHE STRING "Saturn Release ASM flags" FORCE)

# Freestanding: без crt0/libc от GCC; start.S и jo.linker — часть Jo Engine.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-m2 -nostdlib -nostartfiles -nodefaultlibs")

# Кэш-переменные для src/hal/saturn/CMakeLists.txt.
set(JO_ENGINE_LIBGCC_DIR "${JO_ENGINE_LIBGCC_DIR}" CACHE PATH "")
set(JO_ENGINE_TARGET_LIB_DIR "${JO_ENGINE_TARGET_LIB_DIR}" CACHE PATH "")
set(JO_ENGINE_NEWLIB_INCLUDE "${JO_ENGINE_NEWLIB_INCLUDE}" CACHE PATH "")
set(JO_ENGINE_GCC_INCLUDE_FIXED "${JO_ENGINE_GCC_INCLUDE_FIXED}" CACHE PATH "")
