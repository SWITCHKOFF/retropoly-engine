// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/main_n64.cpp
//
// N64 render-demo S43–44 (ADR-015): RSP T&L + RDP с аппаратным Z-буфером,
// perspective-correct текстурами и TMEM-полосами.
//
// Boot:
//   Platform_Init (VI 320×240 16-bit + DFS + ассеты) → idempotence self-test
//   → Render_Init → RSP T&L self-test (W0-векторы) → W0 5-case framebuffer
//   hash → T&L benchmark CPU vs RSP (48/256 треугольников) → main loop.
//
// Main loop: 4 куба / 48 треугольников (W1-подобная нагрузка), вращение,
// текстура 64×64 CI8 (2 полосы TMEM), overlay (RSP/CPU, frame, FPS).
// C-Down переключает CPU T&L fallback для ручного gate (кадр/hash).
#include "hal/platform.h"

#include "core/log/log.h"
#include "core/math/mat4.h"
#include "core/math/vec3.h"
#include "hal/n64/n64_assets.h"
#include "hal/n64/n64_bridge.h"
#include "hal/n64/n64_fs.h"
#include "hal/n64/n64_input.h"
#include "hal/n64/n64_memory.h"
#include "hal/n64/n64_render_bridge.h"
#include "hal/n64/n64_render_control.h"
#include "hal/n64/rsp_tnl.h"
#include "hal/render_backend.h"
#include "render/texture.h"
#include "retro_engine/version.h"

#include <stdio.h>
#include <string.h>

