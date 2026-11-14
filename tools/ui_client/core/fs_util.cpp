// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/fs_util.cpp
#include "fs_util.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace rpe {
namespace ui {

namespace fs = std::filesystem;

namespace {

std::string errorString(const std::error_code& ec, const std::string& fallback)
{
    const std::string message = ec.message();
    return message.empty() ? fallback : message;
}

bool hasWindowsDrive(const std::string& path)
{
    return path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) != 0 &&
           path[1] == ':';
}

} // namespace

bool pathExists(const std::string& path)
{
    std::error_code ec;
    const bool exists = fs::exists(fs::u8path(path), ec);
    return exists && !ec;
}

bool isDirectory(const std::string& path)
{
    std::error_code ec;
    const bool result = fs::is_directory(fs::u8path(path), ec);
    return result && !ec;
}

bool isRegularFile(const std::string& path)
{
    std::error_code ec;
    const bool result = fs::is_regular_file(fs::u8path(path), ec);
    return result && !ec;
}

uint64_t fileSize(const std::string& path)
{
    std::error_code ec;
    const uintmax_t size = fs::file_size(fs::u8path(path), ec);
    if (ec) return 0;
    return static_cast<uint64_t>(size);
}

bool readTextFile(const std::string& path, std::string* out, std::string* error)
{
    if (out == nullptr) return false;
    std::ifstream input(fs::u8path(path), std::ios::binary);
    if (!input) {
        if (error != nullptr) *error = "не удалось открыть файл: " + path;
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (!input.good() && !input.eof()) {
        if (error != nullptr) *error = "ошибка чтения файла: " + path;
        return false;
    }
    *out = std::move(content);
    return true;
}

bool createDirectories(const std::string& path, std::string* error)
{
    if (path.empty()) return true;
    std::error_code ec;
    fs::create_directories(fs::u8path(path), ec);
    if (ec) {
        if (error != nullptr)
            *error =
                "не удалось создать каталог: " + path + " (" + errorString(ec, "ошибка ФС") + ")";
        return false;
    }
    return true;
}

bool writeTextFileAtomic(const std::string& path, const std::string& content, std::string* error)
{
    const std::string parent = parentPath(path);
    if (!parent.empty() && !createDirectories(parent, error)) return false;
    const std::string temp = path + ".tmp";
    {
        std::ofstream output(fs::u8path(temp), std::ios::binary | std::ios::trunc);
        if (!output) {
            if (error != nullptr) *error = "не удалось записать файл: " + temp;
            return false;
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.flush();
        if (!output.good()) {
            if (error != nullptr) *error = "ошибка записи файла: " + temp;
            return false;
        }
    }
    std::error_code ec;
    fs::rename(fs::u8path(temp), fs::u8path(path), ec);
    if (ec) {
        // Windows rename не перезаписывает существующий файл.
        fs::remove(fs::u8path(path), ec);
        ec.clear();
        fs::rename(fs::u8path(temp), fs::u8path(path), ec);
    }
    if (ec) {
        if (error != nullptr)
            *error =
                "не удалось заменить файл: " + path + " (" + errorString(ec, "ошибка ФС") + ")";
        fs::remove(fs::u8path(temp), ec);
        return false;
    }
    return true;
}

bool copyFile(const std::string& source, const std::string& destination, std::string* error)
{
    const std::string parent = parentPath(destination);
    if (!parent.empty() && !createDirectories(parent, error)) return false;
    std::error_code ec;
    fs::copy_file(fs::u8path(source), fs::u8path(destination), fs::copy_options::overwrite_existing,
                  ec);
    if (ec) {
        if (error != nullptr)
            *error = "не удалось скопировать " + source + " → " + destination + " (" +
                     errorString(ec, "ошибка ФС") + ")";
        return false;
    }
    return true;
}

bool removeFile(const std::string& path, std::string* error)
{
    std::error_code ec;
    fs::remove(fs::u8path(path), ec);
    if (ec) {
        if (error != nullptr) *error = "не удалось удалить файл: " + path;
        return false;
    }
    return true;
}

std::vector<std::string> listDirectorySorted(const std::string& path)
{
    std::vector<std::string> entries;
    std::error_code ec;
    for (fs::directory_iterator it(fs::u8path(path), ec), end; !ec && it != end; it.increment(ec)) {
        entries.push_back(it->path().filename().u8string());
    }
    std::sort(entries.begin(), entries.end());
    return entries;
}

std::string absolutePath(const std::string& path)
{
    std::error_code ec;
    const fs::path result = fs::absolute(fs::u8path(path), ec);
    if (ec) return path;
    return normalizePath(result.lexically_normal().u8string());
}

std::string normalizePath(const std::string& path)
{
    std::string result;
    result.reserve(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        if (c == '\\') c = '/';
        if (c == '/' && !result.empty() && result.back() == '/') continue;
        result.push_back(c);
    }
    // Убрать "./" компоненты и завершающий '/'.
    std::string cleaned;
    cleaned.reserve(result.size());
    size_t index = 0;
    while (index < result.size()) {
        const size_t slash = result.find('/', index);
        const size_t end = slash == std::string::npos ? result.size() : slash;
        const std::string component = result.substr(index, end - index);
        if (!component.empty() && component != ".") {
            if (!cleaned.empty() && cleaned.back() != '/') cleaned.push_back('/');
            cleaned.append(component);
        } else if (component.empty() && cleaned.empty() && index == 0 && !result.empty() &&
                   result[0] == '/') {
            cleaned.push_back('/');
        }
        if (slash == std::string::npos) break;
        index = slash + 1;
    }
    if (cleaned.empty()) return ".";
    return cleaned;
}

std::string joinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (isAbsolutePath(right)) return right;
    std::string result = left;
    if (result.back() != '/' && result.back() != '\\') result.push_back('/');
    result.append(right);
    return normalizePath(result);
}

std::string fileName(const std::string& path)
{
    const std::string normalized = normalizePath(path);
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) return normalized;
    return normalized.substr(slash + 1);
}

