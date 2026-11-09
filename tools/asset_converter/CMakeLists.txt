# ============================================================================
# asset_converter — host-only инструмент (Спринты 9–10):
# glTF → .mesh v1, PNG → .tex v1. НИКОГДА не компилируется MIPS-тулчейном:
# PS1-сборка потребляет заранее сгенерированные assets/build/*.mesh/.tex.
# Зависимости (vendored §3.6): cgltf (+ stb_image в texture_converter).
# ============================================================================

add_executable(asset_converter
    main.cpp
    mesh_converter.cpp
    texture_converter.cpp
    color_quantizer.cpp
)
target_compile_features(asset_converter PRIVATE cxx_std_14)
retro_engine_apply_flags(asset_converter)

# Вендоренные заголовки (cgltf/stb) изолируются от канонических флагов
# (-Werror не нагружает third-party): SYSTEM-пути. Опциональность — trait
# Спринта 0: пустой submodule не должен ронять configure PC (гейт уже
# роняет build — vendored-verify), но диагностика здесь явная.
if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/cgltf/cgltf.h")
    target_include_directories(asset_converter SYSTEM PRIVATE
        "${CMAKE_SOURCE_DIR}/src/third_party/cgltf")
else()
    message(FATAL_ERROR
        "src/third_party/cgltf пуст (submodule не инициализирован). "
        "Запустите 'git submodule update --init --recursive'.")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/src/third_party/stb/stb_image.h")
    target_include_directories(asset_converter SYSTEM PRIVATE
        "${CMAKE_SOURCE_DIR}/src/third_party/stb")
else()
    message(FATAL_ERROR
        "src/third_party/stb пуст (submodule не инициализирован). "
        "Запустите 'git submodule update --init --recursive'.")
endif()

# Классы форматов/LE-хелперы/структуры — общий источник истины src/assets.
target_include_directories(asset_converter PRIVATE "${CMAKE_SOURCE_DIR}/src")
