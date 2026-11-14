// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: SDK discovery/manifest.
#include <gtest/gtest.h>

#include <filesystem>

#include "fs_util.h"
#include "sdk.h"
#include "test_util.h"

using namespace rpe::ui;
using namespace rpe::ui::test;

namespace {

std::string fakeSdk(const TempDir& dir, const std::string& platform = "pc",
                    const std::string& version = "0.2.1")
{
    const std::string root = dir.str();
    writeFile(root + "/include/retro_engine/retro_engine.h", "#pragma once\n");
    writeFile(root + "/include/retro_engine/version.h", "#pragma once\n");
    writeFile(root + "/lib/cmake/RetroPolyEngine/RetroPolyEngineConfig.cmake", "# fake\n");
    writeFile(root + "/checksums.sha256", "\n");
    writeFile(root + "/VERSION.txt", "version=" + version + "\n");
    writeFile(root + "/templates/project/CMakeLists.txt", "project(fake)\n");
    const std::string example = platform == "pc"    ? "hello_triangle"
                                : platform == "ps1" ? "hello_triangle_ps1"
                                                    : "hello_triangle_saturn";
    writeFile(root + "/examples/" + example + "/CMakeLists.txt", "project(fake_example)\n");
    if (platform == "pc") {
        writeFile(root + "/tools/asset_converter", "#!/bin/sh\nexit 0\n");
        makeExecutable(root + "/tools/asset_converter");
    }
    const std::string manifest =
        "{\n"
        "  \"schema\": 1,\n"
        "  \"package\": \"retro-engine-sdk\",\n"
        "  \"name\": \"RetroPolyEngine\",\n"
        "  \"version\": \"" +
        version +
        "\",\n"
        "  \"git_commit\": \"deadbeef\",\n"
        "  \"commit_date\": \"2026-11-13T00:00:00+00:00\",\n"
        "  \"platform\": \"" +
        platform +
        "\",\n"
        "  \"os\": \"linux\",\n"
        "  \"arch\": \"x86_64\",\n"
        "  \"build_type\": \"Release\",\n"
        "  \"compiler\": {\"id\": \"GNU\", \"version\": \"13.0\"},\n"
        "  \"compiler_banner\": \"g++ 13.0\",\n"
        "  \"abi\": {\"cxx_standard\": \"c++14\", \"endianness\": \"little\", \"pointer_bits\": "
        "64},\n"
        "  \"components\": {\n"
        "    \"public_headers\": true, \"libraries\": true, \"cmake_package\": true,\n"
        "    \"asset_converter\": true, \"examples\": true, \"templates\": true,\n"
        "    \"legal_notices\": true, \"version\": true, \"checksums\": true,\n"
        "    \"manifest\": true\n"
        "  },\n"
        "  \"examples\": [\"" +
        example +
        "\"],\n"
        "  \"targets\": [\"RetroPolyEngine::retro_engine\", \"RetroPolyEngine::hal\",\n"
        "              \"RetroPolyEngine::log\"]\n"
        "}\n";
    writeFile(root + "/sdk_manifest.json", manifest);
    return root;
}

} // namespace

TEST(SdkTest, LoadsValidPcManifest)
{
    TempDir dir;
    fakeSdk(dir);
    const SdkLoadResult loaded = loadSdk(dir.str(), Lang::Ru);
    ASSERT_TRUE(loaded.ok) << loaded.error;
    EXPECT_TRUE(loaded.sdk.valid);
    EXPECT_EQ(loaded.sdk.version, "0.2.1");
    EXPECT_EQ(loaded.sdk.platform, Platform::Pc);
    EXPECT_EQ(loaded.sdk.compiler_id, "GNU");
    EXPECT_EQ(loaded.sdk.pointer_bits, 64);
    EXPECT_FALSE(loaded.sdk.converter_path.empty());
    EXPECT_TRUE(loaded.sdk.warning.empty());
}

TEST(SdkTest, RejectsIncompatibleVersion)
{
    TempDir dir;
    fakeSdk(dir, "pc", "0.3.0");
    const SdkLoadResult loaded = loadSdk(dir.str(), Lang::Ru);
    EXPECT_FALSE(loaded.ok);
    EXPECT_NE(loaded.error.find("0.3.0"), std::string::npos);
}

TEST(SdkTest, RejectsMissingComponent)
{
    TempDir dir;
    fakeSdk(dir);
    const std::string path = dir.str() + "/sdk_manifest.json";
    std::string manifest = readFile(path);
    const std::string needle = "\"libraries\": true";
    manifest.replace(manifest.find(needle), needle.size(), "\"libraries\": false");
    writeFile(path, manifest);
    const SdkLoadResult loaded = loadSdk(dir.str(), Lang::Ru);
    EXPECT_FALSE(loaded.ok);
    EXPECT_NE(loaded.error.find("libraries"), std::string::npos);
}

TEST(SdkTest, RejectsUnknownPlatform)
{
    TempDir dir;
    fakeSdk(dir);
    const std::string path = dir.str() + "/sdk_manifest.json";
    std::string manifest = readFile(path);
    manifest.replace(manifest.find("\"platform\": \"pc\""), 16, "\"platform\": \"md\"");
    writeFile(path, manifest);
    const SdkLoadResult loaded = loadSdk(dir.str(), Lang::Ru);
    EXPECT_FALSE(loaded.ok);
}

TEST(SdkTest, DiscoversTargets)
{
    TempDir dir;
    fakeSdk(dir);
    AppSettings settings;
    settings.sdk_pc = dir.str();
    const std::vector<TargetAvailability> targets = discoverTargets(settings, Lang::Ru);
    ASSERT_EQ(targets.size(), 3u);
    EXPECT_TRUE(targets[0].available);
    EXPECT_EQ(targets[0].sdk_path, absolutePath(dir.str()));
    EXPECT_FALSE(targets[1].available);
    EXPECT_FALSE(targets[2].available);
}

TEST(SdkTest, FindsCmakeInPath)
{
    const std::string cmake = findExecutable("", {"cmake", "cmake.exe"});
    EXPECT_FALSE(cmake.empty());
}
