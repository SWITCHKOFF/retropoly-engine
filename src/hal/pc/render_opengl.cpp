// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/pc/render_opengl.cpp
//
// HAL PC: OpenGL 3.3 backend рендера (Спринт 7–8, недели 13–16).
//
// Нормативные источники:
//   - §1.2.1 manifest: reference_resolution 320×240; order
//     [TRANSFORM, CLIP, CULL, SORT, PACK, SUBMIT]; depth_test
//     disabled_on_pc_reference; silent_primitive_drop forbidden;
//   - §4 (док): «Фреймбуфер 320x240 — рендеринг в текстуру + nearest вывод»;
//   - docs/plans/sprint_07_08.md, недели 13–16.
//
// Состав backend'а:
//   - reference FBO 320×240 (RGBA8, БЕЗ depth-аттачмента; Z-тест
//     glDisable(GL_DEPTH_TEST) — неделя 13);
//   - программа эмуляции PS1 (ps1_emu.vert/.frag: CI8 + Gouraud + affine
//     + 1/16-px quantize) и программа выкладки (ps1_present, nearest);
//   - динамический VBO подачи (per-triangle) + статический quad выкладки;
//   - hal_pc_swap_buffers (неделя 9) — показ окна (разделение владений).
//
// Диагностика GL — через core/log (STYLE_GUIDE §3) и bool-статусы
// hal_render_backend_* (никаких silent drop; неделя 16).
#include "hal/pc/hal_pc_internal.h"

#include "core/log/log.h"
#include "hal/render_backend.h"
#include "render/texture.h" // RENDER_TEXTURE_MAX/_MAX_DIM (лимиты пула ядра).
#include "render/shaders_embedded.h"

#include <SDL3/SDL.h>
#include <glad/gl.h>

#include <string.h>

// ---------------------------------------------------------------------------
// Состояние backend'а
// ---------------------------------------------------------------------------

/// Инициализирован ли backend (init/shutdown машина).
static bool s_backend_inited = false;

/// Размер reference-фреймбуфера (0 до init).
static uint32_t s_fb_w = 0u;
static uint32_t s_fb_h = 0u;

/** GPU-представление ядровой CI8-текстуры: пара текстур GL. */
typedef struct GpuTexture
{
    GLuint index_texture;   ///< R8 (w×h): индексы палитры (CI8).
    GLuint palette_texture; ///< RGB8 (256×1): честный RGB555+STP unpack.
} GpuTexture;

/// GL-объекты backend'а (0 — не создан).
static GLuint s_emu_program = 0;     ///< ps1_emu (triangle pass).
static GLuint s_present_program = 0; ///< ps1_present (blit pass).
static GLuint s_fbo = 0;             ///< Reference framebuffer 320×240.
static GLuint s_fbo_texture = 0;     ///< RGBA8-текстура FBO (nearest).
static GLuint s_draw_vbo = 0;        ///< Динамический буфер подачи.
static GLuint s_draw_vao = 0;        ///< Vertex-array подачи.
static GLuint s_present_vbo = 0;     ///< Статический quad выкладки.
static GLuint s_present_vao = 0;     ///< Vertex-array выкладки.

/// GPU-клоны ядровых текстур (index slot = handle − 1).
static GpuTexture s_gpu_textures[RENDER_TEXTURE_MAX];

/// Uniform-локации (заполняются после линковки; −1 — uniform отсутствует).
static GLint s_emu_u_mvp = -1;
static GLint s_emu_u_texture_index = -1;
static GLint s_emu_u_palette = -1;
static GLint s_present_u_framebuffer = -1;

/** Число float на вершину подачи: pos(3) + uv(2) + rgba(4). */
#define BACKEND_VERTEX_FLOATS (3u + 2u + 4u)

// ---------------------------------------------------------------------------
// GL-хелперы
// ---------------------------------------------------------------------------

/** Сброс накопленных GL-ошибок (yield последнего кода). */
static GLenum backend_gl_sweep(void)
{
    GLenum error = glGetError();
    while (error != GL_NO_ERROR) {
        error = glGetError();
    }
    return error;
}

