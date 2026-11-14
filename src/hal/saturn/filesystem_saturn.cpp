// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/filesystem_saturn.cpp
//
// Публичный FileSystem_* Saturn и привязка FS-ядра к C-мосту Jo Engine.
// ISO9660/8.3-валидация и выделение памяти — в saturn_fs.cpp (host-тесты).
#include "hal/platform.h"

#include "core/log/log.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/saturn_fs.h"
#include "hal/saturn/saturn_hal_internal.h"

namespace saturn {

namespace {

bool bridge_available(void)
{
    return rpe_saturn_fs_available();
}

bool bridge_get_size(const char* name, uint32_t* out_size)
{
    return rpe_saturn_fs_get_size(name, out_size);
}

bool bridge_read(const char* name, uint8_t* dst, uint32_t size)
{
    return rpe_saturn_fs_read(name, dst, size);
}

const FileSource kSaturnFileSource = {bridge_available, bridge_get_size, bridge_read};

}  // namespace

bool filesystem_init(void)
{
    const bool ok = fs_init(&kSaturnFileSource);
    if (!ok) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal",
                    "CD-ROM filesystem unavailable (ISO9660 / 0.BIN not found)");
    }
    return ok;
}

void filesystem_shutdown(void)
{
    fs_shutdown();
}

}  // namespace saturn

FileData FileSystem_LoadFile(const char* path)
{
    saturn::FsDiag diag = saturn::FS_OK;
    FileData file = saturn::fs_load_file(path, &diag);
    if (diag != saturn::FS_OK) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "FileSystem_LoadFile('%s') failed: %s",
                    (path != nullptr) ? path : "(null)", saturn::fs_diag_string(diag));
    }
    return file;
}

void FileSystem_FreeFile(FileData* file)
{
    saturn::FsDiag diag = saturn::FS_OK;
    if (!saturn::fs_free_file(file, &diag) && diag != saturn::FS_ERR_NULL_ARGUMENT) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "FileSystem_FreeFile failed: %s",
                    saturn::fs_diag_string(diag));
    }
}
