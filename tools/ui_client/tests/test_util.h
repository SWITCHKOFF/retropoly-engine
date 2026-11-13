// SPDX-License-Identifier: MIT
// RetroPolyEngine — общие утилиты UI unit-тестов.
#pragma once

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace rpe {
namespace ui {
namespace test {

class TempDir {
public:
    TempDir()
    {
        static unsigned counter = 0;
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("rpe_ui_test_" + std::to_string(now) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }
    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::filesystem::path& path() const { return path_; }
    std::string str() const { return path_.u8string(); }

private:
    std::filesystem::path path_;
};

inline void setEnv(const char* name, const std::string& value)
{
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

inline void unsetEnv(const char* name)
{
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

inline void writeFile(const std::string& path, const std::string& content)
{
    const std::filesystem::path p(path);
    if (!p.parent_path().empty()) std::filesystem::create_directories(p.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

inline std::string readFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

inline void makeExecutable(const std::string& path)
{
#ifndef _WIN32
    std::filesystem::permissions(path,
                                 std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::group_exec |
                                     std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::add);
#else
    (void)path;
#endif
}

} // namespace test
} // namespace ui
} // namespace rpe