/**
 * Проверка одной GL-операции: логирует отказ через Logger (STyle §3).
 *
 * @param stage Метка ступени для строки журнала.
 * @return true — ошибок нет; false — GL-ошибка зафиксирована.
 */
static bool backend_gl_check(const char* stage)
{
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        Log_Message(LOG_LEVEL_ERROR, "render",
                    "GL error 0x%x on %s", (unsigned)error, stage);
        return false;
    }
    return true;
}

/**
 * Ступень компиляции/линковки шейдера с журналом ошибок через Logger
 * (неделя 14: «компилятор/линковка шейдера с логированием через Logger»).
 *
 * @param stage " compile" или "link" (для журнала).
 * @param name Имя артефакта (ps1_emu и т.п.).
 * @param type GL_VERTEX_SHADER/GL_FRAGMENT_SHADER (link — игнорируется).
 * @param source Исходник шейдера (компиляция; NULL при линковке).
 * @param attach Двухэлементный массив шейдеров (линковка).
 * @return GL-объект (shader или program) или 0 (отказ с журналом).
 */
static GLuint backend_shader_stage(const char* stage, const char* name,
                                   GLenum type, const char* source,
                                   const GLuint attach[2])
{
    const bool is_compile = (source != NULL);
    GLuint object = 0;
    if (is_compile) {
        object = glCreateShader(type);
        if (object == 0) {
            Log_Message(LOG_LEVEL_ERROR, "render",
                        "glCreateShader failed (%s %s)", stage, name);
            return 0;
        }
        glShaderSource(object, 1, &source, NULL);
        glCompileShader(object);
        GLint ok = 0;
        glGetShaderiv(object, GL_COMPILE_STATUS, &ok);
        if (ok != GL_TRUE) {
            char log[512];
            GLsizei log_len = 0;
            glGetShaderInfoLog(object, (GLsizei)sizeof(log), &log_len, log);
            Log_Message(LOG_LEVEL_ERROR, "render",
                        "shader %s compile failed: %.*s", name,
                        (int)log_len, log);
            glDeleteShader(object);
            return 0;
        }
    } else {
        object = glCreateProgram();
        if (object == 0) {
            Log_Message(LOG_LEVEL_ERROR, "render",
                        "glCreateProgram failed (%s)", name);
            return 0;
        }
        glAttachShader(object, attach[0]);
        glAttachShader(object, attach[1]);
        glLinkProgram(object);
        GLint ok = 0;
        glGetProgramiv(object, GL_LINK_STATUS, &ok);
        if (ok != GL_TRUE) {
            char log[512];
            GLsizei log_len = 0;
            glGetProgramInfoLog(object, (GLsizei)sizeof(log), &log_len, log);
            Log_Message(LOG_LEVEL_ERROR, "render",
                        "program %s link failed: %.*s", name,
                        (int)log_len, log);
            glDeleteProgram(object);
            return 0;
        }
    }
    Log_Message(LOG_LEVEL_INFO, "render", "shader %s %s: OK", stage, name);
    return object;
}

/**
 * Сборка shader-программы из встроенных строк (генерация EmbedShaders).
 *
 * @param name Отладочное имя программы (для журнала).
 * @param vert_src Источник вершинного шейдера.
 * @param frag_src Источник пиксельного шейдера.
 * @return GL-программа или 0 (отказ компиляции/линковки).
 */
