// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: AppCore project flow.
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

#include "app_core.h"
#include "fs_util.h"
#include "test_util.h"

#ifndef RPE_TEST_ASSET_CONVERTER
#define RPE_TEST_ASSET_CONVERTER ""
#endif
#ifndef RPE_TEST_SOURCE_DIR
#define RPE_TEST_SOURCE_DIR "."
#endif

using namespace rpe::ui;
using namespace rpe::ui::test;

namespace {

std::string createFakePcSdk(const TempDir& dir)
{
    const std::string root = dir.str();
    writeFile(root + "/include/retro_engine/retro_engine.h", "#pragma once\n");
    writeFile(root + "/include/retro_engine/version.h", "#pragma once\n");
    writeFile(root + "/lib/cmake/RetroPolyEngine/RetroPolyEngineConfig.cmake", "# fake\n");
    writeFile(root + "/checksums.sha256", "\n");
    writeFile(root + "/VERSION.txt", "version=0.2.1\n");
    writeFile(root + "/templates/project/CMakeLists.txt", "project(fake)\n");
    std::filesystem::create_directories(root + "/tools");
    if (isRegularFile(RPE_TEST_ASSET_CONVERTER)) {
        std::filesystem::copy_file(RPE_TEST_ASSET_CONVERTER, root + "/tools/asset_converter",
                                   std::filesystem::copy_options::overwrite_existing);
    } else {
        writeFile(root + "/tools/asset_converter", "#!/bin/sh\nexit 1\n");
    }
    makeExecutable(root + "/tools/asset_converter");
    writeFile(root + "/examples/hello_triangle/CMakeLists.txt",
              "cmake_minimum_required(VERSION 3.25)\n"
              "project(fake_example LANGUAGES CXX)\n"
              "find_package(RetroPolyEngine CONFIG REQUIRED)\n"
              "add_executable(hello_triangle main.cpp)\n"
              "target_link_libraries(hello_triangle PRIVATE RetroPolyEngine::retro_engine)\n"
              "target_compile_definitions(hello_triangle PRIVATE "
              "HT_ASSET_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\")\n"
              "target_include_directories(hello_triangle PRIVATE "
              "\"${CMAKE_CURRENT_SOURCE_DIR}/../..\")\n");
    writeFile(root + "/examples/hello_triangle/main.cpp",
              "#include \"examples/hello_triangle/scene.h\"\nint main() { return 0; }\n");
    writeFile(root + "/examples/hello_triangle/scene.h", "#pragma once\n");
    writeFile(root + "/examples/hello_triangle/assets/triangle.mesh", "fake");
    const std::string manifest =
        "{\"schema\":1,\"package\":\"retro-engine-sdk\",\"name\":\"RetroPolyEngine\","
        "\"version\":\"0.2.1\",\"git_commit\":\"deadbeef\","
        "\"commit_date\":\"2026-11-13T00:00:00+00:00\",\"platform\":\"pc\","
        "\"os\":\"linux\",\"arch\":\"x86_64\",\"build_type\":\"Release\","
        "\"compiler\":{\"id\":\"GNU\",\"version\":\"13.0\"},"
        "\"compiler_banner\":\"g++ 13.0\","
        "\"abi\":{\"cxx_standard\":\"c++14\",\"endianness\":\"little\",\"pointer_bits\":64},"
        "\"components\":{\"public_headers\":true,\"libraries\":true,"
        "\"cmake_package\":true,\"asset_converter\":true,\"examples\":true,"
        "\"templates\":true,\"legal_notices\":true,\"version\":true,"
        "\"checksums\":true,\"manifest\":true},"
        "\"examples\":[\"hello_triangle\"],"
        "\"targets\":[\"RetroPolyEngine::retro_engine\"]}\n";
    writeFile(root + "/sdk_manifest.json", manifest);
    return root;
}

std::string createFakeCrossSdk(const TempDir& dir, const std::string& platform)
{
    const std::string root = dir.str();
    writeFile(root + "/include/retro_engine/retro_engine.h", "#pragma once\n");
    writeFile(root + "/include/retro_engine/version.h", "#pragma once\n");
    writeFile(root + "/lib/cmake/RetroPolyEngine/RetroPolyEngineConfig.cmake", "# fake\n");
    writeFile(root + "/checksums.sha256", "\n");
    writeFile(root + "/VERSION.txt", "version=0.2.1\n");
    writeFile(root + "/templates/project/CMakeLists.txt", "project(fake)\n");
    const std::string example = platform == "ps1" ? "hello_triangle_ps1" : "hello_triangle_saturn";
    const std::string target = example;
    writeFile(root + "/examples/" + example + "/main.cpp", "int main() { return 0; }\n");
    if (platform == "ps1") {
        writeFile(root + "/examples/" + example + "/CMakeLists.txt",
                  "cmake_minimum_required(VERSION 3.25)\n"
                  "project(fake_ps1 LANGUAGES CXX)\n"
                  "find_package(RetroPolyEngine CONFIG REQUIRED)\n"
                  "psn00bsdk_add_executable(hello_triangle_ps1 GPREL main.cpp)\n");
    } else {
        writeFile(root + "/examples/" + example + "/jo_bridge.c", "/* fake */\n");
        writeFile(root + "/examples/" + example + "/CMakeLists.txt",
                  "cmake_minimum_required(VERSION 3.25)\n"
                  "project(fake_saturn LANGUAGES C CXX ASM)\n"
                  "find_package(RetroPolyEngine CONFIG REQUIRED)\n"
                  "add_executable(hello_triangle_saturn main.cpp jo_bridge.c)\n");
    }
    const std::string manifest =
        "{\"schema\":1,\"package\":\"retro-engine-sdk\",\"name\":\"RetroPolyEngine\","
        "\"version\":\"0.2.1\",\"git_commit\":\"deadbeef\","
        "\"commit_date\":\"2026-11-13T00:00:00+00:00\",\"platform\":\"" +
        platform +
        "\","
        "\"os\":\"" +
        platform +
        "\",\"arch\":\"test\",\"build_type\":\"Release\","
        "\"compiler\":{\"id\":\"GNU\",\"version\":\"12.3\"},"
        "\"compiler_banner\":\"g++ 12.3\","
        "\"abi\":{\"cxx_standard\":\"c++14\",\"endianness\":\"little\",\"pointer_bits\":32},"
        "\"components\":{\"public_headers\":true,\"libraries\":true,"
        "\"cmake_package\":true,\"asset_converter\":false,\"examples\":true,"
        "\"templates\":true,\"legal_notices\":true,\"version\":true,"
        "\"checksums\":true,\"manifest\":true},"
        "\"examples\":[\"" +
        example +
        "\"],"
        "\"targets\":[\"RetroPolyEngine::retro_engine\"]}\n";
    writeFile(root + "/sdk_manifest.json", manifest);
    return root;
}

class AppCoreTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        const char* previous = std::getenv("RPE_UI_CONFIG_DIR");
        had_previous_ = previous != nullptr;
        if (had_previous_) previous_ = previous;
        setEnv("RPE_UI_CONFIG_DIR", config_.str());
        core_.initialize(nullptr);
        core_.mutableSettings().sdk_pc = createFakePcSdk(sdk_);
        core_.refreshTargets();
    }
    void TearDown() override
    {
        core_.closeProject();
        if (had_previous_) {
            setEnv("RPE_UI_CONFIG_DIR", previous_);
        } else {
            unsetEnv("RPE_UI_CONFIG_DIR");
        }
    }

    TempDir config_;
    TempDir sdk_;
    TempDir projects_;
    AppCore core_;
    bool had_previous_ = false;
    std::string previous_;
};

} // namespace