std::string fileStem(const std::string& path)
{
    const std::string name = fileName(path);
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return name;
    return name.substr(0, dot);
}

std::string fileExtension(const std::string& path)
{
    const std::string name = fileName(path);
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return std::string();
    return toLowerAscii(name.substr(dot));
}

std::string parentPath(const std::string& path)
{
    const std::string normalized = normalizePath(path);
    const size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) return std::string();
    if (slash == 0) return "/";
    return normalized.substr(0, slash);
}

bool isAbsolutePath(const std::string& path)
{
    if (path.empty()) return false;
    if (path[0] == '/' || path[0] == '\\') return true;
    return hasWindowsDrive(path);
}

bool isSafeRelativePath(const std::string& path)
{
    if (path.empty()) return false;
    if (isAbsolutePath(path)) return false;
    for (size_t i = 0; i < path.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(path[i]);
        if (c < 0x20u) return false;
    }
    size_t index = 0;
    while (index <= path.size()) {
        const size_t slash = path.find_first_of("/\\", index);
        const size_t end = slash == std::string::npos ? path.size() : slash;
        const std::string component = path.substr(index, end - index);
        if (component == "..") return false;
        if (component.find(':') != std::string::npos) return false;
        if (slash == std::string::npos) break;
        index = slash + 1;
    }
    return true;
}