namespace {

constexpr uint32_t W0_CASE_COUNT = 5u;
constexpr uint32_t W1_CUBE_COUNT = 4u;
constexpr uint32_t W1_TRIANGLES = W1_CUBE_COUNT * 12u; // 48
constexpr uint32_t BENCH_TRIANGLES_MAX = 256u;

uint32_t s_frames = 0u;
TextureHandle s_checker_texture = RENDER_TEXTURE_INVALID;
TextureHandle s_cube_texture = RENDER_TEXTURE_INVALID;
const MeshAsset* s_cube_mesh = nullptr;
fixed_t s_angle = 0;
bool s_prev_fallback_button = false;
const bool s_show_overlay = true;

uint8_t s_w0_pixels[64u * 64u];
uint16_t s_w0_palette[256];

constexpr uint32_t CUBE_MAX_VERTICES = 64u;
alignas(16) Vertex s_lit_vertices[W1_CUBE_COUNT][CUBE_MAX_VERTICES];
bool s_lighting_ok = false;
const uint8_t s_ambient[3] = {40u, 40u, 40u};
const uint8_t s_light_rgb[3] = {255u, 255u, 255u};
const float s_light_world[3] = {0.398f, 0.597f, 0.697f};

struct W0HashRecord
{
    const char* name;
    uint32_t hash;
    uint32_t submitted;
    uint32_t culled;
    uint32_t clipped;
    uint32_t depth_rejected;
};

W0HashRecord s_w0_results[W0_CASE_COUNT];

struct BenchResult
{
    uint32_t triangles;
    uint32_t cpu_us;
    uint32_t rsp_us;
    uint32_t sink;
};

BenchResult s_bench_48;
BenchResult s_bench_256;

uint32_t s_frame_us_acc = 0u;
uint32_t s_frame_us_count = 0u;
uint32_t s_begin_us_acc = 0u;
uint32_t s_submit_us_acc = 0u;
uint32_t s_end_us_acc = 0u;
uint32_t s_light_us_acc = 0u;
uint32_t s_fps_start_ms = 0u;

/**
 * S43–44: сверка RSP T&L с CPU-эталоном на W0-подобных векторах.
 * Ошибка > 8 Q16.16 (1.2e-4) считается провалом self-test.
 */
bool tnl_self_test()
{
    if (!rpe_n64_tnl_available()) {
        Log_Message(LOG_LEVEL_ERROR, "n64_tnl", "self-test: RSP T&L unavailable");
        return false;
    }

    const float mvp[16] = {
        1.2f, 0.0f, 0.0f, 0.75f,
        0.0f, 0.9f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, -1.0f,
        0.1f, 0.2f, 0.0f, 8.0f,
    };
    n64::TnlVertex verts[3] = {
        {1.0f, 2.0f, -5.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        {-2.0f, 1.0f, -7.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
        {0.5f, -3.0f, -9.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f},
    };

    n64::TnlMatrix matrix;
    if (!n64::tnl_matrix_from_mvp_gl(mvp, &matrix)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_tnl", "self-test: matrix conversion failed");
        return false;
    }

    RpeN64TnlSlot* in = rpe_n64_tnl_input_slot(0u);
    RpeN64TnlSlot* out = rpe_n64_tnl_output_slot(0u);
    if (in == nullptr || out == nullptr) {
        return false;
    }
    n64::tnl_vertices_to_slot(verts, in);
    RpeN64TnlSlot matrix_slot;
    n64::tnl_matrix_to_slot(matrix, &matrix_slot);
    if (!rpe_n64_tnl_transform(0u, &matrix_slot)) {
        return false;
    }
    rpe_n64_tnl_wait();

    int32_t cpu[3][4];
    n64::tnl_transform_clip_cpu(matrix, verts, cpu);
    int32_t max_error = 0;
    for (uint32_t k = 0u; k < 3u; ++k) {
        for (uint32_t c = 0u; c < 4u; ++c) {
            const int32_t rsp = n64::tnl_slot_component(*out, k * 4u + c);
            int32_t delta = rsp - cpu[k][c];
            if (delta < 0) {
                delta = -delta;
            }
            if (delta > max_error) {
                max_error = delta;
            }
        }
    }

    const int32_t tolerance = 8;
    Log_Message(max_error <= tolerance ? LOG_LEVEL_INFO : LOG_LEVEL_ERROR, "n64_tnl",
                "self-test: RSP vs CPU max_error=%ld (tol=%ld) %s", (long)max_error,
                (long)tolerance, max_error <= tolerance ? "PASS" : "FAIL");
    if (max_error > tolerance) {
        Log_Message(LOG_LEVEL_ERROR, "n64_tnl",
                    "RSP clip v0=(%ld,%ld,%ld,%ld) CPU (%ld,%ld,%ld,%ld)",
                    (long)n64::tnl_slot_component(*out, 0u),
                    (long)n64::tnl_slot_component(*out, 1u),
                    (long)n64::tnl_slot_component(*out, 2u),
                    (long)n64::tnl_slot_component(*out, 3u), (long)cpu[0][0], (long)cpu[0][1],
                    (long)cpu[0][2], (long)cpu[0][3]);
    }
    return max_error <= tolerance;
}

[[noreturn]] void halt_forever(void)
{
    for (;;) {
        rpe_n64_sleep_ms(100u);
    }
}

void boot_platform_or_halt(void)
{
    if (!Platform_Init()) {
        halt_forever();
    }
}

void build_w0_checker()
{
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            const uint32_t cell = ((x / 8u) + (y / 8u)) & 1u;
            s_w0_pixels[y * 64u + x] = (uint8_t)cell;
        }
    }
    s_w0_palette[0] = pack_rgb555_stp(6, 2, 12, false);
    s_w0_palette[1] = pack_rgb555_stp(24, 26, 30, false);
    for (uint32_t i = 2u; i < 256u; ++i) {
        const uint8_t shade = (uint8_t)(i % 32u);
        s_w0_palette[i] = pack_rgb555_stp(shade, 0, (uint8_t)(31u - shade), false);
    }
}

void set_w0_vertex(Vertex* v, fixed_t x, fixed_t y, fixed_t z)
{
    memset(v, 0, sizeof(*v));
    v->x = x;
    v->y = y;
    v->z = z;
    v->u = 0u;
    v->v = 0u;
    v->r = 255u;
    v->g = 255u;
    v->b = 255u;
    v->a = 255u;
}

/** W0-кейсы: visible/backface/near-cross/far-cross/behind-camera. */
void build_w0_case(uint32_t case_id, Vertex v[3], Triangle* tri)
{
    // Камера (0,0,5) смотрит −Z; near=1, far=100 (distance = 5 − z).
    if (case_id == 0u) { // visible: z=-5 → distance 10
        set_w0_vertex(&v[0], fixed_from_int(-2), fixed_from_int(-2), fixed_from_int(-5));
        set_w0_vertex(&v[1], fixed_from_int(2), fixed_from_int(-2), fixed_from_int(-5));
        set_w0_vertex(&v[2], fixed_from_int(0), fixed_from_int(2), fixed_from_int(-5));
    } else if (case_id == 1u) { // backface: обратный winding
        set_w0_vertex(&v[0], fixed_from_int(-2), fixed_from_int(-2), fixed_from_int(-5));
        set_w0_vertex(&v[1], fixed_from_int(0), fixed_from_int(2), fixed_from_int(-5));
        set_w0_vertex(&v[2], fixed_from_int(2), fixed_from_int(-2), fixed_from_int(-5));
    } else if (case_id == 2u) { // near-cross: z=4.5 → distance 0.5 < near
        set_w0_vertex(&v[0], fixed_from_int(-2), fixed_from_int(-2), fixed_from_float(4.5f));
        set_w0_vertex(&v[1], fixed_from_int(2), fixed_from_int(-2), fixed_from_int(-5));
        set_w0_vertex(&v[2], fixed_from_int(0), fixed_from_int(2), fixed_from_int(-5));
    } else if (case_id == 3u) { // far-cross: z=-96 → distance 101 > far
        set_w0_vertex(&v[0], fixed_from_int(-2), fixed_from_int(-2), fixed_from_int(-96));
        set_w0_vertex(&v[1], fixed_from_int(2), fixed_from_int(-2), fixed_from_int(-96));
        set_w0_vertex(&v[2], fixed_from_int(0), fixed_from_int(2), fixed_from_int(-96));
    } else { // behind camera: z=6 → distance −1
        set_w0_vertex(&v[0], fixed_from_int(-2), fixed_from_int(-2), fixed_from_int(6));
        set_w0_vertex(&v[1], fixed_from_int(2), fixed_from_int(-2), fixed_from_int(6));
        set_w0_vertex(&v[2], fixed_from_int(0), fixed_from_int(2), fixed_from_int(6));
    }
    tri->v0 = 0u;
    tri->v1 = 1u;
    tri->v2 = 2u;
}

void w0_camera(Mat4* view, Mat4* proj)
{
    const Vec3 eye = vec3_make(0, 0, fixed_from_int(5));
    const Vec3 target = vec3_make(0, 0, 0);
    const Vec3 up = vec3_make(0, FIXED_ONE, 0);
    mat4_look_at(view, eye, target, up);
    mat4_perspective(proj, fixed_from_float(1.04719755f), fixed_from_float(4.0f / 3.0f),
                     fixed_from_int(1), fixed_from_int(100));
}

void w1_camera(Mat4* view, Mat4* proj)
{
    const Vec3 eye = vec3_make(0, 0, fixed_from_int(12));
    const Vec3 target = vec3_make(0, 0, 0);
    const Vec3 up = vec3_make(0, FIXED_ONE, 0);
    mat4_look_at(view, eye, target, up);
    mat4_perspective(proj, fixed_from_float(1.04719755f), fixed_from_float(4.0f / 3.0f),
                     fixed_from_int(1), fixed_from_int(100));
}

bool hash_framebuffer(uint32_t* out_hash_rgb, uint32_t* out_hash_rgba)
{
    // W0-readback 320×240×4 = 300 KiB; временно берём из asset arena
    // (ассеты уже загружены), чтобы не держать ещё один static-буфер и
    // сохранить 4 MiB RDRAM ledger для SDK/display heap.
    const size_t bytes = (size_t)RENDER_FB_WIDTH * RENDER_FB_HEIGHT * 4u;
    const LinearAllocator::Marker marker = n64::asset_arena().mark();
    uint8_t* rgba = static_cast<uint8_t*>(n64::asset_arena().allocate(bytes, 16u));
    if (rgba == nullptr) {
        return false;
    }
    const bool read_ok = hal_render_backend_read_pixels(rgba, (uint32_t)bytes);
    uint32_t hash = 2166136261u;
    if (read_ok) {
        for (uint32_t i = 0u; i < RENDER_FB_WIDTH * RENDER_FB_HEIGHT; ++i) {
            hash ^= rgba[i * 4u + 0u];
            hash *= 16777619u;
            hash ^= rgba[i * 4u + 1u];
            hash *= 16777619u;
            hash ^= rgba[i * 4u + 2u];
            hash *= 16777619u;
        }
        *out_hash_rgb = hash;
        *out_hash_rgba = n64::tnl_fnv1a32(rgba, bytes);
    }
    (void)n64::asset_arena().rollback(marker);
    return read_ok;
}

/** Один W0-кейс: clear → 1 triangle → readback → FNV hash (app-side). */
bool render_w0_case(uint32_t case_id, uint32_t* out_hash_rgb, uint32_t* out_hash_rgba)
{
    Mat4 view;
    Mat4 proj;
    w0_camera(&view, &proj);
    Mat4 identity;
    mat4_identity(&identity);

    Vertex vertices[3];
    Triangle triangle;
    build_w0_case(case_id, vertices, &triangle);

    Render_BeginFrame();
    Render_Clear(0u, 0u, 32u);
    Render_SetCamera(&view, &proj);
    Render_BindTexture(s_checker_texture);
    Render_DrawMesh(vertices, 3u, &triangle, 1u, &identity);
    Render_EndFrame();
    return hash_framebuffer(out_hash_rgb, out_hash_rgba);
}

/** Z-тест: near сначала, far вторым (без Painter's) — far не должен перекрыть near. */
bool z_self_test()
{
    Mat4 view;
    Mat4 proj;
    w0_camera(&view, &proj);
    hal_render_backend_set_camera(&view, &proj);
    hal_render_backend_begin_frame(0u, 0u, 0u);

    HalRenderBackendVertex near_tri[3] = {
        {-2.0f, -2.0f, -5.0f, 0, 0, 1, 0, 0, 1},
        {2.0f, -2.0f, -5.0f, 0, 0, 1, 0, 0, 1},
        {0.0f, 2.0f, -5.0f, 0, 0, 1, 0, 0, 1},
    };
    HalRenderBackendVertex far_tri[3] = {
        {-2.0f, -2.0f, -10.0f, 0, 0, 0, 1, 0, 1},
        {2.0f, -2.0f, -10.0f, 0, 0, 0, 1, 0, 1},
        {0.0f, 2.0f, -10.0f, 0, 0, 0, 1, 0, 1},
    };
    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    };
    const bool submitted =
        hal_render_backend_draw_triangle(near_tri, identity, RENDER_TEXTURE_INVALID) &&
        hal_render_backend_draw_triangle(far_tri, identity, RENDER_TEXTURE_INVALID);
    hal_render_backend_end_frame();

