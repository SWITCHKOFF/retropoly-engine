# ============================================================================
# HAL PS1 — PSn00bSDK v0.24 (pinned, §3.2), GTE + Ordering Table + CD-ROM.
# Реализация: Спринты 13–14 (docs/plans/sprint_13_14.md).
# Сборка только через пресет ps1-release + toolchain sdk.cmake (PSN00BSDK_LIBS).
#
# Конвейер (topology retro_engine, линия линковки без циклов):
#   retro_engine_log  — core/log (no-op sink на PS1: FILE-sink — PC-only);
#   retro_engine_hal  — PS1-бэкенд рендера + Platform_/Input_/FileSystem_
#                       (PSn00bSDK-заголовки включаются ТОЛЬКО в .cpp этого
#                       каталога; hal/* заголовки остаются платформенными);
#   retro_ps1         — PS-EXE (веха M4-port): общий слой игры
#                       examples/hello_triangle/scene.h + game/systems/*,
#                       lineкing ТОЛЬКО retro_engine (никогда напрямую SDK);
#   retro_disc        — mkpsxiso: SYSTEM.CNF + RETRO.EXE + ассеты
#                       (assets/build коммитятся как versioned-контент).
#
# Кодогенерация статических библиотек = NOGPREL (-G0 -mno-gpopt: без
# gp-относительного доступа — свойство ставится в src/CMakeLists.txt для
# retro_engine{,_hal,_log}), PS-EXE = GPREL (SDK-паттерн examples).
# ============================================================================

# SDK-тулчейн обязан быть инициализирован пресетом (toolchainFile sdk.cmake):
# без него import-таргеты psn00bsdk/psxgpu/... не существуют.
if(NOT TARGET psn00bsdk)
    message(FATAL_ERROR
        "HAL PS1 (Спринты 13–14) требует тулчейна PSn00bSDK (import-таргет "
        "psn00bsdk отсутствует). Сборка — только через пресет ps1-release: "
        "cmake --preset ps1-release (пресет задаёт toolchainFile "
        "$env{PSN00BSDK_LIBS}/cmake/sdk.cmake).")
endif()

# --- Библиотека HAL PS1 (retro_engine_hal) --------------------------------
add_library(retro_engine_hal STATIC
    gte_adapter.cpp       # GTE-адаптер (engine view-space → конвенция GTE)
    gpu_backend.cpp       # рендер-бэкенд (OT + packet-pool + VRAM-текстуры)
    platform_ps1.cpp      # Platform_* (GPU/GTE device + vsync-тайминг)
    input_pad.cpp         # Input_* (геймпад через драйвер BIOS)
    filesystem_cdrom.cpp  # FileSystem_* (ISO9660 через драйвер BIOS)
)
retro_engine_apply_flags(retro_engine_hal)

# Заголовки движка включаются от корня src/; examples/game — от корня
# репозитория (ретро-топология «каждый target видит СВОЙ слой»).
target_include_directories(retro_engine_hal PRIVATE "${CMAKE_SOURCE_DIR}/src")

# core/log — приватная зависимость HAL (диагностика бэкенда). psn00bsdk
# подключён toolchain'ом глобально (link_libraries) — вручную не линкуется.
target_link_libraries(retro_engine_hal PRIVATE retro_engine_log)

# --- PS-EXE вехи M4-port (retro_ps1; диcк-файл RETRO.EXE;1) ----------------
psn00bsdk_add_executable(retro_ps1 GPREL main_ps1.cpp)
retro_engine_apply_flags(retro_ps1)
target_include_directories(retro_ps1 PRIVATE "${CMAKE_SOURCE_DIR}")
target_link_libraries(retro_ps1 PRIVATE retro_engine)

# Ордер статических архивов: хелпер SDK добавил psx*/c ДО ретро-архивов, а в
# однопроходном mipsel-ld ссылки retro_engine_hal на SDK (DrawSync/ClearOTagR/
# ...) остаются неразрешёнными. Решение — ручная ld-группа (--start-group/
# --end-group, сканирование повторяется до замыкания ссылок) ПОВЕРХ позиций.
# Применяется генекс $<TARGET_FILE> (файловые пути, не таргеты): CMake не
# дедуплицирует их и не перестраивает, догоняя позицию в конце списка.
# (Генекс $<LINK_GROUP:RESCAN> отклоняется генератором: SDK-тулчейн ставит
# CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY — идентификация линкера не
# выполняется, feature-таблица недоступна.)
target_link_libraries(retro_ps1 PRIVATE
    "-Wl,--start-group"
    $<TARGET_FILE:retro_engine>
    $<TARGET_FILE:retro_engine_hal>
    $<TARGET_FILE:retro_engine_log>
    # Конкретные GPREL-архивы SDK (psx* — INTERFACE-обёртки без файла; выбор
    # варианта привязан к PSN00BSDK_TARGET_TYPE=GPREL самого retro_ps1).
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

# --- SYSTEM.CNF (сгенерированный шаблон; BOOT=cdrom:\\RETRO.EXE;1) ---------
set(RPE_PS1_BOOT       "cdrom:\\RETRO.EXE;1")
set(RPE_PS1_STACK      "801ffff0")
set(RPE_SYSTEM_CNF     "${CMAKE_CURRENT_BINARY_DIR}/system.cnf")
configure_file("${CMAKE_SOURCE_DIR}/cmake/ps1-system.cnf.in"
               "${RPE_SYSTEM_CNF}" @ONLY)

# --- CD-ROM образ (retro_disc → RETRO.bin/RETRO.cue) -----------------------
# Ассеты — коммитнутые артефакты Спринта 9–10 (asset_converter — host-only,
# хост-CI-джобу PC сборку проходит; PS1-джоба только читает committed-файлы).
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/assets/build/triangle.mesh" OR
   NOT EXISTS "${CMAKE_SOURCE_DIR}/assets/build/texture.tex")
    message(FATAL_ERROR
        "assets/build/triangle.mesh|.tex отсутствуют (versioned-ассеты "
        "Спринта 9–10 обязаны присутствовать в репозитории для PS1-диска).")
endif()

set(RPE_PS1_EXE "${CMAKE_CURRENT_BINARY_DIR}/retro_ps1.exe")
set(RPE_DISC_MESH "${CMAKE_SOURCE_DIR}/assets/build/triangle.mesh")
set(RPE_DISC_TEX "${CMAKE_SOURCE_DIR}/assets/build/texture.tex")
set(RPE_DISC_VOLUME "RETROENG")
psn00bsdk_add_cd_image(retro_disc RETRO "${CMAKE_SOURCE_DIR}/cmake/ps1-disc.xml"
    DEPENDS retro_ps1 "${RPE_SYSTEM_CNF}" "${RPE_DISC_MESH}" "${RPE_DISC_TEX}")
add_dependencies(retro_disc retro_ps1)

# Запуск в эмуляторе (локально; CI только сборка):
#   mkd -s или DuckStation: RETRO.bin/RETRO.cue из build/ps1-release/src/hal/ps1.
