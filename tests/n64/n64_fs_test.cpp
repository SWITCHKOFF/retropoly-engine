// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/n64_fs_test.cpp
//
// Read-only DFS HAL: нормализация имён, точное чтение, open/read/close,
// malformed/truncated/oversize, отсутствие утечек staging-арены и
// освобождение в произвольном порядке.
#include <gtest/gtest.h>

#include "fake_n64_bridge.h"
#include "hal/n64/n64_fs.h"
#include "hal/n64/n64_memory.h"
#include "hal/platform.h"

#include <string.h>

namespace {

const uint8_t kFileBytes[] = {0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u, 0x70u, 0x80u};

class N64FsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Platform_Shutdown();
        rpe_n64_fake_reset();
        n64::fs_shutdown();
        n64::memory_shutdown();
        ASSERT_TRUE(n64::memory_init());
        ASSERT_TRUE(n64::fs_init());
    }

    void TearDown() override
    {
        n64::fs_shutdown();
        n64::memory_shutdown();
    }
};

}  // namespace

TEST_F(N64FsTest, PathNormalizationIsStrictAndUppercase)
{
    char name[n64::FS_NAME_MAX + 1u];
    EXPECT_EQ(n64::fs_path_normalize("cube.msh", name), n64::FS_OK);
    EXPECT_STREQ(name, "CUBE.MSH");
    EXPECT_EQ(n64::fs_path_normalize("TEX0.TEX", name), n64::FS_OK);
    EXPECT_STREQ(name, "TEX0.TEX");
    EXPECT_EQ(n64::fs_path_normalize("triangle-1.mesh", name), n64::FS_OK);
    EXPECT_STREQ(name, "TRIANGLE-1.MESH");

    EXPECT_EQ(n64::fs_path_normalize(nullptr, name), n64::FS_ERR_NULL_ARGUMENT);
    EXPECT_EQ(n64::fs_path_normalize("", name), n64::FS_ERR_INVALID_PATH);
    EXPECT_EQ(n64::fs_path_normalize("..", name), n64::FS_ERR_INVALID_PATH);
    EXPECT_EQ(n64::fs_path_normalize("A/B.MSH", name), n64::FS_ERR_INVALID_PATH);
    EXPECT_EQ(n64::fs_path_normalize("/ABS.MSH", name), n64::FS_ERR_INVALID_PATH);
    EXPECT_EQ(n64::fs_path_normalize("WITH SPACE.TEX", name), n64::FS_ERR_INVALID_PATH);
    EXPECT_EQ(n64::fs_path_normalize(".HIDDEN", name), n64::FS_ERR_INVALID_PATH);
    EXPECT_EQ(n64::fs_path_normalize("TRAILING.", name), n64::FS_ERR_INVALID_PATH);
    EXPECT_EQ(n64::fs_path_normalize("A..B", name), n64::FS_ERR_INVALID_PATH);
    EXPECT_EQ(n64::fs_path_normalize("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456", name),
              n64::FS_ERR_INVALID_PATH);
    EXPECT_STREQ(name, "");
}

TEST_F(N64FsTest, ValidFileIsLoadedAndFreed)
{
    rpe_n64_fake_add_file("CUBE.MSH", kFileBytes, sizeof(kFileBytes), -1);

    n64::FsDiag diag = n64::FS_ERR_COUNT;
    FileData file = n64::fs_load_file("cube.msh", &diag);
    ASSERT_NE(file.data, nullptr);
    EXPECT_EQ(diag, n64::FS_OK);
    EXPECT_EQ(file.size, sizeof(kFileBytes));
    EXPECT_EQ(memcmp(file.data, kFileBytes, sizeof(kFileBytes)), 0);
    EXPECT_EQ(n64::fs_staging_used(), sizeof(kFileBytes));

    EXPECT_TRUE(n64::fs_free_file(&file, &diag));
    EXPECT_EQ(diag, n64::FS_OK);
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(file.size, 0u);
    EXPECT_EQ(n64::fs_staging_used(), 0u);
    // Peak сохраняется для overlay/профилировщика.
    EXPECT_GE(n64::fs_staging_arena().peak(), sizeof(kFileBytes));
}

