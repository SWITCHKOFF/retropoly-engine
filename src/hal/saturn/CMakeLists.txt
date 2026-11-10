# ============================================================================
# HAL Saturn — Jo Engine v2026.2 (pinned, commit 5e99944) + sh-none-elf-gcc.
# Реализация: Спринт 21–22 (docs/plans/sprint_21_22.md), ADR-008.
#
# Топология:
#   jo_engine         — C-ядро Jo Engine (минимальный профиль JO_MINIMAL=1);
#   retro_engine_hal  — C++ HAL: platform/input/render/FS stubs + C-мост;
#   retro_saturn      — bootable SH-2 ELF (start.S + jo_engine + HAL);
#   saturn_disc       — IP.BIN + 0.bin + ISO9660 (2048) + CUE.
#
# Сборка — только через пресет saturn-release:
#   export JO_ENGINE_ROOT=/path/to/joengine-v2026.2
#   cmake --preset saturn-release
#   cmake --build --preset saturn-release
# ============================================================================

if(NOT DEFINED JO_ENGINE_ROOT OR NOT DEFINED JO_ENGINE_INCLUDE_DIR)
    message(FATAL_ERROR
        "HAL Saturn требует toolchain cmake/saturn-toolchain.cmake. "
        "Сборка — только через пресет saturn-release (JO_ENGINE_ROOT).")
endif()

# --- Jo Engine: минимальный профиль (повторяет JO_MINIMAL=1 makefile) ------
set(JO_ENGINE_SOURCES
    "${JO_ENGINE_INCLUDE_DIR}/font.c"
    "${JO_ENGINE_INCLUDE_DIR}/input.c"
    "${JO_ENGINE_INCLUDE_DIR}/physics.c"
    "${JO_ENGINE_INCLUDE_DIR}/core.c"
    "${JO_ENGINE_INCLUDE_DIR}/math.c"
    "${JO_ENGINE_INCLUDE_DIR}/malloc.c"
    "${JO_ENGINE_INCLUDE_DIR}/tools.c"
    "${JO_ENGINE_INCLUDE_DIR}/palette.c"
    "${JO_ENGINE_INCLUDE_DIR}/sprites.c"
    "${JO_ENGINE_INCLUDE_DIR}/map.c"
    "${JO_ENGINE_INCLUDE_DIR}/list.c"
    "${JO_ENGINE_INCLUDE_DIR}/sprite_animator.c"
    "${JO_ENGINE_INCLUDE_DIR}/image.c"
    "${JO_ENGINE_INCLUDE_DIR}/vdp2.c"
    "${JO_ENGINE_INCLUDE_DIR}/time.c"
    "${JO_ENGINE_INCLUDE_DIR}/vdp1_command_pipeline.c"
    "${JO_ENGINE_INCLUDE_DIR}/vdp2_malloc.c"
    "${JO_ENGINE_INCLUDE_DIR}/smpc.c"
    "${JO_ENGINE_INCLUDE_DIR}/dsp.c"
    "${JO_ENGINE_INCLUDE_DIR}/start.S"
)

foreach(_jo_src IN LISTS JO_ENGINE_SOURCES)
    if(NOT EXISTS "${_jo_src}")
        message(FATAL_ERROR
            "jo_engine: отсутствует исходник '${_jo_src}'. "
            "Проверьте JO_ENGINE_ROOT (нужен полный Jo Engine v2026.2).")
    endif()
endforeach()

add_library(jo_engine STATIC ${JO_ENGINE_SOURCES})
target_include_directories(jo_engine SYSTEM PUBLIC "${JO_ENGINE_INCLUDE_DIR}")
set(JO_ENGINE_DEFINES
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
    JO_GLOBAL_MEMORY_SIZE_FOR_MALLOC=393216)

target_compile_options(jo_engine PRIVATE
    -fplan9-extensions
    -fno-common
    -ffast-math
    -fms-extensions
    -fno-builtin
    -fno-isolate-erroneous-paths-dereference)
target_compile_definitions(jo_engine PRIVATE ${JO_ENGINE_DEFINES})
# JO_DEBUG намеренно НЕ определяется: release-профиль (JO_DEBUG=0) — иначе
# #ifdef JO_DEBUG включает диагностический error screen и лишний код.

# --- HAL library ------------------------------------------------------------
add_library(retro_engine_hal STATIC
    platform_saturn.cpp
    render_saturn.cpp
    jo_saturn_bridge.c)
