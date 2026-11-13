// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/process.cpp
#include "process.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace rpe {
namespace ui {

namespace {

std::string quoteForDisplay(const std::string& value)
{
    if (value.empty()) return "\"\"";
    if (value.find_first_of(" \t\"") == std::string::npos) return value;
    std::string result = "\"";
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '"' || value[i] == '\\') result.push_back('\\');
        result.push_back(value[i]);
    }
    result.push_back('"');
    return result;
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty()) return std::wstring();
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(),
                                     static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        length =
            MultiByteToWideChar(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    }
    if (length <= 0) return std::wstring();
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &result[0],
                        length);
    return result;
}

std::string wideToUtf8(const std::wstring& text)
{
    if (text.empty()) return std::string();
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (length <= 0) return std::string();
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &result[0], length,
                        nullptr, nullptr);
    return result;
}

std::wstring quoteWindowsArg(const std::wstring& value)
{
    if (value.empty()) return L"\"\"";
    if (value.find_first_of(L" \t\"") == std::wstring::npos) return value;
    std::wstring result = L"\"";
    size_t backslashes = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        const wchar_t c = value[i];
        if (c == L'\\') {
            ++backslashes;
        } else if (c == L'"') {
            result.append(backslashes + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            backslashes = 0;
            result.push_back(c);
        }
    }
    result.append(backslashes, L'\\');
    result.push_back(L'"');
    return result;
}

std::vector<wchar_t> buildEnvironmentBlock(const std::map<std::string, std::string>& overrides)
{
    std::vector<std::wstring> entries;
    LPWCH block = GetEnvironmentStringsW();
    if (block != nullptr) {
        const wchar_t* cursor = block;
        while (*cursor != L'\0') {
            const std::wstring entry(cursor);
            const size_t equals = entry.find(L'=');
            const std::wstring key =
                equals == std::wstring::npos ? std::wstring() : entry.substr(0, equals);
            bool replaced = false;
            if (!key.empty() && key[0] != L'=') {
                const std::string key_utf8 = wideToUtf8(key);
                std::map<std::string, std::string>::const_iterator it = overrides.find(key_utf8);
                if (it != overrides.end()) {
                    entries.push_back(key + L"=" + utf8ToWide(it->second));
                    replaced = true;
                }
            }
            if (!replaced) entries.push_back(entry);
            cursor += entry.size() + 1;
        }
        FreeEnvironmentStringsW(block);
    }
    for (std::map<std::string, std::string>::const_iterator it = overrides.begin();
         it != overrides.end(); ++it) {
        bool present = false;
        const std::wstring key = utf8ToWide(it->first);
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].compare(0, key.size() + 1, key + L"=") == 0) {
                present = true;
                break;
            }
        }
        if (!present) entries.push_back(key + L"=" + utf8ToWide(it->second));
    }

    std::vector<wchar_t> result;
    for (size_t i = 0; i < entries.size(); ++i) {
        result.insert(result.end(), entries[i].begin(), entries[i].end());
        result.push_back(L'\0');
    }
    result.push_back(L'\0');
    return result;
}
#endif

} // namespace

struct ProcessRunner::Impl
{
    std::atomic<bool> running{false};
    std::atomic<bool> cancelled{false};
    std::atomic<int> exit_code{-1};
    std::atomic<uint64_t> pid{0};
    std::mutex lines_mutex;
    std::vector<std::string> lines;
    std::thread reader;

#ifdef _WIN32
    HANDLE process = nullptr;
    HANDLE read_pipe = nullptr;
#else
    int read_fd = -1;
    int child_pid = -1;
#endif

    void closeHandles()
    {
#ifdef _WIN32
        if (read_pipe != nullptr) {
            CloseHandle(read_pipe);
            read_pipe = nullptr;
        }
        if (process != nullptr) {
            CloseHandle(process);
            process = nullptr;
        }
#else
        if (read_fd >= 0) {
            close(read_fd);
            read_fd = -1;
        }
        child_pid = -1;
#endif
    }

    ~Impl()
    {
        if (reader.joinable()) reader.join();
        closeHandles();
    }