TEST_F(N64FsTest, MissingEmptyOversizeAndTruncatedAreRejected)
{
    n64::FsDiag diag = n64::FS_ERR_COUNT;

    FileData missing = n64::fs_load_file("NOPE.MSH", &diag);
    EXPECT_EQ(missing.data, nullptr);
    EXPECT_EQ(diag, n64::FS_ERR_NOT_FOUND);
    EXPECT_EQ(n64::fs_staging_used(), 0u);

    rpe_n64_fake_add_file("EMPTY.MSH", nullptr, 0u, 0);
    FileData empty = n64::fs_load_file("EMPTY.MSH", &diag);
    EXPECT_EQ(empty.data, nullptr);
    EXPECT_EQ(diag, n64::FS_ERR_EMPTY_FILE);
    EXPECT_EQ(n64::fs_staging_used(), 0u);

    rpe_n64_fake_add_file("HUGE.MSH", kFileBytes, sizeof(kFileBytes),
                          (int32_t)(n64::FS_MAX_FILE_BYTES + 1u));
    FileData huge = n64::fs_load_file("HUGE.MSH", &diag);
    EXPECT_EQ(huge.data, nullptr);
    EXPECT_EQ(diag, n64::FS_ERR_TOO_LARGE);
    EXPECT_EQ(n64::fs_staging_used(), 0u);

    // «Лживый» размер: 8 байт заявлено, 4 реально → short read.
    rpe_n64_fake_add_file("TRUNC.MSH", kFileBytes, 4u, 8);
    FileData truncated = n64::fs_load_file("TRUNC.MSH", &diag);
    EXPECT_EQ(truncated.data, nullptr);
    EXPECT_EQ(diag, n64::FS_ERR_READ_FAIL);
    EXPECT_EQ(n64::fs_staging_used(), 0u);
}

TEST_F(N64FsTest, OpenReadCloseIsExact)
{
    rpe_n64_fake_add_file("CUBE.MSH", kFileBytes, sizeof(kFileBytes), -1);

    n64::FileHandle handle;
    ASSERT_EQ(n64::fs_open("CUBE.MSH", &handle), n64::FS_OK);

    uint8_t first[4] = {};
    uint32_t read = 0u;
    EXPECT_EQ(n64::fs_read(&handle, first, sizeof(first), &read), n64::FS_OK);
    EXPECT_EQ(read, 4u);
    EXPECT_EQ(memcmp(first, kFileBytes, 4u), 0);

    uint8_t second[4] = {};
    EXPECT_EQ(n64::fs_read(&handle, second, sizeof(second), &read), n64::FS_OK);
    EXPECT_EQ(memcmp(second, kFileBytes + 4, 4u), 0);

    uint8_t past[1] = {};
    EXPECT_EQ(n64::fs_read(&handle, past, sizeof(past), &read), n64::FS_ERR_TRUNCATED);
    EXPECT_EQ(read, 0u);

    EXPECT_EQ(n64::fs_close(&handle), n64::FS_OK);
    EXPECT_EQ(n64::fs_read(&handle, first, sizeof(first), &read), n64::FS_ERR_BAD_HANDLE);
    EXPECT_EQ(n64::fs_close(&handle), n64::FS_ERR_BAD_HANDLE);
    EXPECT_EQ(n64::fs_staging_used(), 0u);
}

