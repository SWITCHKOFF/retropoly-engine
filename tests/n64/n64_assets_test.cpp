// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/n64_assets_test.cpp
//
// Boot-загрузка .mesh/.tex v1 из DFS: committed-ассеты, размеры, rollback
// persistent-арены при malformed/truncated входе и missing-файлах.
#include <gtest/gtest.h>

#include "fake_n64_bridge.h"
#include "hal/n64/n64_assets.h"
#include "hal/n64/n64_fs.h"
#include "hal/n64/n64_memory.h"
#include "hal/platform.h"

#include <stdio.h>

#include <string>
#include <vector>

namespace {

std::vector<uint8_t> read_host_file(const char* relative)
{
    const std::string path = std::string(RPE_TEST_SOURCE_DIR) + "/" + relative;
    std::vector<uint8_t> data;
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return data;
    }
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size > 0) {
        data.resize((size_t)size);
        const size_t read = fread(data.data(), 1u, data.size(), file);
        data.resize(read);
    }
    fclose(file);
    return data;
}

class N64AssetsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Platform_Shutdown();
        rpe_n64_fake_reset();
        n64::fs_shutdown();
        n64::memory_shutdown();
        ASSERT_TRUE(n64::memory_init());
        ASSERT_TRUE(n64::fs_init());
        n64::assets_shutdown();
    }

    void TearDown() override
    {
        n64::assets_shutdown();
        n64::fs_shutdown();
        n64::memory_shutdown();
    }
};

}  // namespace

TEST_F(N64AssetsTest, CommittedMeshAndTextureLoadFromDfs)
{
    const std::vector<uint8_t> mesh = read_host_file("assets/build/cube.mesh");
    const std::vector<uint8_t> tex = read_host_file("assets/build/texture_0.tex");
    ASSERT_FALSE(mesh.empty());
    ASSERT_FALSE(tex.empty());
    ASSERT_LE(tex.size(), (size_t)n64::FS_MAX_FILE_BYTES);

    rpe_n64_fake_add_file("CUBE.MSH", mesh.data(), (uint32_t)mesh.size(), -1);
    rpe_n64_fake_add_file("TEX0.TEX", tex.data(), (uint32_t)tex.size(), -1);

    EXPECT_TRUE(n64::assets_load_boot());
    const n64::BootAssets& assets = n64::boot_assets();
    EXPECT_TRUE(assets.mesh_ok);
    EXPECT_TRUE(assets.texture_ok);
    ASSERT_NE(assets.mesh, nullptr);
    ASSERT_NE(assets.texture, nullptr);
    EXPECT_EQ(assets.mesh_file_bytes, mesh.size());
    EXPECT_EQ(assets.texture_file_bytes, tex.size());
    EXPECT_EQ(assets.mesh->vertex_count, 24u);
    EXPECT_EQ(assets.mesh->index_count, 12u);
    EXPECT_EQ(assets.texture->width, 256u);
    EXPECT_EQ(assets.texture->height, 256u);
    EXPECT_GT(n64::asset_arena().used(), tex.size());
    EXPECT_EQ(n64::fs_staging_used(), 0u);
}

TEST_F(N64AssetsTest, MalformedMeshDoesNotLeakArena)
{
    const uint8_t truncated[10] = {'M', 'E', 'S', 'H', 1u, 0u, 0u, 0u, 0u, 0u};
    rpe_n64_fake_add_file("BAD.MSH", truncated, sizeof(truncated), -1);

    MeshAsset* mesh = nullptr;
    uint32_t error = ASSET_ERR_OK;
    EXPECT_FALSE(n64::assets_load_mesh("BAD.MSH", &mesh, &error));
    EXPECT_EQ(mesh, nullptr);
    EXPECT_EQ(error, ASSET_ERR_TRUNCATED_HEADER);
    EXPECT_EQ(n64::asset_arena().used(), 0u);
    EXPECT_EQ(n64::fs_staging_used(), 0u);
}

TEST_F(N64AssetsTest, TruncatedTextureDoesNotLeakArena)
{
    std::vector<uint8_t> tex = read_host_file("assets/build/texture_0.tex");
    ASSERT_GT(tex.size(), 64u);
    tex.resize(tex.size() - 1u);  // обрезан последний пиксель payload'а
    rpe_n64_fake_add_file("TRUNC.TEX", tex.data(), (uint32_t)tex.size(), -1);

    TextureAsset* texture = nullptr;
    uint32_t error = ASSET_ERR_OK;
    EXPECT_FALSE(n64::assets_load_texture("TRUNC.TEX", &texture, &error));
    EXPECT_EQ(texture, nullptr);
    EXPECT_EQ(error, ASSET_ERR_PAYLOAD_RANGE);
    EXPECT_EQ(n64::asset_arena().used(), 0u);
    EXPECT_EQ(n64::fs_staging_used(), 0u);
}

TEST_F(N64AssetsTest, MissingAssetsAreReportedWithoutAllocation)
{
    MeshAsset* mesh = nullptr;
    uint32_t error = ASSET_ERR_OK;
    EXPECT_FALSE(n64::assets_load_mesh("NOPE.MSH", &mesh, &error));
    EXPECT_EQ(mesh, nullptr);
    EXPECT_EQ(error, ASSET_ERR_TRUNCATED_HEADER);
    EXPECT_EQ(n64::asset_arena().used(), 0u);

    EXPECT_FALSE(n64::assets_load_boot());
    EXPECT_FALSE(n64::boot_assets().mesh_ok);
    EXPECT_FALSE(n64::boot_assets().texture_ok);
    EXPECT_EQ(n64::asset_arena().used(), 0u);
}

TEST_F(N64AssetsTest, ValidMeshThenMissingTextureKeepsOnlyMesh)
{
    const std::vector<uint8_t> mesh = read_host_file("assets/build/cube.mesh");
    ASSERT_FALSE(mesh.empty());
    rpe_n64_fake_add_file("CUBE.MSH", mesh.data(), (uint32_t)mesh.size(), -1);

    EXPECT_FALSE(n64::assets_load_boot());
    EXPECT_TRUE(n64::boot_assets().mesh_ok);
    EXPECT_FALSE(n64::boot_assets().texture_ok);
    EXPECT_GT(n64::asset_arena().used(), 0u);
    EXPECT_EQ(n64::fs_staging_used(), 0u);
}