    const size_t bytes = (size_t)RENDER_FB_WIDTH * RENDER_FB_HEIGHT * 4u;
    const LinearAllocator::Marker marker = n64::asset_arena().mark();
    uint8_t* rgba = static_cast<uint8_t*>(n64::asset_arena().allocate(bytes, 16u));
    if (rgba == nullptr) {
        return false;
    }
    const bool read_ok = submitted && hal_render_backend_read_pixels(rgba, (uint32_t)bytes);
    bool pass = false;
    if (read_ok) {
        const uint8_t* pixel =
            &rgba[((RENDER_FB_HEIGHT / 2u) * RENDER_FB_WIDTH + RENDER_FB_WIDTH / 2u) * 4u];
        pass = pixel[0] > 128u && pixel[1] < 64u;
        Log_Message(pass ? LOG_LEVEL_INFO : LOG_LEVEL_ERROR, "n64_z",
                    "self-test near-first/far-second: pixel=(%u,%u,%u) %s", (unsigned)pixel[0],
                    (unsigned)pixel[1], (unsigned)pixel[2], pass ? "PASS" : "FAIL");
    }
    (void)n64::asset_arena().rollback(marker);
    return pass;
}

/** 5 W0-кейсов + сверка RSP/CPU-fallback хэша visible. */
void run_w0_golden()
{
    // Эталоны сняты в pinned gopher64 1.2.1 (2026-11-14); при изменении
    // рендер-пути обновляются вместе с benchmarks/n64_render_s43_44.yaml.
    static const uint32_t kExpected[W0_CASE_COUNT] = {
        0x9e92e718u, 0x569949c5u, 0x569949c5u, 0x569949c5u, 0x569949c5u,
    };

    for (uint32_t case_id = 0u; case_id < W0_CASE_COUNT; ++case_id) {
        const RenderDiagnostics diag_before = render_diagnostics();
        (void)diag_before;
        uint32_t hash_rgb = 0u;
        uint32_t hash_rgba = 0u;
        const bool hash_ok = render_w0_case(case_id, &hash_rgb, &hash_rgba);
        const RenderDiagnostics diag = render_diagnostics();
        s_w0_results[case_id].name = (case_id == 0u)   ? "visible"
                                     : (case_id == 1u) ? "backface"
                                     : (case_id == 2u) ? "near-cross"
                                     : (case_id == 3u) ? "far-cross"
                                                       : "behind";
        s_w0_results[case_id].hash = hash_ok ? hash_rgb : 0u;
        s_w0_results[case_id].submitted = diag.submitted;
        s_w0_results[case_id].culled = diag.backface_rejects;
        s_w0_results[case_id].clipped = diag.clip_rejects;
        s_w0_results[case_id].depth_rejected = diag.depth_rejects;
        const bool expected_ok = hash_ok && hash_rgb == kExpected[case_id];
        Log_Message(expected_ok ? LOG_LEVEL_INFO : LOG_LEVEL_ERROR, "n64_w0",
                    "case=%s hash=%08lx expected=%08lx %s submitted=%u cull=%u clip=%u depth=%u",
                    s_w0_results[case_id].name, (unsigned long)hash_rgb,
                    (unsigned long)kExpected[case_id], expected_ok ? "PASS" : "FAIL",
                    (unsigned)s_w0_results[case_id].submitted,
                    (unsigned)s_w0_results[case_id].culled,
                    (unsigned)s_w0_results[case_id].clipped,
                    (unsigned)s_w0_results[case_id].depth_rejected);
    }

    // Ручной checklist: включить CPU-fallback и сравнить кадр/hash с RSP.
    uint32_t rsp_hash = s_w0_results[0].hash;
    hal_render_backend_set_cpu_fallback(true);
    uint32_t cpu_hash = 0u;
    uint32_t cpu_hash_rgba = 0u;
    const bool cpu_ok = render_w0_case(0u, &cpu_hash, &cpu_hash_rgba);
    hal_render_backend_set_cpu_fallback(false);
    Log_Message(cpu_ok && cpu_hash == rsp_hash ? LOG_LEVEL_INFO : LOG_LEVEL_ERROR, "n64_w0",
                "fallback visible hash=%08lx rsp=%08lx %s", (unsigned long)cpu_hash,
                (unsigned long)rsp_hash,
                (cpu_ok && cpu_hash == rsp_hash) ? "PASS" : "FAIL");
}

