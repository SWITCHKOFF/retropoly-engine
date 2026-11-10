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
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/saturn_memory.h"

namespace {

constexpr uint32_t kSelfTestScratchBytes = 160u * 1024u;

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
    void* scratch = saturn::asset_heap().allocate(kSelfTestScratchBytes, 8u);
    if (scratch == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_asset", "CD self-test: scratch alloc failed (%s)",
                    saturn::AssetHeap::diag_string(saturn::asset_heap().diag()));
        return;
    }
    LinearAllocator alloc(scratch, kSelfTestScratchBytes);
    bool mesh_ok = false;
    bool texture_ok = false;

    FileData mesh_file = FileSystem_LoadFile("CUBE.MSH");
    if (mesh_file.data != nullptr) {
        log_asset_file("CUBE.MSH", mesh_file);
        MeshAsset* mesh = nullptr;
        uint32_t error = ASSET_ERR_OK;
        if (mesh_validate_and_load(mesh_file.data, mesh_file.size, &alloc, &mesh, &error)) {
            mesh_ok = true;
            Log_Message(LOG_LEVEL_INFO, "saturn_asset", "CUBE.MSH valid: %u vertices, %u indices",
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

    // Строка должна уместиться в 40 колонок VDP2-текста (строка 27).
    Log_Message(LOG_LEVEL_INFO, "asset", "CD mesh=%u tex=%u heap=%u", mesh_ok ? 1u : 0u,
                texture_ok ? 1u : 0u, static_cast<unsigned>(saturn::asset_heap().used()));
}

}  // namespace

extern "C" void rpe_saturn_frame_tick(void)
{
    if (!Platform_PumpEvents()) {
        return;  // На Saturn нет SDL_QUIT; контракт на будущее.
    }
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

    Log_Message(LOG_LEVEL_INFO, "saturn_hal", "boot complete: clear frame loop");
    // Self-test идёт последним: его итоговая строка остаётся на VDP2-экране
    // и служит визуальным подтверждением чтения .mesh/.tex с CD.
    boot_asset_self_test();

    rpe_saturn_core_run();  // не возвращается: VBlank sync + callbacks
    for (;;) {
    }
}
