// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_fs.cpp
//
// Реализация read-only DFS-слоя N64 (контракт — n64_fs.h).
//
// Staging-арена — bump allocator с освобождением «сверху»: типичный сценарий
// (load → parse → free) полностью возвращает память; освобождение блока
// ниже живого соседа оставляет дыру до освобождения верхнего блока. Это
// детерминированно и не требует heap.
#include "hal/n64/n64_fs.h"

#include "core/log/log.h"
#include "hal/n64/n64_bridge.h"

#include <string.h>

namespace n64 {

namespace {

constexpr uint32_t FS_HANDLE_MAGIC = 0x4E363446u; /* "N64F" */

struct StagingBlock
{
    bool used;
    bool live;
    bool detached;
    LinearAllocator::Marker marker;
    uint8_t* data;
    uint32_t size;
};

StagingBlock s_blocks[FS_MAX_OPEN_FILES];
bool s_fs_ready = false;

void set_diag(FsDiag* out_diag, FsDiag diag)
{
    if (out_diag != nullptr) {
        *out_diag = diag;
    }
}

bool ascii_upper(char c, char* out)
{
    if (c >= 'a' && c <= 'z') {
        *out = (char)(c - 'a' + 'A');
        return true;
    }
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
        *out = c;
        return true;
    }
    return false;
}

/** Освобождение верхних пустых блоков (rollback bump-арены). */
void reclaim_top()
{
    bool again = true;
    while (again) {
        again = false;
        int top = -1;
        size_t top_offset = 0u;
        // Верхний блок — максимальный marker.offset СРЕДИ ВСЕХ занятых
        // блоков, включая живые: иначе rollback снёс бы данные живого файла.
        for (uint32_t i = 0u; i < FS_MAX_OPEN_FILES; ++i) {
            const StagingBlock& block = s_blocks[i];
            if (!block.used) {
                continue;
            }
            if (top < 0 || block.marker.offset >= top_offset) {
                top = (int)i;
                top_offset = block.marker.offset;
            }
        }
        if (top < 0 || s_blocks[top].live) {
            break;
        }
        if (!fs_staging_arena().rollback(s_blocks[top].marker)) {
            // Чужой/испорченный marker не должен ломать FS: оставить блок
            // занятым и прекратить reclaim (явная диагностика в логе).
            Log_Message(LOG_LEVEL_ERROR, "n64_fs", "staging rollback rejected (slot %d)", top);
            break;
        }
        s_blocks[top] = StagingBlock{};
        again = true;
    }
}

int find_free_slot()
{
    for (uint32_t i = 0u; i < FS_MAX_OPEN_FILES; ++i) {
        if (!s_blocks[i].used) {
            return (int)i;
        }
    }
    return -1;
}

uint8_t* allocate_block(uint32_t size, int slot)
{
    if (slot < 0 || size == 0u || size > fs_staging_arena().capacity()) {
        return nullptr;
    }
    StagingBlock& block = s_blocks[slot];
    block.marker = fs_staging_arena().mark();
    void* data = fs_staging_arena().allocate(size, 8u);
    if (data == nullptr) {
        block = StagingBlock{};
        return nullptr;
    }
    block.used = true;
    block.live = true;
    block.detached = false;
    block.data = static_cast<uint8_t*>(data);
    block.size = size;
    return block.data;
}

void release_block(int slot)
{
    if (slot < 0 || slot >= (int)FS_MAX_OPEN_FILES || !s_blocks[slot].used) {
        return;
    }
    s_blocks[slot].live = false;
    reclaim_top();
}

bool handle_valid(const FileHandle* handle)
{
    return handle != nullptr && handle->magic == FS_HANDLE_MAGIC &&
           handle->slot < FS_MAX_OPEN_FILES && s_blocks[handle->slot].used &&
           s_blocks[handle->slot].live && !s_blocks[handle->slot].detached;
}

FsDiag prepare_name(const char* path, char* name)
{
    return fs_path_normalize(path, name);
}

FsDiag open_internal(const char* path, FileHandle* out, bool detach, FileData* out_file)
{
    if (!s_fs_ready || !rpe_n64_fs_ready()) {
        return FS_ERR_NOT_AVAILABLE;
    }
    char name[FS_NAME_MAX + 1u];
    const FsDiag path_diag = prepare_name(path, name);
    if (path_diag != FS_OK) {
        Log_Message(LOG_LEVEL_WARNING, "n64_fs", "reject path '%s': %s",
                    (path != nullptr) ? path : "(null)", fs_diag_string(path_diag));
        return path_diag;
    }

    const int32_t file_size = rpe_n64_fs_size(name);
    if (file_size < 0) {
        Log_Message(LOG_LEVEL_WARNING, "n64_fs", "not found: %s", name);
        return FS_ERR_NOT_FOUND;
    }
    if (file_size == 0) {
        Log_Message(LOG_LEVEL_WARNING, "n64_fs", "empty file: %s", name);
        return FS_ERR_EMPTY_FILE;
    }
    if ((uint32_t)file_size > FS_MAX_FILE_BYTES) {
        Log_Message(LOG_LEVEL_ERROR, "n64_fs", "file too large: %s (%d bytes, max %u)", name,
                    (int)file_size, (unsigned)FS_MAX_FILE_BYTES);
        return FS_ERR_TOO_LARGE;
    }

    const int slot = find_free_slot();
    if (slot < 0) {
        return FS_ERR_TOO_MANY_OPEN;
    }
    uint8_t* data = allocate_block((uint32_t)file_size, slot);
    if (data == nullptr) {
        return FS_ERR_ALLOC_FAIL;
    }

    const int32_t read = rpe_n64_fs_read(name, data, (uint32_t)file_size);
    if (read != file_size) {
        Log_Message(LOG_LEVEL_ERROR, "n64_fs", "short read: %s (%d/%d bytes)", name, (int)read,
                    (int)file_size);
        release_block(slot);
        return FS_ERR_READ_FAIL;
    }

    if (detach) {
        if (out_file == nullptr) {
            release_block(slot);
            return FS_ERR_NULL_ARGUMENT;
        }
        s_blocks[slot].detached = true;
        out_file->data = data;
        out_file->size = (uint32_t)file_size;
        if (out != nullptr) {
            *out = FileHandle{};
        }
        return FS_OK;
    }

    if (out == nullptr) {
        release_block(slot);
        return FS_ERR_NULL_ARGUMENT;
    }
    out->magic = FS_HANDLE_MAGIC;
    out->slot = (uint32_t)slot;
    out->size = (uint32_t)file_size;
    out->offset = 0u;
    return FS_OK;
}

}  // namespace

