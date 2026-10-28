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

# Вендоренные заголовки конвертера изолируются от канонических флагов.
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/asset_converter/third_party/cgltf.h")
    target_include_directories(asset_converter PRIVATE
        "${CMAKE_SOURCE_DIR}/tools/asset_converter/third_party")
endif()
