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

namespace {

constexpr uint32_t kSelfTestScratchBytes = 160u * 1024u;
constexpr uint32_t kMeshArenaBytes = 64u * 1024u;
constexpr uint32_t kMetricsPeriodFrames = 300u;

LinearAllocator s_mesh_alloc;
void* s_mesh_block = nullptr;
MeshAsset* s_boot_mesh = nullptr;
Mat4 s_mvp;
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
            } else {
                Log_Message(LOG_LEVEL_ERROR, "saturn_asset", "TEX0.TEX invalid (asset error %u)",
                            static_cast<unsigned>(error));
            }
            FileSystem_FreeFile(&tex_file);
        } else {
            Log_Message(LOG_LEVEL_WARNING, "saturn_asset", "TEX0.TEX not loaded from CD");
        }

        if (!saturn::asset_heap().free(scratch)) {
            Log_Message(LOG_LEVEL_ERROR, "saturn_asset", "self-test scratch free failed");
        }
    }

    saturn::parallel_workload_init(s_boot_mesh);

    // Строка должна уместиться в 40 колонок VDP2-текста (строка 27).
    Log_Message(LOG_LEVEL_INFO, "asset", "CD mesh=%u tex=%u heap=%u", mesh_ok ? 1u : 0u,
                texture_ok ? 1u : 0u, static_cast<unsigned>(saturn::asset_heap().used()));
}

void build_static_mvp(void)
{
    Mat4 view;
    Mat4 projection;
    mat4_look_at(&view, vec3_make(0, fixed_from_int(3), fixed_from_int(7)), vec3_make(0, 0, 0),
                 vec3_make(0, FIXED_ONE, 0));
    const fixed_t fov = fixed_mul(FIXED_PI, fixed_div(fixed_from_int(70), fixed_from_int(180)));
    mat4_perspective(&projection, fov, fixed_div(fixed_from_int(320), fixed_from_int(224)),
                     fixed_from_int(1), fixed_from_int(100));
    mat4_multiply(&s_mvp, &projection, &view);
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

extern "C" void rpe_saturn_frame_tick(void)
{
    if (!Platform_PumpEvents()) {
        return;  // На Saturn нет SDL_QUIT; контракт на будущее.
    }

    // S25–26: параллельная подготовка вершин/command list. Slave пишет
    // только shared Work RAM; VDP1/VDP2 не трогает, поэтому VBlank-окно
    // для VRAM не нарушается.
    const saturn::ParallelFrameResult frame = saturn::parallel_workload_run_frame(&s_mvp, 20u);
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

    Render_BeginFrame();
    Render_Clear(0u, 0u, 32u);  // golden clear (0,0,32) → VDP2 0x9000
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

    if (!rpe_saturn_add_frame_callback(rpe_saturn_frame_tick)) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "frame callback registration failed");
        for (;;) {
        }
    }

    // Self-test идёт до benchmark: CUBE.MSH остаётся в persistent-арене и
    // становится входом параллельного workload.
    boot_asset_self_test();
    build_static_mvp();

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

    Log_Message(LOG_LEVEL_INFO, "saturn_hal", "boot complete: dual-CPU prep loop");
    rpe_saturn_core_run();  // не возвращается: VBlank sync + callbacks
    for (;;) {
    }
}
