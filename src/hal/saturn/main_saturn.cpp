// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/main_saturn.cpp
//
// Sega Saturn boot (Спринт 23–24, веха v0.2.0).
//
// Jo Engine вызывает jo_main() из собственного main() после инициализации
// стартового кода (start.S). Последовательность:
//   Platform_Init (core + VDP2 + память + pad + ISO9660)
//   → Render_Init (320×224, backdrop)
//   → boot self-test: CUBE.MSH + TEX0.TEX с CD, валидация загрузчиками ядра
//   → jo_core_run() с per-frame callback (pump → Begin/Clear/End → swap).
//
// Символ jo_main имеет C-linkage: его вызывает C-ядро Jo Engine.
#include "hal/platform.h"

#include "assets/assets.h"
#include "core/log/log.h"
#include "core/math/mat4.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/saturn_memory.h"
#include "hal/saturn/saturn_parallel_workload.h"
#include "hal/saturn/slave_worker.h"
#include "hal/saturn/vdp1_backend.h"
#include "hal/saturn/vdp1_texture.h"
#include "hal/saturn/vdp2_backend.h"

#include <string.h>

namespace {

constexpr uint32_t kSelfTestScratchBytes = 160u * 1024u;
constexpr uint32_t kMeshArenaBytes = 64u * 1024u;
constexpr uint32_t kMetricsPeriodFrames = 300u;
constexpr uint32_t kDemoMaxVertices = 64u;

LinearAllocator s_mesh_alloc;
void* s_mesh_block = nullptr;
MeshAsset* s_boot_mesh = nullptr;
Mat4 s_view;
Mat4 s_proj;
Mat4 s_mvp;
Mat4 s_cube_model;
TextureHandle s_texture = RENDER_TEXTURE_INVALID;
Vertex s_demo_vertices[kDemoMaxVertices];
uint32_t s_demo_vertex_count = 0u;
fixed_t s_bg_scroll_x = 0;
fixed_t s_bg_scroll_y = 0;
fixed_t s_object_yaw = 0;
fixed_t s_object_pitch = 0;
fixed_t s_camera_yaw = 0;
fixed_t s_camera_distance = 0;
fixed_t s_camera_height = 0;
uint32_t s_frame_count = 0u;
uint32_t s_first_hash = 0u;
uint32_t s_determinism_failures = 0u;
saturn::ParallelBenchResult s_bench = {};
#if RPE_SATURN_FAULT_FLOOD
uint32_t s_flood_overflows = 0u;
#endif

uint32_t fnv1a32(const uint8_t* data, size_t size)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

void log_asset_file(const char* name, const FileData& file)
{
    Log_Message(LOG_LEVEL_INFO, "saturn_asset", "%s: %u bytes, fnv1a=%x", name,
                static_cast<unsigned>(file.size),
                static_cast<unsigned>(fnv1a32(file.data, file.size)));
}

void boot_asset_self_test(void)
{
    bool mesh_ok = false;
    bool texture_ok = false;

    // CUBE.MSH остаётся в persistent-арене: workload S25–26 использует
    // реальные вершины/индексы с CD, а не синтетическую геометрию.
    s_mesh_block = saturn::asset_heap().allocate(kMeshArenaBytes, 8u);
    if (s_mesh_block != nullptr) {
        s_mesh_alloc.init(s_mesh_block, kMeshArenaBytes);
        FileData mesh_file = FileSystem_LoadFile("CUBE.MSH");
        if (mesh_file.data != nullptr) {
            log_asset_file("CUBE.MSH", mesh_file);
            MeshAsset* mesh = nullptr;
            uint32_t error = ASSET_ERR_OK;
            if (mesh_validate_and_load(mesh_file.data, mesh_file.size, &s_mesh_alloc, &mesh,
                                       &error)) {
                mesh_ok = true;
                s_boot_mesh = mesh;
                Log_Message(LOG_LEVEL_INFO, "saturn_asset",
                            "CUBE.MSH valid: %u vertices, %u indices",
                            static_cast<unsigned>(mesh->vertex_count),
                            static_cast<unsigned>(mesh->index_count));
            } else {
                Log_Message(LOG_LEVEL_ERROR, "saturn_asset", "CUBE.MSH invalid (asset error %u)",
                            static_cast<unsigned>(error));
            }
            FileSystem_FreeFile(&mesh_file);
        } else {
            Log_Message(LOG_LEVEL_WARNING, "saturn_asset", "CUBE.MSH not loaded from CD");
        }
        if (!mesh_ok) {
            saturn::asset_heap().free(s_mesh_block);
            s_mesh_block = nullptr;
        }
    } else {
        Log_Message(LOG_LEVEL_ERROR, "saturn_asset", "mesh arena alloc failed (%s)",
                    saturn::AssetHeap::diag_string(saturn::asset_heap().diag()));
    }

    void* scratch = saturn::asset_heap().allocate(kSelfTestScratchBytes, 8u);
    if (scratch == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_asset", "CD self-test: scratch alloc failed (%s)",
                    saturn::AssetHeap::diag_string(saturn::asset_heap().diag()));
    } else {
        LinearAllocator alloc(scratch, kSelfTestScratchBytes);

        FileData tex_file = FileSystem_LoadFile("TEX0.TEX");
        if (tex_file.data != nullptr) {
            log_asset_file("TEX0.TEX", tex_file);
            TextureAsset* texture = nullptr;
            uint32_t error = ASSET_ERR_OK;
            if (texture_validate_and_load(tex_file.data, tex_file.size, &alloc, &texture, &error)) {
                texture_ok = true;
                Log_Message(LOG_LEVEL_INFO, "saturn_asset", "TEX0.TEX valid: %ux%u",
                            static_cast<unsigned>(texture->width),
                            static_cast<unsigned>(texture->height));
                s_texture = Render_CreateTexture(texture->width, texture->height, texture->pixels,
                                                 texture->palette);
                if (s_texture == RENDER_TEXTURE_INVALID) {
                    Log_Message(LOG_LEVEL_ERROR, "saturn_asset", "TEX0.TEX upload to VDP1 failed");
                }
            } else {
                Log_Message(LOG_LEVEL_ERROR, "saturn_asset", "TEX0.TEX invalid (asset error %u)",
                            static_cast<unsigned>(error));
            }
            FileSystem_FreeFile(&tex_file);
        } else {
            Log_Message(LOG_LEVEL_WARNING, "saturn_asset", "TEX0.TEX not loaded from CD");
        }

        // S29–30: фоновая текстура 256×256 → VDP2 NBG1 bitmap.
        if (saturn::vdp2_backend_available()) {
            FileData bg_file = FileSystem_LoadFile("TEX1.TEX");
            if (bg_file.data != nullptr) {
                TextureAsset* bg = nullptr;
                uint32_t bg_error = ASSET_ERR_OK;
                if (texture_validate_and_load(bg_file.data, bg_file.size, &alloc, &bg, &bg_error) &&
                    saturn::vdp2_backend_upload_background(bg)) {
                    Log_Message(LOG_LEVEL_INFO, "saturn_asset", "VDP2 bg %ux%u", bg->width,
                                bg->height);
                } else {
                    Log_Message(LOG_LEVEL_WARNING, "saturn_asset",
                                "VDP2 bg upload failed (asset %u)", static_cast<unsigned>(bg_error));
                }
                FileSystem_FreeFile(&bg_file);
            } else {
                Log_Message(LOG_LEVEL_WARNING, "saturn_asset", "TEX1.TEX not loaded from CD");
            }
        }

        if (!saturn::asset_heap().free(scratch)) {
            Log_Message(LOG_LEVEL_ERROR, "saturn_asset", "self-test scratch free failed");
        }
    }

