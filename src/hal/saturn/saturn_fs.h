// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_fs.h
//
// Read-only CD-ROM файловая подсистема Saturn (Спринт 23–24, неделя 47).
//
// Ядро не знает про Jo Engine: доступ к CD идёт через FileSource vtable.
// filesystem_saturn.cpp связывает vtable с C-мостом (internal Jo Engine FS),
// host-тесты подставляют fake-источник. Загрузка синхронная: размер,
// выделение из asset heap, точное чтение; при любой ошибке — освобождение
// и 0 байт persistent-памяти.
#pragma once

#include <stdint.h>

#include "hal/platform.h"

namespace saturn {

/** Верхняя граница файла, принимаемого HAL (защита от лживого размера). */
constexpr uint32_t FS_MAX_FILE_BYTES = 8u * 1024u * 1024u;

/** Диагностика файловых операций. Порядок — контракт. */
enum FsDiag
{
    FS_OK = 0,
    FS_ERR_NOT_INITIALIZED,
    FS_ERR_NOT_AVAILABLE,
    FS_ERR_NULL_ARGUMENT,
    FS_ERR_INVALID_PATH,
    FS_ERR_NOT_FOUND,
    FS_ERR_EMPTY_FILE,
    FS_ERR_TOO_LARGE,
    FS_ERR_ALLOC_FAIL,
    FS_ERR_READ_FAIL,
    FS_ERR_FOREIGN_FREE,
    FS_ERR_COUNT
};

/**
 * Источник файлов (vtable). Имена — уже нормализованные bare 8.3.
 *
 * get_size: false — файл отсутствует/недоступен.
 * read: true только при точном чтении size байт; false — ошибка/обрыв.
 */
struct FileSource
{
    bool (*available)(void);
    bool (*get_size)(const char* name, uint32_t* out_size);
    bool (*read)(const char* name, uint8_t* dst, uint32_t size);
};

/**
 * Подключение источника.
 *
 * @param source vtable (время жизни — static); nullptr отключает FS.
 * @return true, если источник подключён и available().
 */
bool fs_init(const FileSource* source);

/** Отключение источника (идемпотентно). */
void fs_shutdown(void);

/** @return Текущий источник или nullptr. */
const FileSource* fs_source(void);

/** @return true, если источник подключён и доступен. */
bool fs_available(void);

/**
 * Загрузка файла целиком.
 *
 * @param path Путь (валидируется path_normalize).
 * @param out_diag Диагностика (NULL допустим).
 * @return {data,size}; при ошибке {nullptr,0}, heap не изменён.
 */
FileData fs_load_file(const char* path, FsDiag* out_diag);

/**
 * Освобождение буфера, выданного fs_load_file().
 *
 * @param file Результат загрузки (NULL безопасен).
 * @param out_diag Диагностика (NULL допустим).
 * @return true при успехе/уже пустом буфере.
 */
bool fs_free_file(FileData* file, FsDiag* out_diag);

/** @return Статическая строка диагностического кода. */
const char* fs_diag_string(FsDiag diag);

}  // namespace saturn