const n64::TnlVertex* bench_vertices()
{
    static n64::TnlVertex verts[BENCH_TRIANGLES_MAX * 3u];
    static bool built = false;
    if (!built) {
        for (uint32_t i = 0u; i < BENCH_TRIANGLES_MAX; ++i) {
            const float base = (float)(i % 16u) * 0.2f - 1.6f;
            for (uint32_t v = 0u; v < 3u; ++v) {
                n64::TnlVertex& out = verts[i * 3u + v];
                out.x = base + (float)v * 0.3f;
                out.y = -1.0f + (float)((i + v) % 4u) * 0.5f;
                out.z = -10.0f - (float)(i % 8u);
                out.u = (float)v / 2.0f;
                out.v = (float)((i + v) % 2u);
                out.r = 1.0f;
                out.g = 1.0f;
                out.b = 1.0f;
                out.a = 1.0f;
            }
        }
        built = true;
    }
    return verts;
}

BenchResult benchmark_tnl(uint32_t triangles)
{
    BenchResult result = {};
    result.triangles = triangles;

    const float mvp[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, -10.0f, 0.0f,
    };
    n64::TnlMatrix matrix;
    if (!n64::tnl_matrix_from_mvp_gl(mvp, &matrix)) {
        return result;
    }
    const n64::TnlVertex* verts = bench_vertices();
    volatile int32_t sink = 0;

    // CPU reference.
    uint64_t start = rpe_n64_ticks_us();
    for (uint32_t i = 0u; i < triangles; ++i) {
        int32_t clip[3][4];
        n64::tnl_transform_clip_cpu(matrix, &verts[i * 3u], clip);
        sink += clip[0][0];
    }
    result.cpu_us = (uint32_t)(rpe_n64_ticks_us() - start);

    // RSP batch.
    if (rpe_n64_tnl_available()) {
        for (uint32_t i = 0u; i < triangles; ++i) {
            RpeN64TnlSlot* slot = rpe_n64_tnl_input_slot(i);
            if (slot == nullptr) {
                return result;
            }
            n64::tnl_vertices_to_slot(&verts[i * 3u], slot);
        }
        RpeN64TnlSlot matrix_slot;
        n64::tnl_matrix_to_slot(matrix, &matrix_slot);

        start = rpe_n64_ticks_us();
        for (uint32_t i = 0u; i < triangles; ++i) {
            (void)rpe_n64_tnl_transform(i, &matrix_slot);
        }
        rpe_n64_tnl_wait();
        const RpeN64TnlSlot* out = rpe_n64_tnl_output_slot(0u);
        if (out != nullptr) {
            sink += n64::tnl_slot_component(*out, 0u);
        }
        result.rsp_us = (uint32_t)(rpe_n64_ticks_us() - start);
    }
    result.sink = (uint32_t)sink;
    return result;
}