static GLuint backend_build_program(const char* name, const char* vert_src,
                                    const char* frag_src)
{
    backend_gl_sweep();
    const GLuint vert = backend_shader_stage(
        "compile", name, GL_VERTEX_SHADER, vert_src, NULL);
    const GLuint frag = backend_shader_stage(
        "compile", name, GL_FRAGMENT_SHADER, frag_src, NULL);
    if (vert == 0 || frag == 0) {
        if (vert != 0) {
            glDeleteShader(vert);
        }
        if (frag != 0) {
            glDeleteShader(frag);
        }
        return 0;
    }
    const GLuint attach[2] = {vert, frag};
    const GLuint program =
        backend_shader_stage("link", name, 0, NULL, attach);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (program == 0) {
        return 0;
    }
    if (!backend_gl_check("build_program")) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

/**
 * Загрузка 2D-текстуры GL (nearest, clamp-to-edge).
 *
 * @param w Ширина (пикселей).
 * @param h Высота (пикселей).
 * @param internal_format Внутренний формат (GL_R8 / GL_RGB8 / GL_RGBA8).
 * @param format Лейаут данных (GL_RED / GL_RGB / GL_RGBA).
 * @param data Данные (w×h×компоненты); NULL — без данных (FBO).
 * @return GL-текстура или 0 (отказ).
 */
static GLuint backend_upload_texture(uint32_t w, uint32_t h,
                                     GLint internal_format, GLenum format,
                                     const void* data)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, (GLsizei)w, (GLsizei)h, 0,
                 format, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

// ---------------------------------------------------------------------------
// Lifecycle backend'а
// ---------------------------------------------------------------------------

bool hal_render_backend_init(uint32_t width, uint32_t height)
{
    if (s_backend_inited) {
        return true; // Идемпотентный Init.
    }

    SDL_Window* window = hal_pc_window();
    if (window == NULL) {
        // Окно/GL-контекст отсутствуют (Platform_Init обязан их создать).
        Log_Message(LOG_LEVEL_ERROR, "render",
                    "backend_init: окно/GL-контекст отсутствуют");
        return false;
    }

    s_fb_w = width;
    s_fb_h = height;
    memset(s_gpu_textures, 0, sizeof(s_gpu_textures));

    // --- Глобальные состояния (normative: depth test disabled) -----------
    glDisable(GL_DEPTH_TEST); // Норматив §1: reference PC renderer — без Z.
    glDisable(GL_BLEND);      // Прозрачность MVP выключена («transparency off»).
    glDisable(GL_CULL_FACE);  // Backface-cull — зона ядра (PS1-семантика).

    // --- Шейдерные программы (компиляция/линковка через Logger) ----------
    s_emu_program = backend_build_program("ps1_emu", RPE_PS1_EMU_VERT_SRC,
                                          RPE_PS1_EMU_FRAG_SRC);
    if (s_emu_program == 0) {
        return false;
    }
    s_present_program =
        backend_build_program("ps1_present", RPE_PS1_PRESENT_VERT_SRC,
                              RPE_PS1_PRESENT_FRAG_SRC);
    if (s_present_program == 0) {
        glDeleteProgram(s_emu_program);
        s_emu_program = 0;
        return false;
    }

    // Числовые unit-привязки фиксированы (GL usage).
    glUseProgram(s_emu_program);
    s_emu_u_mvp = glGetUniformLocation(s_emu_program, "u_mvp");
    s_emu_u_texture_index =
        glGetUniformLocation(s_emu_program, "u_texture_index");
    s_emu_u_palette = glGetUniformLocation(s_emu_program, "u_palette");
    glUniform1i(s_emu_u_texture_index, 0); // unit 0 = CI8-индексы.
    glUniform1i(s_emu_u_palette, 1);       // unit 1 = палитра 256×1.

    glUseProgram(s_present_program);
    s_present_u_framebuffer =
        glGetUniformLocation(s_present_program, "u_framebuffer");
    glUniform1i(s_present_u_framebuffer, 0); // unit 0 = FBO-текстура.

    // --- Reference FBO 320×240 (RGBA8, БЕЗ depth) ------------------------
    s_fbo_texture = backend_upload_texture(width, height, GL_RGBA8, GL_RGBA,
                                          NULL);
    if (s_fbo_texture == 0) {
        return false;
    }
    glGenFramebuffers(1, &s_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, s_fbo_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log_Message(LOG_LEVEL_ERROR, "render",
                    "reference FBO incomplete (0x%x)",
                    (unsigned)glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return false;
    }

    // --- VAO+VBO подачи (динамический, per-triangle, 3 вершины) ----------
    glGenVertexArrays(1, &s_draw_vao);
    glBindVertexArray(s_draw_vao);
    glGenBuffers(1, &s_draw_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_draw_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(3u * BACKEND_VERTEX_FLOATS * sizeof(float)),
                 NULL, GL_DYNAMIC_DRAW);

    const GLsizei stride = (GLsizei)(BACKEND_VERTEX_FLOATS * sizeof(float));
    glEnableVertexAttribArray(0); // a_position vec3
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1); // a_texcoord vec2
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          (void*)(uintptr_t)(3u * sizeof(float)));
    glEnableVertexAttribArray(2); // a_color vec4
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                          (void*)(uintptr_t)(5u * sizeof(float)));

    // --- VAO+VBO выкладки (fullscreen quad, 6 вершин, pos+uv) ------------
    static const float quad[36] = {
        // x, y (NDC) | u, v (FBO UV)
        -1.0f, -1.0f, 0.0f, 0.0f,
        +1.0f, -1.0f, 1.0f, 0.0f,
        +1.0f, +1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
        +1.0f, +1.0f, 1.0f, 1.0f,
        -1.0f, +1.0f, 0.0f, 1.0f,
    };
    glGenVertexArrays(1, &s_present_vao);
    glBindVertexArray(s_present_vao);
    glGenBuffers(1, &s_present_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_present_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // a_position vec2
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1); // a_texcoord vec2
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(uintptr_t)(2u * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    if (!backend_gl_check("init")) {
        // Частичная неудача — полный откат состояния backend'а.
        hal_render_backend_shutdown();
        s_backend_inited = false;
        return false;
    }

    s_backend_inited = true;
    Log_Message(LOG_LEVEL_INFO, "render",
                "backend PC GL init: FBO %ux%u, depth off, nearest",
                (unsigned)width, (unsigned)height);
    return true;
}