FsDiag fs_path_normalize(const char* path, char* out)
{
    if (out != nullptr) {
        out[0] = '\0';
    }
    if (out == nullptr || path == nullptr) {
        return FS_ERR_NULL_ARGUMENT;
    }
    const size_t length = strlen(path);
    if (length == 0u) {
        return FS_ERR_INVALID_PATH;
    }
    if (length > FS_NAME_MAX) {
        return FS_ERR_INVALID_PATH;
    }
    if (path[0] == '.' || path[length - 1u] == '.') {
        return FS_ERR_INVALID_PATH;
    }

    for (size_t i = 0u; i < length; ++i) {
        char upper = '\0';
        if (!ascii_upper(path[i], &upper)) {
            out[0] = '\0';
            return FS_ERR_INVALID_PATH;
        }
        out[i] = upper;
    }
    out[length] = '\0';

    // DFS имена — bare-файлы; каталоги/`..` в v0.3.0 не используются.
    if (strstr(out, "..") != nullptr || strchr(out, '/') != nullptr ||
        strchr(out, '\\') != nullptr || strchr(out, ':') != nullptr) {
        out[0] = '\0';
        return FS_ERR_INVALID_PATH;
    }
    return FS_OK;
}

bool fs_init()
{
    if (s_fs_ready && rpe_n64_fs_ready()) {
        return true;
    }
    for (uint32_t i = 0u; i < FS_MAX_OPEN_FILES; ++i) {
        s_blocks[i] = StagingBlock{};
    }
    fs_staging_arena().reset();
    s_fs_ready = rpe_n64_fs_init();
    if (!s_fs_ready) {
        Log_Message(LOG_LEVEL_WARNING, "n64_fs",
                    "DFS image not found (ROM без DFS: ассеты недоступны)");
    }
    return s_fs_ready;
}