TEST_F(AppCoreTest, CreatesOpensAndSavesProject)
{
    NewProjectOptions options;
    options.name = "demo_project";
    options.location = projects_.str();
    options.platform = Platform::Pc;
    options.template_name = "hello_triangle";
    std::string error;
    ASSERT_TRUE(core_.newProject(options, &error)) << error;
    ASSERT_TRUE(core_.hasProject());
    const std::string root = projects_.str() + "/demo_project";
    EXPECT_TRUE(isRegularFile(root + "/.rpeproject"));
    EXPECT_TRUE(isRegularFile(root + "/src/main.cpp"));
    EXPECT_TRUE(isRegularFile(root + "/src/scene.h"));
    EXPECT_TRUE(isRegularFile(root + "/assets/build/triangle.mesh"));
    const std::string cmake = readFile(root + "/CMakeLists.txt");
    EXPECT_NE(cmake.find("src/main.cpp"), std::string::npos);
    EXPECT_NE(cmake.find("assets/build"), std::string::npos);
    const std::string main = readFile(root + "/src/main.cpp");
    EXPECT_NE(main.find("#include \"scene.h\""), std::string::npos);

    const ProjectLoadResult loaded = loadProject(root, Lang::Ru);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(loaded.project.name, "demo_project");
    EXPECT_EQ(loaded.project.target, "hello_triangle");

    ASSERT_TRUE(core_.saveProject(&error)) << error;
    core_.closeProject();
    EXPECT_FALSE(core_.hasProject());
    ASSERT_TRUE(core_.openProject(root, &error)) << error;
    EXPECT_TRUE(core_.hasProject());
    const std::vector<RecentProject> recent = core_.recentProjects();
    ASSERT_FALSE(recent.empty());
    EXPECT_TRUE(recent.front().exists);
}