    saturn::parallel_workload_init(s_boot_mesh);

    // Строка должна уместиться в 40 колонок VDP2-текста (строка 27).
    Log_Message(LOG_LEVEL_INFO, "asset", "CD mesh=%u tex=%u bg=%u heap=%u", mesh_ok ? 1u : 0u,
                texture_ok ? 1u : 0u, saturn::vdp2_backend_stats().uploads != 0u ? 1u : 0u,
                static_cast<unsigned>(saturn::asset_heap().used()));
}

fixed_t degrees(int32_t value)
{
    return fixed_mul(fixed_div(fixed_mul(FIXED_PI, fixed_from_int(value)), fixed_from_int(180)),
                     FIXED_ONE);
}

void reset_scene_state(void)
{
    s_object_yaw = degrees(25);
    s_object_pitch = degrees(-12);
    s_camera_yaw = 0;
    s_camera_distance = fixed_from_int(7);
    s_camera_height = fixed_from_int(3);
    s_bg_scroll_x = fixed_from_int(32);
    s_bg_scroll_y = fixed_from_int(16);
}

void update_camera_matrices(void)
{
    const fixed_t eye_x = fixed_mul(sin_fixed(s_camera_yaw), s_camera_distance);
    const fixed_t eye_z = fixed_mul(cos_fixed(s_camera_yaw), s_camera_distance);
    mat4_look_at(&s_view, vec3_make(eye_x, s_camera_height, eye_z), vec3_make(0, 0, 0),
                 vec3_make(0, FIXED_ONE, 0));
    const fixed_t fov = degrees(70);
    mat4_perspective(&s_proj, fov, fixed_div(fixed_from_int(320), fixed_from_int(224)),
                     fixed_from_int(1), fixed_from_int(100));
    mat4_multiply(&s_mvp, &s_proj, &s_view);
}