void fs_shutdown()
{
    for (uint32_t i = 0u; i < FS_MAX_OPEN_FILES; ++i) {
        if (s_blocks[i].used) {
            s_blocks[i].live = false;
        }
        s_blocks[i] = StagingBlock{};
    }
    fs_staging_arena().reset();
    if (s_fs_ready) {
        rpe_n64_fs_shutdown();
    }
    s_fs_ready = false;
}

bool fs_ready()
{
    return s_fs_ready && rpe_n64_fs_ready();
}

size_t fs_staging_used()
{
    return fs_staging_arena().used();
}

FsDiag fs_open(const char* path, FileHandle* out)
{
    if (!s_fs_ready || !rpe_n64_fs_ready()) {
        return FS_ERR_NOT_INITIALIZED;
    }
    if (out == nullptr) {
        return FS_ERR_NULL_ARGUMENT;
    }
    *out = FileHandle{};
    return open_internal(path, out, false, nullptr);
}

FsDiag fs_read(FileHandle* handle, void* dst, uint32_t size, uint32_t* out_read)
{
    if (out_read != nullptr) {
        *out_read = 0u;
    }
    if (!handle_valid(handle)) {
        return FS_ERR_BAD_HANDLE;
    }
    if (size != 0u && dst == nullptr) {
        return FS_ERR_NULL_ARGUMENT;
    }
    StagingBlock& block = s_blocks[handle->slot];
    if (handle->offset > handle->size || size > handle->size - handle->offset) {
        return FS_ERR_TRUNCATED;
    }
    if (size != 0u) {
        memcpy(dst, block.data + handle->offset, size);
    }
    handle->offset += size;
    if (out_read != nullptr) {
        *out_read = size;
    }
    return FS_OK;
}

FsDiag fs_close(FileHandle* handle)
{
    if (!handle_valid(handle)) {
        return FS_ERR_BAD_HANDLE;
    }
    const uint32_t slot = handle->slot;
    handle->magic = 0u;
    handle->slot = 0u;
    handle->size = 0u;
    handle->offset = 0u;
    release_block((int)slot);
    return FS_OK;
}

FsDiag fs_read_range(const char* path, uint32_t offset, void* dst, uint32_t size,
                     uint32_t* out_read)
{
    if (out_read != nullptr) {
        *out_read = 0u;
    }
    FileHandle handle;
    const FsDiag open_diag = fs_open(path, &handle);
    if (open_diag != FS_OK) {
        return open_diag;
    }
    if (offset > handle.size) {
        (void)fs_close(&handle);
        return FS_ERR_TRUNCATED;
    }
    handle.offset = offset;
    const FsDiag read_diag = fs_read(&handle, dst, size, out_read);
    (void)fs_close(&handle);
    return read_diag;
}

FileData fs_load_file(const char* path, FsDiag* out_diag)
{
    set_diag(out_diag, FS_OK);
    FileData file = FileData{};
    const FsDiag diag = open_internal(path, nullptr, true, &file);
    if (diag != FS_OK) {
        set_diag(out_diag, diag);
    }
    return file;
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

    for (uint32_t i = 0u; i < FS_MAX_OPEN_FILES; ++i) {
        StagingBlock& block = s_blocks[i];
        if (block.used && block.live && block.detached && block.data == file->data) {
            block.live = false;
            file->data = nullptr;
            file->size = 0u;
            reclaim_top();
            return true;
        }
    }

    set_diag(out_diag, FS_ERR_FOREIGN_BUFFER);
    return false;
}

const char* fs_diag_string(FsDiag diag)
{
    switch (diag) {
        case FS_OK:
            return "ok";
        case FS_ERR_NOT_INITIALIZED:
            return "not initialized";
        case FS_ERR_NOT_AVAILABLE:
            return "DFS not available";
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
        case FS_ERR_TOO_MANY_OPEN:
            return "too many open files";
        case FS_ERR_ALLOC_FAIL:
            return "staging allocation failed";
        case FS_ERR_READ_FAIL:
            return "read failed";
        case FS_ERR_TRUNCATED:
            return "truncated read";
        case FS_ERR_BAD_HANDLE:
            return "bad handle";
        case FS_ERR_FOREIGN_BUFFER:
            return "foreign buffer";
        case FS_ERR_COUNT:
        default:
            return "unknown";
    }
}

}  // namespace n64
