// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_fs.cpp
//
// Реализация read-only FS-ядра Saturn (контракты — saturn_fs.h).
#include "hal/saturn/saturn_fs.h"

#include "core/log/log.h"
#include "hal/saturn/saturn_fs_path.h"
#include "hal/saturn/saturn_memory.h"

namespace saturn {

namespace {

const FileSource* s_source = nullptr;

void set_diag(FsDiag* out_diag, FsDiag diag)
{
    if (out_diag != nullptr) {
        *out_diag = diag;
    }
}

}  // namespace

bool fs_init(const FileSource* source)
{
    if (source == nullptr || source->available == nullptr || source->get_size == nullptr ||
        source->read == nullptr) {
        s_source = nullptr;
        return false;
    }
    if (!source->available()) {
        s_source = source;
        return false;
    }
    s_source = source;
    return true;
}

void fs_shutdown(void)
{
    s_source = nullptr;
}

const FileSource* fs_source(void)
{
    return s_source;
}

bool fs_available(void)
{
    return s_source != nullptr && s_source->available != nullptr && s_source->available();
}

FileData fs_load_file(const char* path, FsDiag* out_diag)
{
    FileData result = {nullptr, 0u};
    set_diag(out_diag, FS_OK);

    if (s_source == nullptr) {
        set_diag(out_diag, FS_ERR_NOT_INITIALIZED);
        return result;
    }
    if (!s_source->available()) {
        set_diag(out_diag, FS_ERR_NOT_AVAILABLE);
        return result;
    }

    char name[SATURN_FS_NAME_MAX + 1u];
    const PathDiag path_diag = path_normalize(path, name);
    if (path_diag != PATH_OK) {
        Log_Message(LOG_LEVEL_WARNING, "saturn_hal", "fs reject '%s': %s",
                    (path != nullptr) ? path : "(null)", path_diag_string(path_diag));
        set_diag(out_diag, FS_ERR_INVALID_PATH);
        return result;
    }

    uint32_t file_size = 0u;
    if (!s_source->get_size(name, &file_size)) {
        Log_Message(LOG_LEVEL_WARNING, "saturn_hal", "fs file not found: %s", name);
        set_diag(out_diag, FS_ERR_NOT_FOUND);
        return result;
    }
    if (file_size == 0u) {
        Log_Message(LOG_LEVEL_WARNING, "saturn_hal", "fs empty file: %s", name);
        set_diag(out_diag, FS_ERR_EMPTY_FILE);
        return result;
    }
    if (file_size > FS_MAX_FILE_BYTES) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "fs file too large: %s (%u bytes)", name,
                    static_cast<unsigned>(file_size));
        set_diag(out_diag, FS_ERR_TOO_LARGE);
        return result;
    }

    void* data = asset_heap().allocate(file_size, 8u);
    if (data == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "fs alloc failed: %s (%u bytes): %s", name,
                    static_cast<unsigned>(file_size), AssetHeap::diag_string(asset_heap().diag()));
        set_diag(out_diag, FS_ERR_ALLOC_FAIL);
        return result;
    }

    if (!s_source->read(name, static_cast<uint8_t*>(data), file_size)) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "fs read failed: %s (%u bytes)", name,
                    static_cast<unsigned>(file_size));
        asset_heap().free(data);
        set_diag(out_diag, FS_ERR_READ_FAIL);
        return result;
    }

    result.data = static_cast<uint8_t*>(data);
    result.size = file_size;
    set_diag(out_diag, FS_OK);
    return result;
}

bool fs_free_file(FileData* file, FsDiag* out_diag)
{
    set_diag(out_diag, FS_OK);
    if (file == nullptr) {
        set_diag(out_diag, FS_ERR_NULL_ARGUMENT);
        return false;
    }
    if (file->data == nullptr) {
        file->size = 0u;
        return true;
    }

    void* data = file->data;
    file->data = nullptr;
    file->size = 0u;

    if (!asset_heap().free(data)) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "fs free failed: %s",
                    AssetHeap::diag_string(asset_heap().diag()));
        set_diag(out_diag, FS_ERR_FOREIGN_FREE);
        return false;
    }
    return true;
}

const char* fs_diag_string(FsDiag diag)
{
    switch (diag) {
        case FS_OK:
            return "ok";
        case FS_ERR_NOT_INITIALIZED:
            return "not initialized";
        case FS_ERR_NOT_AVAILABLE:
            return "source not available";
        case FS_ERR_NULL_ARGUMENT:
            return "null argument";
        case FS_ERR_INVALID_PATH:
            return "invalid path";
        case FS_ERR_NOT_FOUND:
            return "not found";
        case FS_ERR_EMPTY_FILE:
            return "empty file";
        case FS_ERR_TOO_LARGE:
            return "file too large";
        case FS_ERR_ALLOC_FAIL:
            return "allocation failed";
        case FS_ERR_READ_FAIL:
            return "read failed";
        case FS_ERR_FOREIGN_FREE:
            return "foreign free";
        case FS_ERR_COUNT:
        default:
            return "unknown";
    }
}

}  // namespace saturn
