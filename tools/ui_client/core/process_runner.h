// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/process.h
//
// Кроссплатформенный запуск внешних процессов со streaming stdout/stderr,
// отменой и exit code (CMake, asset_converter, эмулятор, target).
#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace rpe {
namespace ui {

/** Описание запускаемого процесса. */
struct ProcessSpec
{
    std::vector<std::string> argv;
    std::string working_directory;
    /** Дополнительные/переопределяющие переменные окружения. */
    std::map<std::string, std::string> environment;
};

/**
 * Асинхронный процесс: строки stdout+stderr накапливаются в очереди,
 * UI забирает их через takeLines().
 */
class ProcessRunner {
public:
    ProcessRunner();
    ~ProcessRunner();

    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;

    /** Запустить процесс. @return false, если запуск невозможен. */
    bool start(const ProcessSpec& spec, std::string* error);

    /** @return true, пока процесс выполняется. */
    bool running() const;

    /** Отменить процесс (SIGTERM/TerminateProcess + добивание). */
    void cancel();

    /**
     * Ждать завершения.
     *
     * @param timeout_ms -1 — бесконечно; 0 — без ожидания.
     * @return true, если процесс завершён.
     */
    bool wait(int timeout_ms);

    /** Exit code или -1, если процесс ещё не завершён. */
    int exitCode() const;

    /** @return true, если пользователь отменил процесс. */
    bool cancelled() const;

    /** Забрать накопленные строки (потокобезопасно). */
    std::vector<std::string> takeLines();

    /** PID процесса (0 до запуска). */
    uint64_t pid() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/** Текст команды для лога (без shell-экранирования, с кавычками). */
std::string commandLineToString(const std::vector<std::string>& argv);

}  // namespace ui
}  // namespace rpe
