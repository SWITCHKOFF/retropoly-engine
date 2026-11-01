# ============================================================================
# cmake/EmbedShaders.cmake — генерация заголовка со шейдерными источниками.
#
# Канонические файлы шейдеров: src/render/shaders/*.vert|.frag (артефакт
# спринта 7–8). Backend (render_opengl.cpp) компилирует их из ВСТРОЕННЫХ
# строк (content-only embed — компиляторам не нужны файлы шейдеров в
# runtime). Выполняется в configure-фазе src/CMakeLists.txt.
#
# Кодирование: ручное C-escaping слоями string(REPLACE) — сначала бэкслеш,
# затем кавычка, затем переходы строк/табуляция (порядок слоёв критичен,
# иначе двойное экранирование); string(JSON QUOTE) не используется
# (отсутствует в данном билд-окружении).
#
# Выход: ${CMAKE_BINARY_DIR}/generated/render/shaders_embedded.h — static
# const char* RPE_PS1_EMU_VERT_SRC / RPE_PS1_EMU_FRAG_SRC /
# RPE_PS1_PRESENT_VERT_SRC / RPE_PS1_PRESENT_FRAG_SRC (имена — контракт
# render_opengl.cpp). Гард: перегенерация только при изменении SHA256
# исходников (быстрая инкрементальная конфигурация).
# ============================================================================

# Абсолютные пути шейдеров (артефакты спринта) и выходного заголовка.
set(RPE_SHADER_DIR "${CMAKE_SOURCE_DIR}/src/render/shaders")
set(RPE_SHADER_OUT "${CMAKE_BINARY_DIR}/generated/render/shaders_embedded.h")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/render")

# Встраивание ОДНОГО шейдера в выходной заголовок (join — file(APPEND)).
#
#   var_name  — имя C-переменной (контракт бэкенда, см. шапку).
#   file_name — исходный файл в src/render/shaders/.
function(rpe_embed_shader var_name file_name)
    set(rpe_path "${RPE_SHADER_DIR}/${file_name}")
    file(READ "${rpe_path}" rpe_src)
    string(REPLACE "\\" "\\\\" rpe_src "${rpe_src}")
    string(REPLACE "\"" "\\\"" rpe_src "${rpe_src}")
    string(REPLACE "\n" "\\n" rpe_quoted "${rpe_src}")
    string(REPLACE "\t" "\\t" rpe_quoted "${rpe_quoted}")
    file(APPEND "${RPE_SHADER_OUT}"
        "static const char* ${var_name} = \"${rpe_quoted}\";\n")
endfunction()

# Контроль содержимого: цепочка SHA256 исходников шейдеров (полезно для
# диагностики configure-лога).
set(RPE_SHA_IN "")
foreach(rpe_shader ps1_emu.vert ps1_emu.frag ps1_present.vert ps1_present.frag)
    file(SHA256 "${RPE_SHADER_DIR}/${rpe_shader}" rpe_sha)
    string(APPEND RPE_SHA_IN "${rpe_sha};")
endforeach()
set(rpe_meta_out "${CMAKE_BINARY_DIR}/generated/render/shaders_embedded.sha")

# Шейдеры — зависимости КОНФИГУРАЦИИ: правка исходника шейдера заставляет
# ninja перезапустить cmake (перегенерация заголовка) перед пересборкой
# рендер-бэкенда (иначе emit-стейдж строит бэкенд по устаревшему embed).
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${RPE_SHADER_DIR}/ps1_emu.vert"
    "${RPE_SHADER_DIR}/ps1_emu.frag"
    "${RPE_SHADER_DIR}/ps1_present.vert"
    "${RPE_SHADER_DIR}/ps1_present.frag")

# Перегенерация — на КАЖДОЙ конфигурации (заголовок ~20 KiB, стоимость
# пренебрежима; SHA-гард раним к прерванным записям: разманенный header
# при «валидном» .sha блокировал бы сборку до ручной чистки build/).
file(WRITE "${RPE_SHADER_OUT}"
"// SPDX-License-Identifier: MIT
// RetroPolyEngine — generated/render/shaders_embedded.h
//
// GENERATED FILE (cmake/EmbedShaders.cmake) — НЕ редактировать руками.
// Канонические источники — src/render/shaders/*.vert|.frag; изменение
// шейдера требует перегенерации (переконфигурация CMake).
#pragma once
")

rpe_embed_shader(RPE_PS1_EMU_VERT_SRC "ps1_emu.vert")
rpe_embed_shader(RPE_PS1_EMU_FRAG_SRC "ps1_emu.frag")
rpe_embed_shader(RPE_PS1_PRESENT_VERT_SRC "ps1_present.vert")
rpe_embed_shader(RPE_PS1_PRESENT_FRAG_SRC "ps1_present.frag")

file(WRITE "${rpe_meta_out}" "${RPE_SHA_IN}")

# Экспорт в родительскую область: путь к generated для target_include.
set(RPE_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated" PARENT_SCOPE)
