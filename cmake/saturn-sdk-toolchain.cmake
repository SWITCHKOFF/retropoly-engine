# SPDX-License-Identifier: MIT
# ============================================================================
# cmake/saturn-sdk-toolchain.cmake — Saturn SDK toolchain wrapper (S33–34).
#
# Требует pinned Jo Engine v2026.2 (компилятор + linker script) в
# JO_ENGINE_ROOT; библиотеки движка берутся из установленного SDK.
# ============================================================================
if(NOT DEFINED JO_ENGINE_ROOT AND DEFINED ENV{JO_ENGINE_ROOT})
    set(JO_ENGINE_ROOT "$ENV{JO_ENGINE_ROOT}" CACHE PATH "Jo Engine v2026.2 root")
endif()
if(NOT JO_ENGINE_ROOT)
    message(FATAL_ERROR
        "JO_ENGINE_ROOT не задан. Скачайте Jo Engine v2026.2 и выполните:\n"
        "  export JO_ENGINE_ROOT=/path/to/joengine")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/saturn-toolchain.cmake")
set(PLATFORM_SATURN ON CACHE BOOL "RetroPolyEngine Saturn target" FORCE)

# newlib-заголовки bundled-компилятора (как в корневом CMake движка):
# без них stdint.h/stddef.h не находятся при сборке SDK-потребителя.
include_directories(SYSTEM
    "${JO_ENGINE_NEWLIB_INCLUDE}"
    "${JO_ENGINE_GCC_INCLUDE_FIXED}")

# Профиль Jo Engine, с которым собраны библиотеки движка (ADR-008):
# заголовки Jo Engine без этих define не компилируются.
add_compile_definitions(
    JO_MINIMAL=1
    JO_NTSC_VERSION=1
    JO_SCREEN_WIDTH=320
    JO_SCREEN_HEIGHT=224
    JO_FRAMERATE=1
    JO_MAX_SPRITE=255
    JO_MAX_FILE_IN_IMAGE_PACK=32
    JO_MAP_MAX_LAYER=8
    JO_MAX_SPRITE_ANIM=16
    JO_MAX_FS_BACKGROUND_JOBS=1
    JO_GLOBAL_MEMORY_SIZE_FOR_MALLOC=393216
    JO_COMPILE_WITH_FS_SUPPORT=1
    JO_COMPILE_WITH_PRINTF_SUPPORT=1
    JO_COMPILE_WITH_DUAL_CPU_SUPPORT=1)
