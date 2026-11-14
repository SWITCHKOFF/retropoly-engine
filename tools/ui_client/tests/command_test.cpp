// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: command builder.
#include <gtest/gtest.h>

#include <algorithm>

#include "command.h"
#include "fs_util.h"
#include "test_util.h"

using namespace rpe::ui;
using namespace rpe::ui::test;

namespace {

bool contains(const std::vector<std::string>& values, const std::string& needle)
{
    return std::find(values.begin(), values.end(), needle) != values.end();
}

bool containsPrefix(const std::vector<std::string>& values, const std::string& prefix)
{
    return std::any_of(values.begin(), values.end(), [&](const std::string& value) {
        return value.compare(0, prefix.size(), prefix) == 0;
    });
}

SdkInfo fakeSdk(Platform platform, const std::string& path)
{
    SdkInfo sdk;
    sdk.valid = true;
    sdk.path = path;
    sdk.platform = platform;
    sdk.version = "0.2.1";
    return sdk;
}

} // namespace

TEST(CommandTest, PcBuildPlanUsesNinjaAndSdk)
{
    if (findExecutable("", {"cmake", "cmake.exe"}).empty() ||
        findExecutable("", {"ninja", "ninja.exe"}).empty()) {
        GTEST_SKIP() << "cmake/ninja not installed";
    }
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Pc,
                                          "hello_triangle", "hello_triangle");
    const BuildPlan plan = makeBuildPlan(project, dir.str(), fakeSdk(Platform::Pc, dir.str()),
                                         AppSettings(), Lang::Ru);
    ASSERT_TRUE(plan.ok) << plan.error;
    EXPECT_EQ(plan.build_dir, normalizePath(dir.str() + "/build/pc"));
    EXPECT_TRUE(contains(plan.configure, "-S"));
    EXPECT_TRUE(contains(plan.configure, dir.str()));
    EXPECT_TRUE(contains(plan.configure, "-G"));
    EXPECT_TRUE(contains(plan.configure, "Ninja"));
    EXPECT_TRUE(containsPrefix(plan.configure, "-DCMAKE_PREFIX_PATH="));
    EXPECT_TRUE(containsPrefix(plan.configure, "-DCMAKE_MAKE_PROGRAM="));
    EXPECT_TRUE(contains(plan.build, "--target"));
    EXPECT_TRUE(contains(plan.build, "hello_triangle"));
}

TEST(CommandTest, Ps1PlanAddsToolchain)
{
    if (findExecutable("", {"cmake", "cmake.exe"}).empty() ||
        findExecutable("", {"ninja", "ninja.exe"}).empty()) {
        GTEST_SKIP() << "cmake/ninja not installed";
    }
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Ps1,
                                          "hello_triangle_ps1", "hello_triangle_ps1");
    const BuildPlan plan = makeBuildPlan(project, dir.str(), fakeSdk(Platform::Ps1, dir.str()),
                                         AppSettings(), Lang::Ru);
    ASSERT_TRUE(plan.ok) << plan.error;
    EXPECT_TRUE(containsPrefix(plan.configure, "-DCMAKE_TOOLCHAIN_FILE="));
    EXPECT_TRUE(contains(plan.configure, "-DCMAKE_TOOLCHAIN_FILE=" +
                                             joinPath(dir.str(), "cmake/ps1-toolchain.cmake")));
    EXPECT_EQ(plan.build_dir, normalizePath(dir.str() + "/build/ps1"));
}

TEST(CommandTest, PcRunFindsExecutable)
{
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Pc,
                                          "hello_triangle", "hello_triangle");
#ifdef _WIN32
    const std::string exe = dir.str() + "/build/pc/bin/hello_triangle.exe";
#else
    const std::string exe = dir.str() + "/build/pc/bin/hello_triangle";
#endif
    writeFile(exe, "fake");
    makeExecutable(exe);
    const RunPlan plan =
        makeRunPlan(project, dir.str(), fakeSdk(Platform::Pc, dir.str()), AppSettings(), Lang::Ru);
    ASSERT_TRUE(plan.ok) << plan.error;
    EXPECT_EQ(plan.argv.size(), 1u);
    EXPECT_EQ(plan.argv[0], absolutePath(exe));
    EXPECT_FALSE(plan.uses_emulator);
}

TEST(CommandTest, PcRunReportsMissingBinary)
{
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Pc,
                                          "hello_triangle", "hello_triangle");
    const RunPlan plan =
        makeRunPlan(project, dir.str(), fakeSdk(Platform::Pc, dir.str()), AppSettings(), Lang::Ru);
    EXPECT_FALSE(plan.ok);
    EXPECT_EQ(plan.error_key, "err.run_no_binary");
}

TEST(CommandTest, Ps1RunUsesConfiguredEmulatorAndCue)
{
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Ps1,
                                          "hello_triangle_ps1", "hello_triangle_ps1");
    const std::string emulator = dir.str() + "/duckstation";
    writeFile(emulator, "fake");
    makeExecutable(emulator);
    const std::string cue = dir.str() + "/build/ps1/game.cue";
    writeFile(cue, "FILE \"game.bin\" BINARY\n");
    AppSettings settings;
    settings.emulator_ps1 = emulator;
    const RunPlan plan =
        makeRunPlan(project, dir.str(), fakeSdk(Platform::Ps1, dir.str()), settings, Lang::Ru);
    ASSERT_TRUE(plan.ok) << plan.error;
    EXPECT_TRUE(plan.uses_emulator);
    EXPECT_EQ(plan.argv.size(), 2u);
    EXPECT_EQ(plan.cue_path, normalizePath(cue));
}

TEST(CommandTest, Ps1RunWithoutEmulatorIsError)
{
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Ps1,
                                          "hello_triangle_ps1", "hello_triangle_ps1");
    const RunPlan plan =
        makeRunPlan(project, dir.str(), fakeSdk(Platform::Ps1, dir.str()), AppSettings(), Lang::Ru);
    EXPECT_FALSE(plan.ok);
    EXPECT_EQ(plan.error_key, "err.run_no_emulator");
}

TEST(CommandTest, ProjectEmulatorOverridesSettings)
{
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Saturn,
                                          "hello_triangle_saturn", "hello_triangle_saturn");
    const std::string emulator = dir.str() + "/mednafen";
    writeFile(emulator, "fake");
    makeExecutable(emulator);
    project.emulator_path = emulator;
    writeFile(dir.str() + "/build/saturn/game.cue", "cue");
    const RunPlan plan = makeRunPlan(project, dir.str(), fakeSdk(Platform::Saturn, dir.str()),
                                     AppSettings(), Lang::Ru);
    ASSERT_TRUE(plan.ok) << plan.error;
    EXPECT_EQ(plan.argv[0], absolutePath(emulator));
}