void hal_render_backend_shutdown(void)
{
    if (!s_backend_inited) {
        return; // Идемпотентность; безопасно без Init.
    }
    if (hal_pc_window() == NULL) {
        // Platform_Shutdown уже разрушил context — GL-вызовы невозможны;
        // состояние backend'а считается собранным (следование Lifecycle:
        // Render_Shutdown обязан идти ПЕРЕД Platform_Shutdown — см. README).
        s_backend_inited = false;
        s_fb_w = 0u;
        s_fb_h = 0u;
        return;
    }

    for (uint32_t i = 0u; i < RENDER_TEXTURE_MAX; ++i) {
        // GPU-текстура ПАРА: индексы + палитра (delete двух имён).
        if (s_gpu_textures[i].index_texture != 0) {
            glDeleteTextures(1, &s_gpu_textures[i].index_texture);
            s_gpu_textures[i].index_texture = 0;
        }
        if (s_gpu_textures[i].palette_texture != 0) {
            glDeleteTextures(1, &s_gpu_textures[i].palette_texture);
            s_gpu_textures[i].palette_texture = 0;
        }
    }
    if (s_present_program != 0) {
        glDeleteProgram(s_present_program);
        s_present_program = 0;
    }
    if (s_emu_program != 0) {
        glDeleteProgram(s_emu_program);
        s_emu_program = 0;
    }
    if (s_present_vbo != 0) {
        glDeleteBuffers(1, &s_present_vbo);
        s_present_vbo = 0;
    }
    if (s_present_vao != 0) {
        glDeleteVertexArrays(1, &s_present_vao);
        s_present_vao = 0;
    }
    if (s_draw_vbo != 0) {
        glDeleteBuffers(1, &s_draw_vbo);
        s_draw_vbo = 0;
    }
    if (s_draw_vao != 0) {
        glDeleteVertexArrays(1, &s_draw_vao);
        s_draw_vao = 0;
    }
    if (s_fbo != 0) {
        glDeleteFramebuffers(1, &s_fbo);
        s_fbo = 0;
    }
    if (s_fbo_texture != 0) {
        glDeleteTextures(1, &s_fbo_texture);
        s_fbo_texture = 0;
    }
    s_backend_inited = false;
    s_fb_w = 0u;
    s_fb_h = 0u;
    Log_Message(LOG_LEVEL_INFO, "render", "backend PC GL shutdown: OK");
}