    void append(std::string* partial, const char* data, size_t size)
    {
        std::vector<std::string> ready;
        for (size_t i = 0; i < size; ++i) {
            const char c = data[i];
            if (c == '\n') {
                if (!partial->empty() && partial->back() == '\r') partial->pop_back();
                ready.push_back(*partial);
                partial->clear();
            } else {
                partial->push_back(c);
            }
        }
        if (!ready.empty()) {
            std::lock_guard<std::mutex> lock(lines_mutex);
            lines.insert(lines.end(), ready.begin(), ready.end());
        }
    }

    void finish(std::string* partial, int code)
    {
        if (!partial->empty()) {
            if (partial->back() == '\r') partial->pop_back();
            std::lock_guard<std::mutex> lock(lines_mutex);
            lines.push_back(*partial);
            partial->clear();
        }
        exit_code.store(code);
        running.store(false);
    }
};

ProcessRunner::ProcessRunner() : impl_(new Impl())
{
}

ProcessRunner::~ProcessRunner()
{
    if (impl_ && impl_->running.load()) {
        cancel();
        wait(2000);
    }
}

#ifdef _WIN32

bool ProcessRunner::start(const ProcessSpec& spec, std::string* error)
{
    if (impl_->running.load()) {
        if (error != nullptr) *error = "process is already running";
        return false;
    }
    if (spec.argv.empty()) {
        if (error != nullptr) *error = "empty command line";
        return false;
    }
    impl_->lines.clear();
    impl_->cancelled.store(false);
    impl_->exit_code.store(-1);
    if (impl_->reader.joinable()) impl_->reader.join();
    impl_->closeHandles();

    std::wstring command_line;
    for (size_t i = 0; i < spec.argv.size(); ++i) {
        if (i > 0) command_line.push_back(L' ');
        command_line.append(quoteWindowsArg(utf8ToWide(spec.argv[i])));
    }

    SECURITY_ATTRIBUTES attributes;
    std::memset(&attributes, 0, sizeof(attributes));
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &attributes, 0)) {
        if (error != nullptr) *error = "CreatePipe failed";
        return false;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup;
    std::memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = write_pipe;
    startup.hStdError = write_pipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    std::vector<wchar_t> environment;
    LPVOID environment_ptr = nullptr;
    if (!spec.environment.empty()) {
        environment = buildEnvironmentBlock(spec.environment);
        environment_ptr = environment.data();
    }

    const std::wstring working_directory = utf8ToWide(spec.working_directory);
    PROCESS_INFORMATION process_info;
    std::memset(&process_info, 0, sizeof(process_info));
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');
    const BOOL created = CreateProcessW(
        nullptr, mutable_command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, environment_ptr,
        working_directory.empty() ? nullptr : working_directory.c_str(), &startup, &process_info);
    CloseHandle(write_pipe);
    if (!created) {
        CloseHandle(read_pipe);
        if (error != nullptr) *error = "CreateProcess failed: " + spec.argv[0];
        return false;
    }
    CloseHandle(process_info.hThread);

    impl_->process = process_info.hProcess;
    impl_->read_pipe = read_pipe;
    impl_->pid.store(static_cast<uint64_t>(process_info.dwProcessId));
    impl_->running.store(true);

    HANDLE process_handle = process_info.hProcess;
    HANDLE pipe_handle = read_pipe;
    Impl* impl = impl_.get();
    impl->reader = std::thread([impl, process_handle, pipe_handle]() {
        std::string partial;
        char buffer[4096];
        for (;;) {
            DWORD read = 0;
            const BOOL ok =
                ReadFile(pipe_handle, buffer, static_cast<DWORD>(sizeof(buffer)), &read, nullptr);
            if (!ok || read == 0) break;
            impl->append(&partial, buffer, read);
        }
        WaitForSingleObject(process_handle, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(process_handle, &code);
        impl->finish(&partial, static_cast<int>(code));
    });
    return true;
}

void ProcessRunner::cancel()
{
    impl_->cancelled.store(true);
    if (impl_->process != nullptr) {
        TerminateProcess(impl_->process, 1);
    }
}

#else

bool ProcessRunner::start(const ProcessSpec& spec, std::string* error)
{
    if (impl_->running.load()) {
        if (error != nullptr) *error = "process is already running";
        return false;
    }
    if (spec.argv.empty()) {
        if (error != nullptr) *error = "empty command line";
        return false;
    }
    impl_->lines.clear();
    impl_->cancelled.store(false);
    impl_->exit_code.store(-1);
    if (impl_->reader.joinable()) impl_->reader.join();
    impl_->closeHandles();

    int pipe_fds[2] = {-1, -1};
    if (pipe(pipe_fds) != 0) {
        if (error != nullptr) *error = "pipe failed";
        return false;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        if (error != nullptr) *error = "fork failed";
        return false;
    }
    if (pid == 0) {
        // Child: отдельная process group для отмены (kill(-pid)).
        setsid();
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        if (!spec.working_directory.empty()) {
            if (chdir(spec.working_directory.c_str()) != 0) _exit(126);
        }
        for (std::map<std::string, std::string>::const_iterator it = spec.environment.begin();
             it != spec.environment.end(); ++it) {
            setenv(it->first.c_str(), it->second.c_str(), 1);
        }
        std::vector<char*> argv;
        argv.reserve(spec.argv.size() + 1);
        for (size_t i = 0; i < spec.argv.size(); ++i) {
            argv.push_back(const_cast<char*>(spec.argv[i].c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(pipe_fds[1]);
    impl_->read_fd = pipe_fds[0];
    impl_->child_pid = static_cast<int>(pid);
    impl_->pid.store(static_cast<uint64_t>(pid));
    impl_->running.store(true);

    const int fd = pipe_fds[0];
    const int child = static_cast<int>(pid);
    Impl* impl = impl_.get();
    impl->reader = std::thread([impl, fd, child]() {
        std::string partial;
        char buffer[4096];
        for (;;) {
            const ssize_t count = read(fd, buffer, sizeof(buffer));
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) break;
            impl->append(&partial, buffer, static_cast<size_t>(count));
        }
        int status = 0;
        int waited = 0;
        do {
            waited = waitpid(child, &status, 0);
        } while (waited < 0 && errno == EINTR);
        int code = 1;
        if (waited < 0) {
            code = 1;
        } else if (WIFEXITED(status)) {
            code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            code = 128 + WTERMSIG(status);
        }
        impl->finish(&partial, code);
    });
    return true;
}

void ProcessRunner::cancel()
{
    impl_->cancelled.store(true);
    if (!impl_->running.load()) return;
    if (impl_->child_pid > 0) {
        const pid_t pid = static_cast<pid_t>(impl_->child_pid);
        kill(-pid, SIGTERM);
        // Гарантированное завершение: добить через 500 мс.
        std::thread([pid]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            kill(-pid, SIGKILL);
        }).detach();
    }
}

#endif

bool ProcessRunner::running() const
{
    return impl_->running.load();
}

bool ProcessRunner::wait(int timeout_ms)
{
    if (!impl_->running.load()) {
        if (impl_->reader.joinable()) impl_->reader.join();
        return true;
    }
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    while (impl_->running.load()) {
        if (timeout_ms >= 0) {
            const int elapsed =
                static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count());
            if (elapsed >= timeout_ms) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!impl_->running.load() && impl_->reader.joinable()) impl_->reader.join();
    return !impl_->running.load();
}

int ProcessRunner::exitCode() const
{
    return impl_->exit_code.load();
}

bool ProcessRunner::cancelled() const
{
    return impl_->cancelled.load();
}

std::vector<std::string> ProcessRunner::takeLines()
{
    std::lock_guard<std::mutex> lock(impl_->lines_mutex);
    std::vector<std::string> result;
    result.swap(impl_->lines);
    return result;
}

uint64_t ProcessRunner::pid() const
{
    return impl_->pid.load();
}

std::string commandLineToString(const std::vector<std::string>& argv)
{
    std::string result;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i > 0) result.push_back(' ');
        result.append(quoteForDisplay(argv[i]));
    }
    return result;
}

} // namespace ui
} // namespace rpe