void build_static_mvp(void)
{
    reset_scene_state();
    update_camera_matrices();
}

void init_demo_vertices(void)
{
    s_demo_vertex_count = 0u;
    if (s_boot_mesh != nullptr && s_boot_mesh->vertex_count <= kDemoMaxVertices) {
        for (uint32_t i = 0u; i < s_boot_mesh->vertex_count; ++i) {
            s_demo_vertices[i] = s_boot_mesh->vertices[i];
            // Мягкий тинт вокруг белого: VDP1 Gouraud — аддитивные offsets
            // (16 = нейтраль), сильные цвета разрушают текстуру.
            static const uint8_t tint[8][3] = {{255, 232, 232}, {232, 255, 232},
                                               {232, 232, 255}, {255, 255, 232},
                                               {255, 232, 255}, {232, 255, 255},
                                               {255, 255, 255}, {238, 238, 238}};
            s_demo_vertices[i].r = tint[i & 7u][0];
            s_demo_vertices[i].g = tint[i & 7u][1];
            s_demo_vertices[i].b = tint[i & 7u][2];
            s_demo_vertices[i].a = 255u;
        }
        s_demo_vertex_count = s_boot_mesh->vertex_count;
    }
}

void update_demo_model(void)
{
    Mat4 ry;
    Mat4 rx;
    mat4_rotate_y(&ry, s_object_yaw);
    mat4_rotate_x(&rx, s_object_pitch);
    mat4_multiply(&s_cube_model, &ry, &rx);
}