TEST_F(AppCoreTest, ImportsAssetIntoProject)
{
    if (!isRegularFile(RPE_TEST_ASSET_CONVERTER)) {
        GTEST_SKIP() << "asset_converter not built";
    }
    // Для импорта нужен настоящий converter: подменяем валидный SDK.
    NewProjectOptions options;
    options.name = "asset_project";
    options.location = projects_.str();
    options.platform = Platform::Pc;
    std::string error;
    ASSERT_TRUE(core_.newProject(options, &error)) << error;
    core_.mutableSettings().sdk_pc = sdk_.str();
    ASSERT_TRUE(core_.setSdkPath(Platform::Pc, sdk_.str(), &error)) << error;

    const AssetImportResult result =
        core_.importAsset(std::string(RPE_TEST_SOURCE_DIR) + "/assets/source/cube.gltf");
    ASSERT_TRUE(result.ok) << result.error << "\n" << result.log;
    EXPECT_EQ(core_.project().assets.size(), 1u);
    EXPECT_TRUE(isRegularFile(core_.projectRootPath() + "/" + result.entry.output));
}

TEST_F(AppCoreTest, TargetSwitchRequiresSdk)
{
    NewProjectOptions options;
    options.name = "target_project";
    options.location = projects_.str();
    options.platform = Platform::Pc;
    std::string error;
    ASSERT_TRUE(core_.newProject(options, &error)) << error;
    EXPECT_FALSE(core_.setProjectTarget(Platform::Saturn, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(core_.project().platform, Platform::Pc);
}

TEST_F(AppCoreTest, BuildRequiresProjectAndSdk)
{
    std::string error;
    EXPECT_FALSE(core_.startBuild(&error));
    EXPECT_FALSE(error.empty());
}

TEST_F(AppCoreTest, CreatesPs1ProjectFromSdkTemplate)
{
    TempDir ps1_sdk;
    createFakeCrossSdk(ps1_sdk, "ps1");
    setEnv("PSN00BSDK_LIBS", "/fake/psn00bsdk/lib");
    core_.mutableSettings().sdk_ps1 = ps1_sdk.str();
    core_.refreshTargets();

    NewProjectOptions options;
    options.name = "ps1_project";
    options.location = projects_.str();
    options.platform = Platform::Ps1;
    std::string error;
    ASSERT_TRUE(core_.newProject(options, &error)) << error;
    const std::string cmake = readFile(projects_.str() + "/ps1_project/CMakeLists.txt");
    EXPECT_NE(cmake.find("src/main.cpp"), std::string::npos);
    EXPECT_NE(cmake.find("hello_triangle_ps1"), std::string::npos);
    const ProjectLoadResult loaded = loadProject(projects_.str() + "/ps1_project", Lang::Ru);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_EQ(loaded.project.platform, Platform::Ps1);
    EXPECT_EQ(loaded.project.target, "hello_triangle_ps1");
    unsetEnv("PSN00BSDK_LIBS");
}

TEST_F(AppCoreTest, CreatesSaturnProjectFromSdkTemplate)
{
    TempDir saturn_sdk;
    createFakeCrossSdk(saturn_sdk, "saturn");
    setEnv("JO_ENGINE_ROOT", "/fake/joengine");
    core_.mutableSettings().sdk_saturn = saturn_sdk.str();
    core_.refreshTargets();

    NewProjectOptions options;
    options.name = "saturn_project";
    options.location = projects_.str();
    options.platform = Platform::Saturn;
    std::string error;
    ASSERT_TRUE(core_.newProject(options, &error)) << error;
    const std::string cmake = readFile(projects_.str() + "/saturn_project/CMakeLists.txt");
    EXPECT_NE(cmake.find("src/main.cpp src/jo_bridge.c"), std::string::npos);
    EXPECT_TRUE(isRegularFile(projects_.str() + "/saturn_project/src/jo_bridge.c"));
    unsetEnv("JO_ENGINE_ROOT");
}
