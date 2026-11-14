// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_assets.cpp
//
// Реализация boot-загрузки ассетов N64 из DFS (контракт — n64_assets.h).
#include "hal/n64/n64_assets.h"

#include "core/log/log.h"
#include "hal/n64/n64_memory.h"

namespace n64 {

namespace {

BootAssets s_boot_assets;

void set_asset_error(uint32_t* out_error, uint32_t value)
{
    if (out_error != nullptr) {
        *out_error = value;
    }
}

bool load_mesh(const char* path, MeshAsset** out, uint32_t* out_error)
{
    set_asset_error(out_error, ASSET_ERR_OK);
    if (out == nullptr) {
        set_asset_error(out_error, ASSET_ERR_NULL_ARGUMENT);
        return false;
    }
    *out = nullptr;

    FsDiag fs_diag = FS_OK;
    FileData file = fs_load_file(path, &fs_diag);
    if (file.data == nullptr) {
        Log_Message(LOG_LEVEL_WARNING, "n64_asset", "%s: load failed (%s)", path,
                    fs_diag_string(fs_diag));
        set_asset_error(out_error, ASSET_ERR_TRUNCATED_HEADER);
        return false;
    }

    const LinearAllocator::Marker marker = asset_arena().mark();
    MeshAsset* mesh = nullptr;
    uint32_t error = ASSET_ERR_OK;
    const bool ok =
        mesh_validate_and_load(file.data, file.size, &asset_arena().allocator(), &mesh, &error);
    if (!ok) {
        (void)asset_arena().rollback(marker);
        Log_Message(LOG_LEVEL_ERROR, "n64_asset", "%s: invalid .mesh (asset error %u)", path,
                    static_cast<unsigned>(error));
    } else {
        asset_arena().note_used();
        *out = mesh;
    }
    set_asset_error(out_error, error);
    (void)fs_free_file(&file, nullptr);
    return ok;
}

bool load_texture(const char* path, TextureAsset** out, uint32_t* out_error)
{
    set_asset_error(out_error, ASSET_ERR_OK);
    if (out == nullptr) {
        set_asset_error(out_error, ASSET_ERR_NULL_ARGUMENT);
        return false;
    }
    *out = nullptr;

    FsDiag fs_diag = FS_OK;
    FileData file = fs_load_file(path, &fs_diag);
    if (file.data == nullptr) {
        Log_Message(LOG_LEVEL_WARNING, "n64_asset", "%s: load failed (%s)", path,
                    fs_diag_string(fs_diag));
        set_asset_error(out_error, ASSET_ERR_TRUNCATED_HEADER);
        return false;
    }

    const LinearAllocator::Marker marker = asset_arena().mark();
    TextureAsset* texture = nullptr;
    uint32_t error = ASSET_ERR_OK;
    const bool ok = texture_validate_and_load(file.data, file.size, &asset_arena().allocator(),
                                              &texture, &error);
    if (!ok) {
        (void)asset_arena().rollback(marker);
        Log_Message(LOG_LEVEL_ERROR, "n64_asset", "%s: invalid .tex (asset error %u)", path,
                    static_cast<unsigned>(error));
    } else {
        asset_arena().note_used();
        *out = texture;
    }
    set_asset_error(out_error, error);
    (void)fs_free_file(&file, nullptr);
    return ok;
}

}  // namespace

void assets_shutdown()
{
    s_boot_assets = BootAssets{};
}

const BootAssets& boot_assets()
{
    return s_boot_assets;
}

bool assets_load_boot()
{
    s_boot_assets = BootAssets{};

    MeshAsset* mesh = nullptr;
    uint32_t mesh_error = ASSET_ERR_OK;
    FsDiag fs_diag = FS_OK;
    FileData mesh_file = fs_load_file("CUBE.MSH", &fs_diag);
    if (mesh_file.data != nullptr) {
        s_boot_assets.mesh_file_bytes = mesh_file.size;
        const LinearAllocator::Marker marker = asset_arena().mark();
        if (mesh_validate_and_load(mesh_file.data, mesh_file.size, &asset_arena().allocator(),
                                   &mesh, &mesh_error)) {
            asset_arena().note_used();
            s_boot_assets.mesh = mesh;
            s_boot_assets.mesh_ok = true;
            Log_Message(LOG_LEVEL_INFO, "n64_asset", "CUBE.MSH: %u bytes, %u vertices, %u indices",
                        static_cast<unsigned>(mesh_file.size),
                        static_cast<unsigned>(mesh->vertex_count),
                        static_cast<unsigned>(mesh->index_count));
        } else {
            (void)asset_arena().rollback(marker);
            Log_Message(LOG_LEVEL_ERROR, "n64_asset", "CUBE.MSH invalid (asset error %u)",
                        static_cast<unsigned>(mesh_error));
        }
        (void)fs_free_file(&mesh_file, nullptr);
    } else {
        Log_Message(LOG_LEVEL_WARNING, "n64_asset", "CUBE.MSH not loaded from DFS (%s)",
                    fs_diag_string(fs_diag));
    }

    TextureAsset* texture = nullptr;
    uint32_t texture_error = ASSET_ERR_OK;
    FileData tex_file = fs_load_file("TEX0.TEX", &fs_diag);
    if (tex_file.data != nullptr) {
        s_boot_assets.texture_file_bytes = tex_file.size;
        const LinearAllocator::Marker marker = asset_arena().mark();
        if (texture_validate_and_load(tex_file.data, tex_file.size, &asset_arena().allocator(),
                                      &texture, &texture_error)) {
            asset_arena().note_used();
            s_boot_assets.texture = texture;
            s_boot_assets.texture_ok = true;
            Log_Message(LOG_LEVEL_INFO, "n64_asset", "TEX0.TEX: %u bytes, %ux%u",
                        static_cast<unsigned>(tex_file.size), static_cast<unsigned>(texture->width),
                        static_cast<unsigned>(texture->height));
        } else {
            (void)asset_arena().rollback(marker);
            Log_Message(LOG_LEVEL_ERROR, "n64_asset", "TEX0.TEX invalid (asset error %u)",
                        static_cast<unsigned>(texture_error));
        }
        (void)fs_free_file(&tex_file, nullptr);
    } else {
        Log_Message(LOG_LEVEL_WARNING, "n64_asset", "TEX0.TEX not loaded from DFS (%s)",
                    fs_diag_string(fs_diag));
    }

    Log_Message(LOG_LEVEL_INFO, "n64_asset", "boot assets: mesh=%u tex=%u arena=%u/%u bytes",
                s_boot_assets.mesh_ok ? 1u : 0u, s_boot_assets.texture_ok ? 1u : 0u,
                static_cast<unsigned>(asset_arena().used()),
                static_cast<unsigned>(asset_arena().capacity()));
    return s_boot_assets.mesh_ok && s_boot_assets.texture_ok;
}

bool assets_load_mesh(const char* path, MeshAsset** out, uint32_t* out_error)
{
    return load_mesh(path, out, out_error);
}

bool assets_load_texture(const char* path, TextureAsset** out, uint32_t* out_error)
{
    return load_texture(path, out, out_error);
}

}  // namespace n64