void update_input_and_scene(void)
{
    const fixed_t step = degrees(2);
    const fixed_t zero = 0;

    if (Input_IsPadButtonDown(0u, INPUT_PAD_START)) {
        reset_scene_state();
    }
    if (Input_IsPadButtonDown(0u, INPUT_PAD_LEFT)) {
        s_object_yaw = fixed_sub(s_object_yaw, step);
    }
    if (Input_IsPadButtonDown(0u, INPUT_PAD_RIGHT)) {
        s_object_yaw = fixed_add(s_object_yaw, step);
    }
    if (Input_IsPadButtonDown(0u, INPUT_PAD_UP)) {
        s_object_pitch = fixed_sub(s_object_pitch, step);
    }
    if (Input_IsPadButtonDown(0u, INPUT_PAD_DOWN)) {
        s_object_pitch = fixed_add(s_object_pitch, step);
    }
    const fixed_t pitch_limit = degrees(60);
    if (s_object_pitch < fixed_sub(zero, pitch_limit)) {
        s_object_pitch = fixed_sub(zero, pitch_limit);
    } else if (s_object_pitch > pitch_limit) {
        s_object_pitch = pitch_limit;
    }

    if (Input_IsPadButtonDown(0u, INPUT_PAD_CROSS)) {
        s_camera_distance = fixed_sub(s_camera_distance, fixed_from_int(1));
    }
    if (Input_IsPadButtonDown(0u, INPUT_PAD_CIRCLE)) {
        s_camera_distance = fixed_add(s_camera_distance, fixed_from_int(1));
    }
    if (Input_IsPadButtonDown(0u, INPUT_PAD_SQUARE)) {
        s_camera_height = fixed_sub(s_camera_height, fixed_from_int(1));
    }
    if (Input_IsPadButtonDown(0u, INPUT_PAD_TRIANGLE)) {
        s_camera_height = fixed_add(s_camera_height, fixed_from_int(1));
    }
    if (Input_IsPadButtonDown(0u, INPUT_PAD_L1)) {
        s_camera_yaw = fixed_sub(s_camera_yaw, step);
    }
    if (Input_IsPadButtonDown(0u, INPUT_PAD_R1)) {
        s_camera_yaw = fixed_add(s_camera_yaw, step);
    }
    // Диапазоны камеры: 4..12 units, высота 1..6.
    const fixed_t min_dist = fixed_from_int(4);
    const fixed_t max_dist = fixed_from_int(12);
    if (s_camera_distance < min_dist) {
        s_camera_distance = min_dist;
    } else if (s_camera_distance > max_dist) {
        s_camera_distance = max_dist;
    }
    const fixed_t min_height = fixed_from_int(1);
    const fixed_t max_height = fixed_from_int(6);
    if (s_camera_height < min_height) {
        s_camera_height = min_height;
    } else if (s_camera_height > max_height) {
        s_camera_height = max_height;
    }

    update_camera_matrices();
    update_demo_model();
    // Фон следует за орбитой камеры: fixed yaw → целые пиксели VDP2.
    s_bg_scroll_x = fixed_add(fixed_from_int(32), fixed_mul(s_camera_yaw, fixed_from_int(32)));
    s_bg_scroll_y = fixed_from_int(16);
    if (s_bg_scroll_x < 0) {
        s_bg_scroll_x = 0;
    }
    const fixed_t max_x =
        fixed_from_int(static_cast<int32_t>(saturn::VDP2_BITMAP_WIDTH - saturn::VDP2_SCREEN_WIDTH));
    if (s_bg_scroll_x > max_x) {
        s_bg_scroll_x = max_x;
    }
    saturn::vdp2_backend_set_scroll(s_bg_scroll_x, s_bg_scroll_y);
}


void log_benchmark(const saturn::ParallelBenchResult& bench)
{
    if (!bench.slave_used) {
        Log_Message(LOG_LEVEL_WARNING, "par", "1=%u 2=n/a h=%u", bench.single_us,
                    bench.hashes_match ? 1u : 0u);
        return;
    }
    // Единицы — микросекунды; g — выигрыш dual-CPU в процентах; h=1 — hash совпал.
    Log_Message(LOG_LEVEL_INFO, "par", "1=%u 2=%u g=%u h=%u", bench.single_us, bench.dual_us,
                bench.speedup_pct, bench.hashes_match ? 1u : 0u);
}

void log_frame_metrics(void)
{
    const saturn::SlaveWorkerStats& stats = saturn::slave_worker_stats();
    // Компактно, чтобы строка 27 (40 колонок) не обрезала hash при больших f.
    Log_Message(LOG_LEVEL_INFO, "par", "f=%uc=%uw=%uh=%x", s_frame_count, stats.completed,
                stats.last_wait_us, saturn::parallel_workload_last_hash() & 0xFFFFFFu);
}

}  // namespace

extern "C" void rpe_saturn_vdp2_vblank_tick(void)
{
    saturn::vdp2_backend_commit_vblank();
}

