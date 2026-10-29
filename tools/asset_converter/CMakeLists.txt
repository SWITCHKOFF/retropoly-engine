# ============================================================================
# asset_converter — host-only инструмент (Спринты 9–10):
# glTF → .mesh v1, PNG → .tex v1. НИКОГДА не компилируется MIPS-тулчейном:
# PS1-сборка потребляет заранее сгенерированные assets/build/*.mesh/.tex.
# Зависимости (vendored): cgltf, stb (§3.6).
# ============================================================================

add_executable(asset_converter
    main.cpp
)
retro_engine_apply_flags(asset_converter)

# Вендоренные заголовки (cgltf — §3.6, src/third_party/cgltf) изолируются
# от канонических флагов: подключаются как SYSTEM-путь.
if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/cgltf/cgltf.h")
    target_include_directories(asset_converter SYSTEM PRIVATE
        "${CMAKE_SOURCE_DIR}/src/third_party/cgltf")
endif()