// ---------------------------------------------------------------------------
// Кадр: начало / очистка / текстуры / подача / выкладка / readback
// ---------------------------------------------------------------------------

void hal_render_backend_begin_frame(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_backend_inited) {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glViewport(0, 0, (GLsizei)s_fb_w, (GLsizei)s_fb_h);
    glClearColor((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f,
                 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    (void)backend_gl_check("begin_frame");
}

void hal_render_backend_clear(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_backend_inited) {
        return;
    }
    // Немедленная очистка ТЕКУЩЕГО назначения (FBO во время кадра; без
    // активного кадра — окно; назначение выбирается state'ом рендера;
    // Render_Clear между кадрами двигает clear state, кадр — переназначает).
    glClearColor((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f,
                 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    (void)backend_gl_check("clear");
}

bool hal_render_backend_create_texture(TextureHandle handle, uint32_t w,
                                       uint32_t h, const uint8_t* pixels_ci8,
                                       const uint16_t* palette)
{
    if (!s_backend_inited || handle == RENDER_TEXTURE_INVALID
        || handle > RENDER_TEXTURE_MAX) {
        return false;
    }
    const uint32_t slot = (uint32_t)handle - 1u;
    if (s_gpu_textures[slot].index_texture != 0
        || s_gpu_textures[slot].palette_texture != 0) {
        return false; // GPU-клон уже существует (повторное создание — отказ).
    }
    // Валидация данных ядра (malformed input reject before dereference;
    // пиксели CI8 ограничены пулом ядра 256×256 — RENDER_TEXTURE_MAX_DIM).
    if (pixels_ci8 == NULL || palette == NULL || w == 0u || h == 0u
        || w > RENDER_TEXTURE_MAX_DIM || h > RENDER_TEXTURE_MAX_DIM) {
        return false;
    }

    backend_gl_sweep();
    // CI8-индексы: GL_R8 (0..255 — точный; R8 core 3.0+).
    const GLuint index_texture = backend_upload_texture(
        w, h, GL_R8, GL_RED, pixels_ci8);
    if (index_texture == 0) {
        Log_Message(LOG_LEVEL_ERROR, "render",
                    "texture upload failed (indexes)");
        return false;
    }

    // Палитра 256×1 RGB8: честный unpack PS1 (5→8 бит, §1.2.5).
    uint8_t palette_rgb[256 * 3];
    for (uint32_t i = 0u; i < 256u; ++i) {
        unpack_rgb555_stp(palette[i], &palette_rgb[i * 3u + 0u],
                          &palette_rgb[i * 3u + 1u],
                          &palette_rgb[i * 3u + 2u]);
    }
    const GLuint palette_texture =
        backend_upload_texture(256u, 1u, GL_RGB8, GL_RGB, palette_rgb);
    if (palette_texture == 0) {
        glDeleteTextures(1, &index_texture);
        Log_Message(LOG_LEVEL_ERROR, "render",
                    "texture upload failed (palette)");
        return false;
    }

    s_gpu_textures[slot].index_texture = index_texture;
    s_gpu_textures[slot].palette_texture = palette_texture;
    backend_gl_check("create_texture"); // Диагностика мягких ошибок.
    return true;
}

void hal_render_backend_destroy_texture(TextureHandle handle)
{
    if (!s_backend_inited || handle == RENDER_TEXTURE_INVALID
        || handle > RENDER_TEXTURE_MAX) {
        return; // Безопасно.
    }
    const uint32_t slot = (uint32_t)handle - 1u;
    if (s_gpu_textures[slot].index_texture != 0) {
        glDeleteTextures(1, &s_gpu_textures[slot].index_texture);
        s_gpu_textures[slot].index_texture = 0;
    }
    if (s_gpu_textures[slot].palette_texture != 0) {
        glDeleteTextures(1, &s_gpu_textures[slot].palette_texture);
        s_gpu_textures[slot].palette_texture = 0;
    }
}

bool hal_render_backend_draw_triangle(
    const HalRenderBackendVertex vertices[3], const float* mvp_gl,
    TextureHandle texture)
{
    if (!s_backend_inited || vertices == NULL || mvp_gl == NULL
        || texture == RENDER_TEXTURE_INVALID
        || texture > RENDER_TEXTURE_MAX) {
        return false;
    }
    const uint32_t slot = (uint32_t)texture - 1u;
    if (s_gpu_textures[slot].index_texture == 0
        || s_gpu_textures[slot].palette_texture == 0) {
        return false; // Текстура не создана/gl-клон отсутствует.
    }

    backend_gl_sweep();
    glUseProgram(s_emu_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_gpu_textures[slot].index_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_gpu_textures[slot].palette_texture);
    glUniformMatrix4fv(s_emu_u_mvp, 1, GL_FALSE, mvp_gl);

    // Конверсия вершины в interleaved float-строку (pos3 uv2 rgba4).
    float packed[9 * 3];
    for (uint32_t v = 0u; v < 3u; ++v) {
        packed[v * 9u + 0u] = vertices[v].x;
        packed[v * 9u + 1u] = vertices[v].y;
        packed[v * 9u + 2u] = vertices[v].z;
        packed[v * 9u + 3u] = vertices[v].u;
        packed[v * 9u + 4u] = vertices[v].v;
        packed[v * 9u + 5u] = vertices[v].r;
        packed[v * 9u + 6u] = vertices[v].g;
        packed[v * 9u + 7u] = vertices[v].b;
        packed[v * 9u + 8u] = vertices[v].a;
    }
    glBindVertexArray(s_draw_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_draw_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(packed), packed, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (glGetError() != GL_NO_ERROR) {
        // Явная ошибка per-draw failure (никакой silent drop).
        Log_Message(LOG_LEVEL_ERROR, "render", "draw_triangle GL failure");
        return false;
    }
    return true;
}

void hal_render_backend_end_frame(void)
{
    if (!s_backend_inited) {
        return;
    }
    backend_gl_sweep();
    // Выкладка: default framebuffer, размер окна (nearest от FBO-текстуры).
    SDL_Window* window = hal_pc_window();
    int window_w = (int)s_fb_w;
    int window_h = (int)s_fb_h;
    if (window != NULL) {
        // SDL3: SDL_GL_GetDrawableSize удалён — SDL_GetWindowSizeInPixels.
        SDL_GetWindowSizeInPixels(window, &window_w, &window_h);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_w, window_h);

    glUseProgram(s_present_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_fbo_texture);
    glBindVertexArray(s_present_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Возврат к нейтральному состоянию (следующий кадр добавляет всё).
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    (void)backend_gl_check("end_frame");
}

bool hal_render_backend_read_pixels(uint8_t* dst_rgba8, uint32_t dst_size)
{
    if (!s_backend_inited || dst_rgba8 == NULL) {
        return false;
    }
    const uint32_t required = s_fb_w * s_fb_h * 4u;
    if (dst_size < required) {
        return false;
    }
    backend_gl_sweep();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo);
    glReadPixels(0, 0, (GLsizei)s_fb_w, (GLsizei)s_fb_h, GL_RGBA,
                 GL_UNSIGNED_BYTE, dst_rgba8);
    const bool ok = backend_gl_check("read_pixels");
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return ok;
}

// ---------------------------------------------------------------------------
// Показ окна (поддержание week 9-контракта — swap window)
// ---------------------------------------------------------------------------

bool hal_pc_swap_buffers(void)
{
    SDL_Window* window = hal_pc_window();
    if (window == NULL) {
        return false;
    }
    SDL_GL_SwapWindow(window);
    return true;
}