void run_benchmarks()
{
    s_bench_48 = benchmark_tnl(48u);
    s_bench_256 = benchmark_tnl(BENCH_TRIANGLES_MAX);
    Log_Message(LOG_LEVEL_INFO, "n64_bench",
                "tnl triangles=48 cpu_us=%u rsp_us=%u; triangles=256 cpu_us=%u rsp_us=%u",
                (unsigned)s_bench_48.cpu_us, (unsigned)s_bench_48.rsp_us,
                (unsigned)s_bench_256.cpu_us, (unsigned)s_bench_256.rsp_us);
    Log_Message(LOG_LEVEL_INFO, "n64_bench", "bench sink=%u",
                (unsigned)(s_bench_48.sink ^ s_bench_256.sink));
}

void make_dot_matrix(const float dir[3], RpeN64TnlSlot* slot)
{
    n64::TnlMatrix matrix = {};
    // RSP transform: row0 · v = dir·n; остальные строки нулевые.
    matrix.m[0] = n64::tnl_float_to_fixed(dir[0]);
    matrix.m[4] = n64::tnl_float_to_fixed(dir[1]);
    matrix.m[8] = n64::tnl_float_to_fixed(dir[2]);
    n64::tnl_matrix_to_slot(matrix, slot);
}

/** RSP diffuse dot: n·L на верифицированном transform-ядре. */
bool light_self_test()
{
    if (!rpe_n64_tnl_available()) {
        Log_Message(LOG_LEVEL_ERROR, "n64_light", "self-test: RSP T&L unavailable");
        return false;
    }
    const float normals[3][3] = {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f},
                                 {0.0f, 0.0f, -1.0f}};
    const float light[3] = {0.0f, 0.0f, 1.0f};
    n64::TnlVertex verts[3];
    for (uint32_t v = 0u; v < 3u; ++v) {
        verts[v] = n64::TnlVertex{normals[v][0], normals[v][1], normals[v][2], 0, 0,
                                  1,               1,               1,               1};
    }
    RpeN64TnlSlot matrix_slot;
    make_dot_matrix(light, &matrix_slot);
    RpeN64TnlSlot* input = rpe_n64_tnl_input_slot(0u);
    RpeN64TnlSlot* output = rpe_n64_tnl_output_slot(0u);
    if (input == nullptr || output == nullptr) {
        return false;
    }
    n64::tnl_vertices_to_slot(verts, input);
    if (!rpe_n64_tnl_transform(0u, &matrix_slot)) {
        return false;
    }
    rpe_n64_tnl_wait();

    int32_t expected[3][4];
    n64::TnlMatrix matrix = {};
    matrix.m[0] = n64::tnl_float_to_fixed(light[0]);
    matrix.m[4] = n64::tnl_float_to_fixed(light[1]);
    matrix.m[8] = n64::tnl_float_to_fixed(light[2]);
    n64::tnl_transform_clip_cpu(matrix, verts, expected);
    int32_t max_error = 0;
    for (uint32_t v = 0u; v < 3u; ++v) {
        const int32_t rsp = n64::tnl_slot_component(*output, v * 4u + 0u);
        int32_t delta = rsp - expected[v][0];
        if (delta < 0) {
            delta = -delta;
        }
        if (delta > max_error) {
            max_error = delta;
        }
    }
    const int32_t tolerance = 8;
    Log_Message(max_error <= tolerance ? LOG_LEVEL_INFO : LOG_LEVEL_ERROR, "n64_light",
                "self-test: RSP dot(n,L) vs CPU max_error=%ld (tol=%ld) %s", (long)max_error,
                (long)tolerance, max_error <= tolerance ? "PASS" : "FAIL");
    return max_error <= tolerance;
}