extern "C" void rpe_saturn_frame_tick(void)
{
    if (!Platform_PumpEvents()) {
        return;  // На Saturn нет SDL_QUIT; контракт на будущее.
    }

    // S31–32: ввод камеры/объекта и лёгкий Master+Slave workload каждый
    // кадр (16 треугольников на CPU) — оба SH-2 задействованы, бюджет 60 Гц.
    update_input_and_scene();
    const saturn::ParallelFrameResult frame =
        saturn::parallel_workload_run_frame_n(&s_mvp, 16u, 20u);
    if (s_frame_count == 0u) {
        s_first_hash = frame.hash;
    } else if (frame.hash != s_first_hash) {
        ++s_determinism_failures;
        if (s_determinism_failures == 1u) {
            Log_Message(LOG_LEVEL_ERROR, "par", "determinism mismatch");
        }
    }
    ++s_frame_count;
    if ((s_frame_count % (kMetricsPeriodFrames * 2u)) == 0u) {
        log_benchmark(s_bench);
    } else if ((s_frame_count % kMetricsPeriodFrames) == 0u) {
        log_frame_metrics();
    }
    if ((s_frame_count % 120u) == 0u) {
        const saturn::Vdp1BackendStats& stats = saturn::vdp1_backend_stats();
        const uint32_t overdraw = (stats.pixels_last * 1000u) / (320u * 224u);
        Log_Message(LOG_LEVEL_INFO, "dbg", "HELLO f=%u c=%u o=%u s=%u", s_frame_count,
                    stats.commands_last, overdraw,
                    saturn::vdp2_backend_stats().scroll_commits);
    }
#if RPE_SATURN_FAULT_FLOOD
    if ((s_frame_count % (kMetricsPeriodFrames * 3u)) == 0u) {
        Log_Message(LOG_LEVEL_WARNING, "scu", "flood overflows=%u", s_flood_overflows);
    }
#endif
#if RPE_SATURN_FAULT_HANG
    if ((s_frame_count % 60u) == 0u) {
        Log_Message(LOG_LEVEL_INFO, "par", "hang f=%u", s_frame_count);
    }
#endif

    Render_SetCamera(&s_view, &s_proj);
    Render_BeginFrame();
    Render_Clear(0u, 0u, 32u);  // golden clear (0,0,32) → VDP1 EWDR/VDP2 0x9000
    if (s_texture != RENDER_TEXTURE_INVALID) {
        Render_BindTexture(s_texture);
    } else {
        Render_BindTexture(RENDER_TEXTURE_INVALID);
    }
    if (s_boot_mesh != nullptr && s_demo_vertex_count != 0u) {
        Render_DrawMesh(s_demo_vertices, s_demo_vertex_count, s_boot_mesh->indices,
                        s_boot_mesh->index_count, &s_cube_model);
    }
    // VDP1-overlay из ~156 команд снижает кадр до 20 Гц; для gate
    // используем VDP2-строку (см. dbg-лог), VDP1-overlay остаётся API.
    Render_EndFrame();
    Platform_SwapBuffers();
}

extern "C" void jo_main(void)
{
    if (!Platform_Init()) {
        // На Saturn нет stdout/host-ФС; при отказе ядра не запускаем цикл.
        for (;;) {
        }
    }
    if (!Render_Init(320u, 224u)) {
        // Target profile ADR-008; Render_Init сам диагностирует неверный размер.
        for (;;) {
        }
    }
    if (!saturn::vdp2_backend_init()) {
        Log_Message(LOG_LEVEL_WARNING, "saturn_hal", "VDP2 background disabled");
    }

    if (!rpe_saturn_add_frame_callback(rpe_saturn_frame_tick)) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "frame callback registration failed");
        for (;;) {
        }
    }
    if (!rpe_saturn_add_vblank_callback(rpe_saturn_vdp2_vblank_tick)) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vblank callback registration failed");
        for (;;) {
        }
    }

    // Self-test идёт до benchmark: CUBE.MSH остаётся в persistent-арене и
    // становится входом параллельного workload.
    boot_asset_self_test();
    build_static_mvp();
    Render_SetCamera(&s_view, &s_proj);
    init_demo_vertices();
    update_demo_model();

    s_bench = saturn::parallel_workload_benchmark(&s_mvp, 3u);
    log_benchmark(s_bench);

#if RPE_SATURN_FAULT_HANG
    // Ручная приёмка: следующий Slave-job зависает; Master обязан уйти в
    // fallback по таймауту и продолжить кадры.
    saturn::slave_worker_set_fault(saturn::SLAVE_FAULT_HANG_NEXT);
#endif
#if RPE_SATURN_FAULT_FLOOD
    // Ручная приёмка: переполнение очереди на реальном Saturn.
    s_flood_overflows = saturn::slave_worker_debug_flood_probe();
    Log_Message(LOG_LEVEL_WARNING, "scu", "flood overflows=%u", s_flood_overflows);
#endif

    Log_Message(LOG_LEVEL_INFO, "saturn_hal", "boot complete: bg=%u",
                saturn::vdp2_backend_stats().uploads);

    rpe_saturn_core_run();  // не возвращается: VBlank sync + callbacks
    for (;;) {
    }
}
