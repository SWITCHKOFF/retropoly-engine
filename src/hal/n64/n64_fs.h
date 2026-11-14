// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_fs.h
//
// Read-only DFS (DragonFS/mkdfs) файловая подсистема N64 (S41–42).
//
// libdragon не включается: источник — C-мост (dfs_open/read/close в
// n64_bridge.c), host-тесты подменяют его fake_n64_bridge.cpp.
//
// Контракты:
//   - имена — bare UPPERCASE (8.3-стиль существующих ассетов: CUBE.MSH,
//     TEX0.TEX); строчные нормализуются, host-пути/`..`/`/` отвергаются;
//   - размер проверяется ДО чтения; чтение — точное (short read = ошибка);
//   - буферы выдаются из отдельной staging-арены (128 KiB), НЕ из heap;
//   - open/read/close и load/free не оставляют занятой памяти при ошибке.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/n64/n64_memory.h"
#include "hal/platform.h"

namespace n64 {

/** Максимальный размер файла, принимаемого HAL (staging-арена). */
constexpr uint32_t FS_MAX_FILE_BYTES = FS_STAGING_BYTES;

/** Максимум одновременно открытых/загруженных файлов. */
constexpr uint32_t FS_MAX_OPEN_FILES = 4u;

/** Максимальная длина нормализованного имени без NUL. */
constexpr size_t FS_NAME_MAX = 31u;

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
    FS_ERR_TOO_MANY_OPEN,
    FS_ERR_ALLOC_FAIL,
    FS_ERR_READ_FAIL,
    FS_ERR_TRUNCATED,
    FS_ERR_BAD_HANDLE,
    FS_ERR_FOREIGN_BUFFER,
    FS_ERR_COUNT
};

/** Открытый read-only файл (непрозрачная структура; magic — валидация). */
struct FileHandle
{
    uint32_t magic;
    uint32_t slot;
    uint32_t size;
    uint32_t offset;
};

/**
 * Инициализация DFS (bridge dfs_init). Идемпотентна.
 *
 * @return true — DFS найдена и доступна; false — ROM без DFS (не крах).
 */
bool fs_init();

/** Закрытие всех файлов и сброс staging-арены (идемпотентно). */
void fs_shutdown();

/** true — fs_init выполнен и bridge сообщает готовность. */
bool fs_ready();

/** @return Занятые байты staging-арены (для профилировщика). */
size_t fs_staging_used();

/**
 * Нормализация имени в DFS-имя верхнего регистра.
 *
 * @param path Вход (NUL-терминированный; NULL — ошибка).
 * @param out Выход: минимум FS_NAME_MAX+1 байт.
 * @return FS_OK либо детерминированный код.
 */
FsDiag fs_path_normalize(const char* path, char* out);

/**
 * Открыть файл: размер проверяется, содержимое читается в staging-блок.
 *
 * @param path Путь (нормализуется).
 * @param out Выход: валидный handle только при FS_OK.
 * @return FS_OK / FS_ERR_*.
 */
FsDiag fs_open(const char* path, FileHandle* out);

/**
 * Точное чтение из открытого файла (без частичных успехов).
 *
 * @param handle Открытый handle.
 * @param dst Буфер назначения (не NULL при size > 0).
 * @param size Число байт.
 * @param out_read Выход: прочитано (size) или 0 (NULL допустим).
 * @return FS_OK / FS_ERR_TRUNCATED / FS_ERR_BAD_HANDLE / FS_ERR_NULL_ARGUMENT.
 */
FsDiag fs_read(FileHandle* handle, void* dst, uint32_t size, uint32_t* out_read);

/** Закрыть handle (освобождает staging-блок; повторно — FS_ERR_BAD_HANDLE). */
FsDiag fs_close(FileHandle* handle);

/**
 * Прочитать диапазон файла без ручного open/close.
 *
 * @return FS_OK при точном чтении; иначе FS_ERR_*.
 */
FsDiag fs_read_range(const char* path, uint32_t offset, void* dst, uint32_t size,
                     uint32_t* out_read);

/**
 * Загрузка файла целиком (буфер — staging-арена).
 *
 * @param path Путь.
 * @param out_diag Диагностика (NULL допустим).
 * @return {data,size}; при ошибке {nullptr,0}, staging не изменён.
 */
FileData fs_load_file(const char* path, FsDiag* out_diag);

/**
 * Освобождение буфера fs_load_file().
 *
 * @return true при успехе/уже пустом буфере; false — чужой указатель.
 */
bool fs_free_file(FileData* file, FsDiag* out_diag);

/** @return Статическая строка диагностического кода. */
const char* fs_diag_string(FsDiag diag);

}  // namespace n64