TEST_F(N64FsTest, ReadRangeAndNullArgumentsAreSafe)
{
    rpe_n64_fake_add_file("DATA.BIN", kFileBytes, sizeof(kFileBytes), -1);

    uint8_t out[4] = {};
    uint32_t read = 0u;
    EXPECT_EQ(n64::fs_read_range("DATA.BIN", 2u, out, sizeof(out), &read), n64::FS_OK);
    EXPECT_EQ(read, 4u);
    EXPECT_EQ(memcmp(out, kFileBytes + 2, 4u), 0);

    EXPECT_EQ(n64::fs_read_range("DATA.BIN", 6u, out, 4u, &read), n64::FS_ERR_TRUNCATED);
    EXPECT_EQ(read, 0u);

    EXPECT_EQ(n64::fs_read_range(nullptr, 0u, out, 1u, &read), n64::FS_ERR_NULL_ARGUMENT);
    EXPECT_EQ(n64::fs_open(nullptr, nullptr), n64::FS_ERR_NULL_ARGUMENT);

    n64::FileHandle handle;
    ASSERT_EQ(n64::fs_open("DATA.BIN", &handle), n64::FS_OK);
    EXPECT_EQ(n64::fs_read(&handle, nullptr, 1u, &read), n64::FS_ERR_NULL_ARGUMENT);
    EXPECT_EQ(n64::fs_close(&handle), n64::FS_OK);
}

TEST_F(N64FsTest, ReadFailureRollsBackStaging)
{
    rpe_n64_fake_add_file("BROKEN.MSH", kFileBytes, sizeof(kFileBytes), -1);
    rpe_n64_fake_fail_read("BROKEN.MSH", 1);

    n64::FsDiag diag = n64::FS_ERR_COUNT;
    FileData file = n64::fs_load_file("BROKEN.MSH", &diag);
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(diag, n64::FS_ERR_READ_FAIL);
    EXPECT_EQ(n64::fs_staging_used(), 0u);
}

TEST_F(N64FsTest, OutOfOrderFreeReclaimsAllTopBlocks)
{
    rpe_n64_fake_add_file("A.BIN", kFileBytes, sizeof(kFileBytes), -1);
    rpe_n64_fake_add_file("B.BIN", kFileBytes, sizeof(kFileBytes), -1);

    n64::FsDiag diag = n64::FS_ERR_COUNT;
    FileData a = n64::fs_load_file("A.BIN", &diag);
    FileData b = n64::fs_load_file("B.BIN", &diag);
    ASSERT_NE(a.data, nullptr);
    ASSERT_NE(b.data, nullptr);
    const size_t both = n64::fs_staging_used();
    EXPECT_EQ(both, 2u * sizeof(kFileBytes));

    EXPECT_TRUE(n64::fs_free_file(&a, &diag));
    EXPECT_EQ(n64::fs_staging_used(), both);  // нижний блок — дыра до free верхнего

    EXPECT_TRUE(n64::fs_free_file(&b, &diag));
    EXPECT_EQ(n64::fs_staging_used(), 0u);
}

TEST_F(N64FsTest, ForeignBufferIsRejected)
{
    uint8_t local[8] = {};
    FileData foreign = {local, sizeof(local)};
    n64::FsDiag diag = n64::FS_ERR_COUNT;
    EXPECT_FALSE(n64::fs_free_file(&foreign, &diag));
    EXPECT_EQ(diag, n64::FS_ERR_FOREIGN_BUFFER);
    EXPECT_EQ(foreign.data, local);
    EXPECT_EQ(foreign.size, sizeof(local));
}

TEST_F(N64FsTest, FsInitFailureIsExplicitAndSafe)
{
    n64::fs_shutdown();
    rpe_n64_fake_fs_init_result = 0;
    EXPECT_FALSE(n64::fs_init());
    EXPECT_FALSE(n64::fs_ready());

    n64::FsDiag diag = n64::FS_ERR_COUNT;
    FileData file = n64::fs_load_file("CUBE.MSH", &diag);
    EXPECT_EQ(file.data, nullptr);
    EXPECT_EQ(diag, n64::FS_ERR_NOT_AVAILABLE);

    n64::FileHandle handle;
    EXPECT_EQ(n64::fs_open("CUBE.MSH", &handle), n64::FS_ERR_NOT_INITIALIZED);
}