/** Поставить RSP dot(n·L) для куба в очередь (без wait). */
bool light_cube_enqueue(const MeshAsset* mesh, fixed_t angle, uint32_t slot_base)
{
    if (mesh == nullptr || mesh->vertex_count > CUBE_MAX_VERTICES) {
        return false;
    }
    Mat4 inverse;
    mat4_rotate_y(&inverse, -angle);
    const Vec4 local = mat4_transform(
        &inverse, Vec4{fixed_from_float(s_light_world[0]), fixed_from_float(s_light_world[1]),
                       fixed_from_float(s_light_world[2]), 0});
    const float local_dir[3] = {fixed_to_float(local.x), fixed_to_float(local.y),
                                fixed_to_float(local.z)};
    RpeN64TnlSlot matrix_slot;
    make_dot_matrix(local_dir, &matrix_slot);

    const uint32_t vertex_count = mesh->vertex_count;
    for (uint32_t base = 0u; base < vertex_count; base += 3u) {
        const uint32_t slot = slot_base + base / 3u;
        if (slot >= rpe_n64_tnl_slot_capacity()) {
            return false;
        }
        n64::TnlVertex verts[3];
        for (uint32_t v = 0u; v < 3u; ++v) {
            const uint32_t idx = base + v;
            const Vertex& src = mesh->vertices[(idx < vertex_count) ? idx : 0u];
            verts[v] = n64::TnlVertex{fixed_to_float((int32_t)src.nx * 516),
                                      fixed_to_float((int32_t)src.ny * 516),
                                      fixed_to_float((int32_t)src.nz * 516), 0, 0, 1, 1, 1, 1};
        }
        RpeN64TnlSlot* input = rpe_n64_tnl_input_slot(slot);
        if (input == nullptr) {
            return false;
        }
        n64::tnl_vertices_to_slot(verts, input);
        if (!rpe_n64_tnl_transform(slot, &matrix_slot)) {
            return false;
        }
    }
    return true;
}

/** Прочитать dot из RSP-слотов и упаковать ambient+light в u8. */
bool light_cube_read(const MeshAsset* mesh, Vertex* out, uint32_t slot_base)
{
    if (mesh == nullptr || out == nullptr || mesh->vertex_count > CUBE_MAX_VERTICES) {
        return false;
    }
    const uint32_t vertex_count = mesh->vertex_count;
    for (uint32_t v = 0u; v < vertex_count; ++v) {
        out[v] = mesh->vertices[v];
        const uint32_t slot = slot_base + v / 3u;
        const uint32_t lane = v % 3u;
        const RpeN64TnlSlot* output = rpe_n64_tnl_output_slot(slot);
        if (output == nullptr) {
            return false;
        }
        const int32_t dot = n64::tnl_slot_component(*output, lane * 4u + 0u);
        for (uint32_t c = 0u; c < 3u; ++c) {
            int32_t value = (int32_t)s_ambient[c];
            if (dot > 0) {
                value += (int32_t)(((int64_t)dot * s_light_rgb[c]) >> 16);
            }
            if (value > 255) {
                value = 255;
            }
            if (c == 0u) {
                out[v].r = (uint8_t)value;
            } else if (c == 1u) {
                out[v].g = (uint8_t)value;
            } else {
                out[v].b = (uint8_t)value;
            }
        }
    }
    return true;
}