retro_engine_apply_flags(retro_engine_hal)
target_include_directories(retro_engine_hal PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${JO_ENGINE_INCLUDE_DIR}")
target_compile_definitions(retro_engine_hal PRIVATE PLATFORM_SATURN=1 ${JO_ENGINE_DEFINES})
target_link_libraries(retro_engine_hal PRIVATE retro_engine_log jo_engine)

# --- Bootable ELF -----------------------------------------------------------
add_executable(retro_saturn main_saturn.cpp)
retro_engine_apply_flags(retro_saturn)
target_include_directories(retro_saturn PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_definitions(retro_saturn PRIVATE PLATFORM_SATURN=1)

set(RPE_SATURN_ELF "${CMAKE_CURRENT_BINARY_DIR}/retro_saturn.elf")
set(RPE_SATURN_RAW "${CMAKE_CURRENT_BINARY_DIR}/retro_saturn.bin")
set(RPE_SATURN_MAP "${CMAKE_CURRENT_BINARY_DIR}/retro_saturn.map")
set_target_properties(retro_saturn PROPERTIES
    OUTPUT_NAME "retro_saturn"
    SUFFIX ".elf"
    LINK_DEPENDS "${JO_ENGINE_LINKER_SCRIPT}")

# Jo Engine — whole-archive (start.S + JO_WORK-секции должны попасть в ELF
# даже без прямой ссылки); движковые архивы — в --start-group (циклические
# ссылки HAL/log/ядро на однопроходном ld). libc/libgcc — тем же приёмом.
target_link_libraries(retro_saturn PRIVATE
    "-Wl,--whole-archive" "$<TARGET_FILE:jo_engine>" "-Wl,--no-whole-archive"
    "-Wl,--start-group"
    "$<TARGET_FILE:retro_engine>"
    "$<TARGET_FILE:retro_engine_hal>"
    "$<TARGET_FILE:retro_engine_log>"
    "-Wl,--end-group"
    "-Wl,--start-group" "-lc" "-lnosys" "-lgcc" "-Wl,--end-group")
target_link_directories(retro_saturn PRIVATE
    "${JO_ENGINE_TARGET_LIB_DIR}"
    "${JO_ENGINE_LIBGCC_DIR}")
target_link_options(retro_saturn PRIVATE
    "LINKER:-T,${JO_ENGINE_LINKER_SCRIPT}"
    "LINKER:-Map,${RPE_SATURN_MAP}"
    "LINKER:-e,___Start"
    "LINKER:--relax")

# --- IP.BIN (security/IP header; template — Jo Engine COMMON) ---------------
set(RPE_SATURN_IP "${CMAKE_CURRENT_BINARY_DIR}/IP.BIN")
set(RPE_SATURN_IP_OBJ "${CMAKE_CURRENT_BINARY_DIR}/ip.o")
set(RPE_JO_GAME_TITLE "RetroPolyEngine")
set(RPE_JO_GAME_VERSION "V1.000")
set(RPE_JO_GAME_RELEASE_DATE "20261109")
set(RPE_JO_GAME_MAKER_ID "RETROPOLY")
set(RPE_JO_GAME_PRODUCT_NUMBER "RPE-0001")
set(RPE_JO_GAME_PERIPHERALS "J")

add_custom_command(
    OUTPUT "${RPE_SATURN_IP}"
    COMMAND "${CMAKE_C_COMPILER}" -m2 -c
        "-DJO_IP_TITLE=\"${RPE_JO_GAME_TITLE}\""
        "-DJO_IP_VERSION=\"${RPE_JO_GAME_VERSION}\""
        "-DJO_IP_RELEASE_DATE=\"${RPE_JO_GAME_RELEASE_DATE}\""
        "-DJO_IP_MAKER_ID=\"${RPE_JO_GAME_MAKER_ID}\""
        "-DJO_IP_PRODUCT_NUMBER=\"${RPE_JO_GAME_PRODUCT_NUMBER}\""
        "-DJO_IP_PERIPHERALS=\"${RPE_JO_GAME_PERIPHERALS}\""
        "-Wa,-I${JO_ENGINE_ROOT}/Compiler/COMMON"
        "${JO_ENGINE_ROOT}/Compiler/COMMON/ip.S"
        -o "${RPE_SATURN_IP_OBJ}"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "${RPE_SATURN_IP_OBJ}" "${RPE_SATURN_IP}"
    DEPENDS "${JO_ENGINE_ROOT}/Compiler/COMMON/ip.S"
            "${JO_ENGINE_ROOT}/Compiler/COMMON/IP.BIN"
    COMMENT "saturn: IP.BIN (RetroPolyEngine)"
    VERBATIM)

# --- Bootable ISO9660 (2048-byte sectors) + CUE ------------------------------
# Предпочитаем host-утилиты mkisofs/genisoimage; в изолированных окружениях
# (Flatpak и др.) fallback на tools/saturn_mkiso.py + pycdlib.
find_program(RPE_MKISOFS NAMES mkisofs genisoimage)
if(RPE_MKISOFS)
    set(RPE_SATURN_MKISO_CMD "${RPE_MKISOFS}"
        -quiet
        -sysid "SEGA SATURN"
        -volid "RETROENG"
        -volset "RETROENG"
        -publisher "SEGA ENTERPRISES, LTD."
        -preparer "SEGA ENTERPRISES, LTD."
        -appid "RetroPolyEngine"
        -abstract "disc/ABS.TXT"
        -copyright "disc/CPY.TXT"
        -biblio "disc/BIB.TXT"
        -generic-boot "IP.BIN"
        -full-iso9660-filenames
        -o "retro_saturn.iso"
        "disc")
else()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "import pycdlib"
        RESULT_VARIABLE _rpe_pycdlib_ok
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT _rpe_pycdlib_ok EQUAL 0)
        message(FATAL_ERROR
            "saturn-release требует mkisofs/genisoimage ИЛИ Python3 с pycdlib. "
            "Установите пакет genisoimage или выполните: "
            "python3 -m pip install pycdlib.")
    endif()
    set(RPE_SATURN_MKISO_CMD
        "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tools/saturn_mkiso.py"
        --output "retro_saturn.iso"
        --ip-bin "IP.BIN"
        --input-dir "disc"
        --vol-id "RETROENG"
        --sys-id "SEGA SATURN"
        --publisher "SEGA ENTERPRISES, LTD."
        --preparer "SEGA ENTERPRISES, LTD."
        --app-id "RetroPolyEngine"
        --abstract "disc/ABS.TXT"
        --copyright "disc/CPY.TXT"
        --biblio "disc/BIB.TXT")
endif()

set(RPE_SATURN_DISC_DIR "${CMAKE_CURRENT_BINARY_DIR}/disc")
set(RPE_SATURN_ISO "${CMAKE_CURRENT_BINARY_DIR}/retro_saturn.iso")
set(RPE_SATURN_CUE "${CMAKE_CURRENT_BINARY_DIR}/retro_saturn.cue")
file(MAKE_DIRECTORY "${RPE_SATURN_DISC_DIR}")
file(WRITE "${RPE_SATURN_DISC_DIR}/ABS.TXT" "RetroPolyEngine Saturn skeleton\n")
file(WRITE "${RPE_SATURN_DISC_DIR}/CPY.TXT" "RetroPolyEngine contributors\n")
file(WRITE "${RPE_SATURN_DISC_DIR}/BIB.TXT" "docs/SATURN.md\n")
# Jo Engine Linux CueMaker выпускает MODE1/2048 (ISO9660, 2048-byte
# sectors); genisoimage cdrkit не умеет 2352. Это capability pinned
# toolchain, а не выбор движка — см. docs/SATURN.md.
file(GENERATE OUTPUT "${RPE_SATURN_CUE}"
     CONTENT "FILE \"retro_saturn.iso\" BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n")

# Raw SH-2 binary: именно он кладётся на диск как 0.BIN (не ELF!).
add_custom_command(
    OUTPUT "${RPE_SATURN_RAW}"
    COMMAND "${CMAKE_OBJCOPY}" -O binary "${RPE_SATURN_ELF}" "${RPE_SATURN_RAW}"
    DEPENDS retro_saturn
    COMMENT "saturn: retro_saturn.bin (raw SH-2)"
    VERBATIM)

add_custom_command(
    OUTPUT "${RPE_SATURN_ISO}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${RPE_SATURN_RAW}" "${RPE_SATURN_DISC_DIR}/0.bin"
    COMMAND ${RPE_SATURN_MKISO_CMD}
    DEPENDS "${RPE_SATURN_RAW}" "${RPE_SATURN_IP}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "saturn: retro_saturn.iso (ISO9660 2048) + CUE"
    VERBATIM)

add_custom_target(saturn_disc ALL
    DEPENDS "${RPE_SATURN_ISO}" "${RPE_SATURN_CUE}" "${RPE_SATURN_RAW}")
add_custom_target(saturn DEPENDS retro_saturn saturn_disc)

# Артефакты для CI/локальной проверки:
#   build/saturn-release/src/hal/saturn/retro_saturn.elf  (SH-2 ELF)
#   build/saturn-release/src/hal/saturn/retro_saturn.bin  (raw program)
#   build/saturn-release/src/hal/saturn/retro_saturn.iso  (ISO9660, 2048-byte)
#   build/saturn-release/src/hal/saturn/retro_saturn.cue
#   build/saturn-release/src/hal/saturn/IP.BIN
#   build/saturn-release/src/hal/saturn/retro_saturn.map
