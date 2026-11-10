// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_fs_test.cpp
//
// CD-ROM FS-ядро: валидный файл, отсутствие, empty/truncated/oversize,
// host-путь, allocation failure, освобождение и 0 байт persistent-памяти
// при любой ошибке.
#include "hal/platform.h"
#include "hal/saturn/saturn_fs.h"
#include "hal/saturn/saturn_memory.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

#include <string.h>

namespace {

const uint8_t kFileBytes[] = {0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u, 0x70u, 0x80u};

bool source_available(void)
{
    return rpe_saturn_fs_available();
}

bool source_get_size(const char* name, uint32_t* out_size)
{
    return rpe_saturn_fs_get_size(name, out_size);
}

bool source_read(const char* name, uint8_t* dst, uint32_t size)
{
    return rpe_saturn_fs_read(name, dst, size);
}

const saturn::FileSource kTestSource = {source_available, source_get_size, source_read};

class SaturnFsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fake_saturn::reset();
        saturn::fs_shutdown();
        saturn::memory_shutdown();
        ASSERT_TRUE(saturn::memory_init());
        fake_saturn::fs_available = true;
        ASSERT_TRUE(saturn::fs_init(&kTestSource));
    }

    void TearDown() override
    {
        saturn::fs_shutdown();
        saturn::memory_shutdown();
    }
};

}  // namespace

TEST_F(SaturnFsTest, ValidFileIsLoadedAndFreed)
{
    fake_saturn::add_file("CUBE.MSH", kFileBytes, sizeof(kFileBytes));

    saturn::FsDiag diag = saturn::FS_ERR_COUNT;
    FileData file = saturn::fs_load_file("cube.msh", &diag);
    ASSERT_NE(file.data, nullptr);
    EXPECT_EQ(diag, saturn::FS_OK);
    EXPECT_EQ(file.size, sizeof(kFileBytes));
    EXPECT_EQ(memcmp(file.data, kFileBytes, sizeof(kFileBytes)), 0);
    EXPECT_GT(saturn::asset_heap().used(), 0u);

    EXPECT_TRUE(saturn::fs_free_file(&file, &diag));
    EXPECT_EQ(diag, saturn::FS_OK);
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(file.size, 0u);
    EXPECT_EQ(saturn::asset_heap().used(), 0u);
}

TEST_F(SaturnFsTest, MissingFileLeavesHeapUntouched)
{
    saturn::FsDiag diag = saturn::FS_ERR_COUNT;
    FileData file = saturn::fs_load_file("NOPE.MSH", &diag);
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(file.size, 0u);
    EXPECT_EQ(diag, saturn::FS_ERR_NOT_FOUND);
    EXPECT_EQ(saturn::asset_heap().used(), 0u);
}

TEST_F(SaturnFsTest, EmptyFileIsRejected)
{
    fake_saturn::add_file("EMPTY.MSH", nullptr, 0u, 0u, true);

    saturn::FsDiag diag = saturn::FS_ERR_COUNT;
    FileData file = saturn::fs_load_file("EMPTY.MSH", &diag);
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(diag, saturn::FS_ERR_EMPTY_FILE);
    EXPECT_EQ(saturn::asset_heap().used(), 0u);
}

TEST_F(SaturnFsTest, TruncatedFileIsRejectedAndRolledBack)
{
    fake_saturn::add_file("TRUNC.MSH", kFileBytes, 4u, 8u, true);

    saturn::FsDiag diag = saturn::FS_ERR_COUNT;
    FileData file = saturn::fs_load_file("TRUNC.MSH", &diag);
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(diag, saturn::FS_ERR_READ_FAIL);
    EXPECT_EQ(saturn::asset_heap().used(), 0u);
}

TEST_F(SaturnFsTest, OversizeAndAllocationFailureAreRejected)
{
    fake_saturn::add_file("HUGE.MSH", nullptr, 0u, saturn::FS_MAX_FILE_BYTES + 1u, false);

    saturn::FsDiag diag = saturn::FS_ERR_COUNT;
    FileData huge = saturn::fs_load_file("HUGE.MSH", &diag);
    EXPECT_EQ(huge.data, nullptr);
    EXPECT_EQ(diag, saturn::FS_ERR_TOO_LARGE);
    EXPECT_EQ(saturn::asset_heap().used(), 0u);

    fake_saturn::add_file("BIG.MSH", nullptr, 0u, saturn::ASSET_HEAP_BYTES + 1u, false);
    FileData big = saturn::fs_load_file("BIG.MSH", &diag);
    EXPECT_EQ(big.data, nullptr);
    EXPECT_EQ(diag, saturn::FS_ERR_ALLOC_FAIL);
    EXPECT_EQ(saturn::asset_heap().used(), 0u);
}

TEST_F(SaturnFsTest, HostPathsAndIsoSuffixAreRejected)
{
    const char* paths[] = {"../CUBE.MSH", "C:\\CUBE.MSH", "CUBE.MSH;1", "DIR/CUBE.MSH"};
    for (const char* path : paths) {
        saturn::FsDiag diag = saturn::FS_ERR_COUNT;
        FileData file = saturn::fs_load_file(path, &diag);
        EXPECT_EQ(file.data, nullptr) << path;
        EXPECT_EQ(diag, saturn::FS_ERR_INVALID_PATH) << path;
        EXPECT_EQ(saturn::asset_heap().used(), 0u) << path;
    }
}

TEST_F(SaturnFsTest, PublicWrappersAndForeignFree)
{
    fake_saturn::add_file("TEX0.TEX", kFileBytes, sizeof(kFileBytes));

    FileData file = FileSystem_LoadFile("TEX0.TEX");
    ASSERT_NE(file.data, nullptr);
    EXPECT_EQ(file.size, sizeof(kFileBytes));
    FileSystem_FreeFile(&file);
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(saturn::asset_heap().used(), 0u);

    uint8_t local[4] = {1u, 2u, 3u, 4u};
    FileData foreign = {local, sizeof(local)};
    saturn::FsDiag diag = saturn::FS_OK;
    EXPECT_FALSE(saturn::fs_free_file(&foreign, &diag));
    EXPECT_EQ(diag, saturn::FS_ERR_FOREIGN_FREE);
    EXPECT_EQ(foreign.data, nullptr);
    EXPECT_EQ(foreign.size, 0u);

    EXPECT_FALSE(saturn::fs_free_file(nullptr, &diag));
    EXPECT_EQ(diag, saturn::FS_ERR_NULL_ARGUMENT);
}

TEST_F(SaturnFsTest, SourceUnavailableIsReported)
{
    fake_saturn::fs_available = false;
    EXPECT_FALSE(saturn::fs_available());

    saturn::FsDiag diag = saturn::FS_ERR_COUNT;
    FileData file = saturn::fs_load_file("CUBE.MSH", &diag);
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(diag, saturn::FS_ERR_NOT_AVAILABLE);
}
