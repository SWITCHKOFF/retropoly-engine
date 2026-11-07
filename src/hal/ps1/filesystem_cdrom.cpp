// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/ps1/filesystem_cdrom.cpp
//
// Реализация FileSystem_* HAL PS1 (Спринты 13–14): чтение файлов целиком с
// CD-ROM через драйвер BIOS (psxcd.h: CdSearchFile → CdControl(CdlSetloc) →
// CdRead/CdReadSync) — паттерн SDK-примера sound/cdstream.
//
// Зеркало filesystem_pc.cpp по контракту platform.h:
//   - FileSystem_LoadFile: целиком, буфер в куче (malloc), владение у
//     вызывающего; освобождение ТОЛЬКО через FileSystem_FreeFile;
//   - транспорт секторный: буфер округляется ВВЕРХ до 2048 байт/сектор
//     (ISO9660 mode 2 data sectors); FileData.size — точный размер файла;
//   - отказ {nullptr, 0} без краша: нет диска/файла/памяти/ошибка чтения.
//
// CdInit — часть device-lifecycle (вызов из Platform_Init через hook
// hal_ps1_fs_init; двойная инициализация безопасна).
#include "hal/platform.h"

#include "hal/ps1/hal_ps1_internal.h"
#include "core/log/log.h"

// SDK third-party: изоляция от -Wpedantic (§3.3). u_char (typedef из
// sys/types.h) нужен для CdControl-сигнатуры; psxcd.h под namespace-шилдом.
#include <sys/types.h> // typedef u_char (PSn00bSDK libpsn00b/sys/types.h)
#include <psxcd.h>

#include <stdlib.h>
#include <string.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Состояние драйвера BIOS
// ---------------------------------------------------------------------------

static bool s_cd_inited = false;

/** Байт на data-сектор (ISO9660: строго 2048; драйвер BIOS в режиме Speed). */
#define CD_SECTOR_BYTES 2048u

// ---------------------------------------------------------------------------
// Hook (platform_ps1.cpp)
// ---------------------------------------------------------------------------

void hal_ps1_fs_init(void)
{
    if (s_cd_inited) {
        return; // Идемпотентность (повторная Platform_Init).
    }
    CdInit();
    s_cd_inited = true;
    Log_Message(LOG_LEVEL_INFO, "ps1_hal", "fs init: CD-ROM драйвер BIOS (CdInit)");
}

// ---------------------------------------------------------------------------
// Публичный контракт (platform.h)
// ---------------------------------------------------------------------------

FileData FileSystem_LoadFile(const char* path)
{
    FileData result;
    result.data = nullptr;
    result.size = 0u;

    if (!s_cd_inited || path == nullptr || path[0] == '\0') {
        return result; // Безопасный отказ (контракт: NULL/пусто).
    }

    // Поиск файла в ISO9660-дереве диска (позиция + размер на выходе).
    CdlFILE file;
    if (CdSearchFile(&file, path) == 0) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "load: файл не найден на диске (%s)",
                    path);
        return result;
    }
    if (file.size <= 0) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "load: некорректный размер (%s)",
                    path);
        return result;
    }

    // Сектор-выравненный транспорт: буфер кучи (malloc: 4-alignment гарантия
    // MIPS-ABI; MeshAsset/TextureAsset читают файл как «плоский» stream —
    // ровно тот же распределенной-порядок, что PC-поток).
    const uint32_t sector_count = ((uint32_t)file.size + CD_SECTOR_BYTES - 1u) /
                                  CD_SECTOR_BYTES;
    const size_t buffer_size = (size_t)sector_count * (size_t)CD_SECTOR_BYTES;
    uint8_t* buffer = (uint8_t*)malloc(buffer_size);
    if (buffer == nullptr) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "load: heap %zu байт недоступен",
                    buffer_size);
        return result;
    }

    // Установка позиции → CdRead (Speed-режим) → блокирующий sync.
    CdControl(CdlSetloc, (u_char*)&file.pos, 0);
    if (CdRead((int)sector_count, (uint32_t*)buffer, CdlModeSpeed) == 0) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "load: CdRead отказ (%u секторов)",
                    (unsigned)sector_count);
        free(buffer);
        return result;
    }
    if (CdReadSync(0, nullptr) < 0) {
        Log_Message(LOG_LEVEL_ERROR, "ps1_hal", "load: ошибка чтения диска (%s)",
                    path);
        free(buffer);
        return result;
    }

    result.data = buffer;
    result.size = (uint32_t)file.size; // Точный размер (не сектор-выравненный).
    Log_Message(LOG_LEVEL_INFO, "ps1_hal", "load: %s (%u байт, %u секторов)", path,
                (unsigned)result.size, (unsigned)sector_count);
    return result;
}

void FileSystem_FreeFile(FileData* file)
{
    if (file == nullptr) {
        return;
    }
    if (file->data != nullptr) {
        free(file->data); // Пара malloc/FileSystem_FreeFile (контракт владения).
        file->data = nullptr;
    }
    file->size = 0u;
}
