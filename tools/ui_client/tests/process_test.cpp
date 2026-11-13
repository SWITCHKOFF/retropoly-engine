// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: process runner.
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <thread>

#include "process_runner.h"
#include "sdk.h"

using namespace rpe::ui;

namespace {

std::vector<std::string> drain(ProcessRunner* runner)
{
    std::vector<std::string> all;
    while (runner->running()) {
        std::vector<std::string> lines = runner->takeLines();
        all.insert(all.end(), lines.begin(), lines.end());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::vector<std::string> rest = runner->takeLines();
    all.insert(all.end(), rest.begin(), rest.end());
    return all;
}

std::string cmakeExecutable()
{
    return findExecutable("", {"cmake", "cmake.exe"});
}

} // namespace

TEST(ProcessTest, StreamsOutputAndExitCode)
{
    const std::string cmake = cmakeExecutable();
    if (cmake.empty()) GTEST_SKIP() << "cmake not installed";
    ProcessSpec spec;
    spec.argv = {cmake, "-E", "echo", "hello-from-ui"};
    ProcessRunner runner;
    std::string error;
    ASSERT_TRUE(runner.start(spec, &error)) << error;
    EXPECT_TRUE(runner.pid() > 0u);
    EXPECT_TRUE(runner.wait(10000));
    const std::vector<std::string> lines = drain(&runner);
    EXPECT_EQ(runner.exitCode(), 0);
    const bool found = std::any_of(lines.begin(), lines.end(), [](const std::string& line) {
        return line.find("hello-from-ui") != std::string::npos;
    });
    EXPECT_TRUE(found);
}

TEST(ProcessTest, MissingExecutableFailsCleanly)
{
    ProcessSpec spec;
    spec.argv = {"/definitely/not/a/real/executable"};
    ProcessRunner runner;
    std::string error;
    if (runner.start(spec, &error)) {
        EXPECT_TRUE(runner.wait(5000));
        EXPECT_NE(runner.exitCode(), 0);
    } else {
        EXPECT_FALSE(error.empty());
    }
}

TEST(ProcessTest, CancelStopsLongProcess)
{
    const std::string cmake = cmakeExecutable();
    if (cmake.empty()) GTEST_SKIP() << "cmake not installed";
    ProcessSpec spec;
    spec.argv = {cmake, "-E", "sleep", "10"};
    ProcessRunner runner;
    std::string error;
    ASSERT_TRUE(runner.start(spec, &error)) << error;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    runner.cancel();
    EXPECT_TRUE(runner.wait(5000));
    EXPECT_FALSE(runner.running());
    EXPECT_NE(runner.exitCode(), 0);
}
