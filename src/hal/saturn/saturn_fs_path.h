// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_fs_path.h
//
// Валидация/нормализация имён CD-файлов Saturn (Спринт 23–24, неделя 47).
//
// ISO9660 на диске хранит имена в 8.3-форме с суффиксом «;1». Jo Engine
// скрывает суффикс и сравнивает ровно 12 символов, поэтому HAL принимает
// ТОЛЬКО bare-имена: [A-Z0-9]{1,8} (.[A-Z0-9]{1,3})?; строчные буквы
// нормализуются в верхний регистр. Host-пути, разделители, `;1`, `..`,
// буквы диска и wildcard'ы отвергаются. Модуль host-тестируемый.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace saturn {

/** Максимальная длина имени без NUL (8.3). */
constexpr size_t SATURN_FS_NAME_MAX = 12u;

/** Диагностика валидации пути. Порядок — контракт. */
enum PathDiag
{
    PATH_OK = 0,
    PATH_ERR_NULL_ARGUMENT,
    PATH_ERR_EMPTY,
    PATH_ERR_TOO_LONG,
    PATH_ERR_HOST_PATH,
    PATH_ERR_INVALID_CHARACTER,
    PATH_ERR_INVALID_DOT,
    PATH_ERR_INVALID_83,
    PATH_ERR_COUNT
};

/**
 * Нормализация пути в bare 8.3-имя верхнего регистра.
 *
 * @param path Входной путь (NUL-терминированный).
 * @param out Выход: буфер минимум SATURN_FS_NAME_MAX+1 байт; при ошибке
 *            записывается пустая строка (если out != nullptr).
 * @return PATH_OK либо детерминированный код ошибки.
 */
PathDiag path_normalize(const char* path, char* out);

/** @return Статическая строка диагностического кода. */
const char* path_diag_string(PathDiag diag);

}  // namespace saturn