void update_lighting()
{
    if (!s_lighting_ok || s_cube_mesh == nullptr) {
        return;
    }
    for (uint32_t i = 0u; i < W1_CUBE_COUNT; ++i) {
        if (!light_cube_enqueue(s_cube_mesh, s_angle + fixed_from_float((float)i * 0.7f),
                                i * 8u)) {
            s_lighting_ok = false;
            return;
        }
    }
    rpe_n64_tnl_wait();
    for (uint32_t i = 0u; i < W1_CUBE_COUNT; ++i) {
        if (!light_cube_read(s_cube_mesh, s_lit_vertices[i], i * 8u)) {
            s_lighting_ok = false;
            return;
        }
    }
}

void draw_w1_scene(const Mat4& view, const Mat4& proj)
{
    Render_SetCamera(&view, &proj);
    Render_BindTexture(s_cube_texture);
    const fixed_t positions[W1_CUBE_COUNT][3] = {
        {fixed_from_float(-3.5f), fixed_from_float(-2.8f), 0},
        {fixed_from_float(3.5f), fixed_from_float(-2.8f), 0},
        {fixed_from_float(-3.5f), fixed_from_float(2.8f), 0},
        {fixed_from_float(3.5f), fixed_from_float(2.8f), 0},
    };
    for (uint32_t i = 0u; i < W1_CUBE_COUNT; ++i) {
        Mat4 rotation;
        Mat4 translation;
        Mat4 model;
        mat4_rotate_y(&rotation, s_angle + fixed_from_float((float)i * 0.7f));
        mat4_translate(&translation, positions[i][0], positions[i][1], positions[i][2]);
        mat4_multiply(&model, &translation, &rotation);
        const Vertex* vertices = s_lighting_ok ? s_lit_vertices[i] : s_cube_mesh->vertices;
        Render_DrawMesh(vertices, s_cube_mesh->vertex_count, s_cube_mesh->indices,
                        s_cube_mesh->index_count, &model);
    }
}

char s_overlay_text[512];

void draw_overlay()
{
    char* p = s_overlay_text;
    p += snprintf(p, (size_t)(s_overlay_text + sizeof(s_overlay_text) - p),
                  "RPE N64 S43-44\n%s\n48 TRI / 4 CUBES\nTEX64 CI8 STRIPS\nZ 18BIT 320X240\n%s\n",
                  hal_render_backend_rsp_active() ? "RSP TNL+RDP" : "CPU TNL+RDP",
                  s_lighting_ok ? "RSP LIGHT" : "MESH COLORS");
    if (s_frame_us_count != 0u) {
        const uint32_t avg = s_frame_us_acc / s_frame_us_count;
        p += snprintf(p, (size_t)(s_overlay_text + sizeof(s_overlay_text) - p),
                      "FRAME %lu US / %lu FPS\n", (unsigned long)avg,
                      (unsigned long)((avg != 0u) ? (1000000u / avg) : 0u));
    }
    p += snprintf(p, (size_t)(s_overlay_text + sizeof(s_overlay_text) - p),
                  "W0 HASH %08lx\n", (unsigned long)s_w0_results[0].hash);
}

void log_heartbeat()
{
    const n64::BootAssets& assets = n64::boot_assets();
    Log_Message(LOG_LEVEL_INFO, "n64_render",
                "frame=%u time=%u ms tnl=%s draws=%u pending=%u overlay=%u assets=%u/%u",
                (unsigned)s_frames, (unsigned)Platform_GetTimeMs(),
                hal_render_backend_rsp_active() ? "RSP" : "CPU",
                (unsigned)hal_render_backend_draw_call_count(),
                (unsigned)rpe_n64_tnl_failures(), (unsigned)render_diagnostics().submitted,
                assets.mesh_ok ? 1u : 0u, assets.texture_ok ? 1u : 0u);
}

}  // namespace

