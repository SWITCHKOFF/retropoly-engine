// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/pc/filesystem_pc.cpp
//
// HAL PC: файловая система (sprint_05_06.md, неделя 12). Нормативный API
// §3 engineering doc v1.43: загрузка файла целиком в непрерывный буфер.
//
// Контракт владения (см. platform.h): буфер выделяет SDL-аллокатор
// (SDL_LoadFile) и освобождается только через FileSystem_FreeFile.
// Отказы (нет файла / пустой файл / превышение 4 ГиБ / мусорный путь)
// возвращают {nullptr, 0} без краша.
#include "hal/platform.h"

#include <SDL3/SDL.h>

#include <stdint.h>

FileData FileSystem_LoadFile(const char* path)
{
    FileData result;
    result.data = nullptr;
    result.size = 0u;

    if (path == nullptr || path[0] == '\0') {
        return result;
    }

    size_t size = 0u;
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr) {
        // Файл не найден / не читается: ok=false без краша (контракт §3).
        return result;
    }
    if (size == 0u || size > 0xFFFFFFFFu) {
        // Пустой файл или файл > UINT32_MAX не выражается FileData-контрактом.
        SDL_free(data);
        return result;
    }

    result.data = (uint8_t*)data;
    result.size = (uint32_t)size;
    return result;
}

void FileSystem_FreeFile(FileData* file)
{
    if (file == nullptr) {
        return;
    }
    SDL_free(file->data);
    file->data = nullptr;
    file->size = 0u;
}
