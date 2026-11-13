// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: asset import/probe.
#include <gtest/gtest.h>

#include "asset_import.h"
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

SdkInfo converterSdk()
{
    SdkInfo sdk;
    sdk.valid = true;
    sdk.platform = Platform::Pc;
    sdk.path = "/fake/sdk";
    sdk.converter_path = RPE_TEST_ASSET_CONVERTER;
    return sdk;
}

std::string sourceAsset(const std::string& name)
{
    return std::string(RPE_TEST_SOURCE_DIR) + "/assets/source/" + name;
}

} // namespace

TEST(AssetImportTest, ImportsMeshAndTexture)
{
    if (!isRegularFile(RPE_TEST_ASSET_CONVERTER)) {
        GTEST_SKIP() << "asset_converter not built";
    }
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Pc,
                                          "hello_triangle", "hello_triangle");

    AssetImportResult mesh =
        importAsset(dir.str(), &project, converterSdk(), sourceAsset("cube.gltf"), Lang::Ru);
    ASSERT_TRUE(mesh.ok) << mesh.error << "\n" << mesh.log;
    EXPECT_EQ(mesh.entry.type, AssetType::Mesh);
    EXPECT_GT(mesh.entry.vertex_count, 0u);
    EXPECT_GT(mesh.entry.triangle_count, 0u);
    EXPECT_TRUE(isRegularFile(mesh.output));

    AssetImportResult texture =
        importAsset(dir.str(), &project, converterSdk(), sourceAsset("texture.png"), Lang::Ru);
    ASSERT_TRUE(texture.ok) << texture.error << "\n" << texture.log;
    EXPECT_EQ(texture.entry.type, AssetType::Texture);
    EXPECT_EQ(texture.entry.width, 256u);
    EXPECT_EQ(texture.entry.height, 256u);
    ASSERT_EQ(project.assets.size(), 2u);

    AssetImportResult again =
        importAsset(dir.str(), &project, converterSdk(), sourceAsset("cube.gltf"), Lang::Ru);
    ASSERT_TRUE(again.ok) << again.error;
    EXPECT_EQ(project.assets.size(), 2u);
}

TEST(AssetImportTest, RemoveDeletesOutputAndEntry)
{
    if (!isRegularFile(RPE_TEST_ASSET_CONVERTER)) {
        GTEST_SKIP() << "asset_converter not built";
    }
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Pc,
                                          "hello_triangle", "hello_triangle");
    AssetImportResult imported =
        importAsset(dir.str(), &project, converterSdk(), sourceAsset("cube.gltf"), Lang::Ru);
    ASSERT_TRUE(imported.ok) << imported.error;
    ASSERT_EQ(project.assets.size(), 1u);
    const std::string output = imported.output;
    std::string error;
    ASSERT_TRUE(removeAsset(dir.str(), &project, 0, true, &error, Lang::Ru)) << error;
    EXPECT_TRUE(project.assets.empty());
    EXPECT_FALSE(isRegularFile(output));
}

TEST(AssetImportTest, RejectsUnsupportedExtensionAndMalformedOutput)
{
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Pc,
                                          "hello_triangle", "hello_triangle");
    AssetImportResult result =
        importAsset(dir.str(), &project, converterSdk(), "notes.txt", Lang::Ru);
    EXPECT_FALSE(result.ok);

    const std::string malformed = dir.str() + "/bad.mesh";
    writeFile(malformed, "not a mesh");
    AssetEntry entry;
    entry.type = AssetType::Mesh;
    std::string error;
    EXPECT_FALSE(probeAssetFile(malformed, dir.str(), &entry, &error, Lang::Ru));
    EXPECT_FALSE(error.empty());
}

TEST(AssetImportTest, RejectsTraversalOnRemove)
{
    TempDir dir;
    Project project = makeProjectTemplate("demo", "0.2.1", dir.str(), Platform::Pc,
                                          "hello_triangle", "hello_triangle");
    AssetEntry asset;
    asset.source = "assets/source/cube.gltf";
    asset.output = "assets/build/cube.mesh";
    asset.type = AssetType::Mesh;
    project.assets.push_back(asset);
    std::string error;
    EXPECT_TRUE(removeAsset(dir.str(), &project, 0, false, &error, Lang::Ru));
    EXPECT_TRUE(project.assets.empty());
}