int main(void)
{
    // Self-test идемпотентности: повторный Init и Init после Shutdown
    // обязаны вернуть true и не сломать VI/debug-канал (критерий S39–40).
    boot_platform_or_halt();
    boot_platform_or_halt();
    Platform_Shutdown();
    boot_platform_or_halt();

    Log_Message(LOG_LEVEL_INFO, "n64_hal", "boot: RetroPolyEngine %s target=n64 arch=mips64",
                RETRO_ENGINE_VERSION_STRING);
    Log_Message(LOG_LEVEL_INFO, "n64_hal",
                "boot: VI 320x240 16-bit NTSC, RDPQ/joypad/DFS ready, time=%u ms",
                Platform_GetTimeMs());

    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) {
        Log_Message(LOG_LEVEL_ERROR, "n64_hal", "boot: Render_Init failed, halt");
        halt_forever();
    }

    if (!tnl_self_test()) {
        Log_Message(LOG_LEVEL_ERROR, "n64_hal", "boot: RSP T&L self-test FAIL (CPU fallback)");
    }
    s_lighting_ok = light_self_test();
    if (!s_lighting_ok) {
        Log_Message(LOG_LEVEL_WARNING, "n64_hal", "boot: RSP lighting unavailable (mesh colors)");
    }

    build_w0_checker();
    s_checker_texture =
        Render_CreateTexture(64u, 64u, s_w0_pixels, s_w0_palette);
    const n64::BootAssets& assets = n64::boot_assets();
    if (assets.texture_ok && assets.texture != nullptr) {
        s_cube_texture = Render_CreateTexture(assets.texture->width, assets.texture->height,
                                              assets.texture->pixels, assets.texture->palette);
    }
    s_cube_mesh = assets.mesh;
    if (s_checker_texture == RENDER_TEXTURE_INVALID || s_cube_texture == RENDER_TEXTURE_INVALID ||
        s_cube_mesh == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "n64_hal", "boot: texture/mesh unavailable, halt");
        halt_forever();
    }

    run_w0_golden();
    (void)z_self_test();
    run_benchmarks();

    Mat4 view;
    Mat4 proj;
    w1_camera(&view, &proj);

    s_fps_start_ms = Platform_GetTimeMs();
    for (;;) {
        (void)Platform_PumpEvents();

        const bool fallback_button = Input_IsPadButtonDown(0u, INPUT_PAD_SELECT);
        if (fallback_button && !s_prev_fallback_button) {
            hal_render_backend_set_cpu_fallback(!hal_render_backend_cpu_fallback());
        }
        s_prev_fallback_button = fallback_button;

        const uint64_t loop_start = rpe_n64_ticks_us();
        s_angle += fixed_from_float(0.02f);
        update_lighting();
        s_light_us_acc += (uint32_t)(rpe_n64_ticks_us() - loop_start);

        const uint64_t frame_start = rpe_n64_ticks_us();
        Render_BeginFrame();
        const uint64_t after_begin = rpe_n64_ticks_us();
        Render_Clear(0u, 0u, 32u);
        draw_w1_scene(view, proj);
        if (s_show_overlay) {
            draw_overlay();
            hal_render_backend_set_text_overlay(s_overlay_text);
        } else {
            hal_render_backend_set_text_overlay(nullptr);
        }
        const uint64_t after_submit = rpe_n64_ticks_us();
        Render_EndFrame();
        const uint64_t after_end = rpe_n64_ticks_us();
        s_begin_us_acc += (uint32_t)(after_begin - frame_start);
        s_submit_us_acc += (uint32_t)(after_submit - after_begin);
        s_end_us_acc += (uint32_t)(after_end - after_submit);
        s_frame_us_acc += (uint32_t)(after_end - loop_start);
        ++s_frame_us_count;
        ++s_frames;

        if ((s_frames % 60u) == 0u) {
            const uint32_t now_ms = Platform_GetTimeMs();
            const uint32_t elapsed_ms = now_ms - s_fps_start_ms;
            const uint32_t fps = (elapsed_ms != 0u) ? (60000u / elapsed_ms) : 0u;
            const uint32_t avg_us = (elapsed_ms != 0u) ? ((elapsed_ms * 1000u) / 60u) : 0u;
            Log_Message(LOG_LEVEL_INFO, "n64_bench",
                        "w1 frames=%u avg_us=%u fps=%u begin=%u submit=%u end=%u light=%u detach=%u tnl=%u rdp=%u draws=%u submitted=%u",
                        (unsigned)s_frames, (unsigned)avg_us, (unsigned)fps,
                        (unsigned)(s_begin_us_acc / s_frame_us_count),
                        (unsigned)(s_submit_us_acc / s_frame_us_count),
                        (unsigned)(s_end_us_acc / s_frame_us_count),
                        (unsigned)(s_light_us_acc / s_frame_us_count),
                        (unsigned)rpe_n64_frame_detach_us(),
                        (unsigned)hal_render_backend_last_tnl_us(),
                        (unsigned)hal_render_backend_last_rdp_us(),
                        (unsigned)hal_render_backend_draw_call_count(),
                        (unsigned)render_diagnostics().submitted);
            s_frame_us_acc = 0u;
            s_frame_us_count = 0u;
            s_begin_us_acc = 0u;
            s_submit_us_acc = 0u;
            s_end_us_acc = 0u;
            s_light_us_acc = 0u;
            s_fps_start_ms = now_ms;
        }

        if ((s_frames % 300u) == 0u) {
            log_heartbeat();
        }

#if RPE_N64_BOOT_FRAMES
        if (s_frames >= (uint32_t)RPE_N64_BOOT_FRAMES) {
            Log_Message(LOG_LEVEL_INFO, "n64_hal", "boot: frame limit reached, shutdown");
            Render_Shutdown();
            Platform_Shutdown();
            halt_forever();
        }
#endif
    }
}