bool pathIsInside(const std::string& root, const std::string& child)
{
    std::error_code ec;
    std::string normalized_root =
        normalizePath(fs::absolute(fs::u8path(root), ec).lexically_normal().u8string());
    if (ec) return false;
    ec.clear();
    std::string normalized_child =
        normalizePath(fs::absolute(fs::u8path(child), ec).lexically_normal().u8string());
    if (ec) return false;
#ifdef _WIN32
    std::transform(normalized_root.begin(), normalized_root.end(), normalized_root.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(normalized_child.begin(), normalized_child.end(), normalized_child.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    if (normalized_root == normalized_child) return true;
    if (normalized_root.empty()) return false;
    if (normalized_root.back() != '/' && normalized_root.back() != '\\') {
        normalized_root.push_back('/');
    }
    return normalized_child.compare(0, normalized_root.size(), normalized_root) == 0;
}

std::string uniquePath(const std::string& path)
{
    if (!pathExists(path)) return path;
    const std::string parent = parentPath(path);
    const std::string stem = fileStem(path);
    const std::string extension = fileExtension(path);
    for (int index = 1; index < 10000; ++index) {
        const std::string candidate =
            joinPath(parent, stem + "_" + std::to_string(index) + extension);
        if (!pathExists(candidate)) return candidate;
    }
    return path;
}

std::string toLowerAscii(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string trim(const std::string& value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string sanitizeIdentifier(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (std::isalnum(c) != 0 || c == '_' || c == '-') {
            result.push_back(static_cast<char>(c));
        } else if (std::isspace(c) != 0 || c == '.') {
            result.push_back('_');
        }
    }
    while (!result.empty() && result.front() == '-')
        result.erase(result.begin());
    if (result.empty()) result = "retro_project";
    return result;
}

std::string sanitizeFileName(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x20u || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|') {
            result.push_back('_');
        } else {
            result.push_back(static_cast<char>(c));
        }
    }
    result = trim(result);
    if (result.empty() || result == "." || result == "..") result = "asset";
    return result;
}

std::string homeDirectory()
{
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
    if (home != nullptr && home[0] != '\0') return std::string(home);
    const char* drive = std::getenv("HOMEDRIVE");
    const char* path = std::getenv("HOMEPATH");
    if (drive != nullptr && path != nullptr) return std::string(drive) + path;
    return std::string();
#else
    const char* home = std::getenv("HOME");
    return home != nullptr ? std::string(home) : std::string();
#endif
}

std::string executableDirectory()
{
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return std::string();
    const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length),
                                                nullptr, 0, nullptr, nullptr);
    if (utf8_length <= 0) return std::string();
    std::string path(static_cast<size_t>(utf8_length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), &path[0], utf8_length,
                        nullptr, nullptr);
    return parentPath(normalizePath(path));
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) return std::string();
    std::error_code ec;
    const fs::path canonical = fs::canonical(fs::u8path(buffer.data()), ec);
    if (ec) return parentPath(normalizePath(buffer.data()));
    return parentPath(normalizePath(canonical.u8string()));
#else
    char buffer[4096];
    const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0) return std::string();
    buffer[length] = '\0';
    return parentPath(normalizePath(buffer));
#endif
}

std::string configDirectory()
{
    const char* override_dir = std::getenv("RPE_UI_CONFIG_DIR");
    if (override_dir != nullptr && override_dir[0] != '\0') {
        return normalizePath(override_dir);
    }
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata != nullptr && appdata[0] != '\0') {
        return joinPath(appdata, "RetroPolyEngine");
    }
    return joinPath(homeDirectory(), "AppData/Roaming/RetroPolyEngine");
#elif defined(__APPLE__)
    return joinPath(homeDirectory(), "Library/Application Support/RetroPolyEngine");
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && xdg[0] != '\0') {
        return joinPath(xdg, "retro-engine-ui");
    }
    return joinPath(homeDirectory(), ".config/retro-engine-ui");
#endif
}

std::string tempDirectory()
{
    const char* tmp = std::getenv("TMPDIR");
    if (tmp != nullptr && tmp[0] != '\0') return normalizePath(tmp);
    tmp = std::getenv("TEMP");
    if (tmp != nullptr && tmp[0] != '\0') return normalizePath(tmp);
    tmp = std::getenv("TMP");
    if (tmp != nullptr && tmp[0] != '\0') return normalizePath(tmp);
#ifdef _WIN32
    return normalizePath("C:/Windows/Temp");
#else
    return "/tmp";
#endif
}

std::string formatBytes(uint64_t bytes)
{
    if (bytes < 1024u) return std::to_string(bytes) + " B";
    const double kib = static_cast<double>(bytes) / 1024.0;
    if (kib < 1024.0) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.1f KiB", kib);
        return buffer;
    }
    const double mib = kib / 1024.0;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f MiB", mib);
    return buffer;
}

}  // namespace ui
}  // namespace rpe
