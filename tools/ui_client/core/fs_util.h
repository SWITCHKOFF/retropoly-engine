// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/fs_util.h
//
// Файловые/путевые утилиты UI client. Все проверки путей — лексические и
// детерминированные: проект не может ссылаться за собственный корень.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace rpe {
namespace ui {

/** @return true, если путь существует (файл или каталог). */
bool pathExists(const std::string& path);
/** @return true, если путь — каталог. */
bool isDirectory(const std::string& path);
/** @return true, если путь — обычный файл. */
bool isRegularFile(const std::string& path);

/** Размер файла в байтах или 0, если файл недоступен. */
uint64_t fileSize(const std::string& path);

/** Прочитать текстовый файл целиком. */
bool readTextFile(const std::string& path, std::string* out, std::string* error);

/** Записать файл атомарно (tmp + rename); создать родительские каталоги. */
bool writeTextFileAtomic(const std::string& path, const std::string& content, std::string* error);

/** Скопировать файл; родительские каталоги создаются. */
bool copyFile(const std::string& source, const std::string& destination, std::string* error);

/** Создать каталог и всех родителей. */
bool createDirectories(const std::string& path, std::string* error);

/** Удалить файл; отсутствие файла — не ошибка. */
bool removeFile(const std::string& path, std::string* error);

/** @return элементы каталога, отсортированные по имени. */
std::vector<std::string> listDirectorySorted(const std::string& path);

/** Абсолютный путь (относительно текущего каталога). */
std::string absolutePath(const std::string& path);

/** Склеить два пути через '/'. */
std::string joinPath(const std::string& left, const std::string& right);

/** Последний компонент пути. */
std::string fileName(const std::string& path);
/** Имя без последнего расширения. */
std::string fileStem(const std::string& path);
/** Расширение с точкой в нижнем регистре (".png") или пустая строка. */
std::string fileExtension(const std::string& path);
/** Родительский каталог ("" для корня/без родителя). */
std::string parentPath(const std::string& path);

/** Нормализовать разделители в '/', убрать "./" и повторные '/'. */
std::string normalizePath(const std::string& path);

/** @return true для POSIX-абсолютного или Windows drive/UNC пути. */
bool isAbsolutePath(const std::string& path);

/**
 * Безопасный относительный путь внутри проекта.
 *
 * Запрещены: пустой путь, абсолютные пути, drive/UNC, компоненты "..",
 * управляющие символы. "." и повторные разделители нормализуются.
 */
bool isSafeRelativePath(const std::string& path);

/** @return true, если child находится внутри root (после нормализации). */
bool pathIsInside(const std::string& root, const std::string& child);

/** Создать свободное имя: при коллизии добавить _1, _2, ... */
std::string uniquePath(const std::string& path);

/** Строка в нижнем регистре (ASCII). */
std::string toLowerAscii(const std::string& value);
/** Убрать пробелы по краям. */
std::string trim(const std::string& value);

/** Идентификатор CMake-цели: [A-Za-z0-9_-], не пустой. */
std::string sanitizeIdentifier(const std::string& value);
/** Безопасное имя файла без разделителей пути и управляющих символов. */
std::string sanitizeFileName(const std::string& value);

/** Домашний каталог пользователя ("" если неизвестен). */
std::string homeDirectory();

/** Каталог исполняемого файла ("" если неизвестен). */
std::string executableDirectory();

/**
 * Каталог пользовательской конфигурации UI client.
 *
 * RPE_UI_CONFIG_DIR имеет приоритет (CI/smoke/E2E); иначе:
 *   Linux:   $XDG_CONFIG_HOME/retro-engine-ui  или ~/.config/retro-engine-ui
 *   macOS:   ~/Library/Application Support/RetroPolyEngine
 *   Windows: %APPDATA%/RetroPolyEngine
 */
std::string configDirectory();

/** Временный каталог процесса. */
std::string tempDirectory();

/** Человекочитаемый размер (B/KiB/MiB), детерминированный. */
std::string formatBytes(uint64_t bytes);

}  // namespace ui
}  // namespace rpe
