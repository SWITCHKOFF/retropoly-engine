// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_fs_path_test.cpp
//
// Валидация CD-имён: bare 8.3, uppercase-нормализация, отказ host-путей,
// «;1», `..`, wildcard'ов и неверных длин.
#include "hal/saturn/saturn_fs_path.h"

#include <gtest/gtest.h>

#include <string.h>

namespace {

struct ValidCase
{
    const char* input;
    const char* expected;
};

}  // namespace

TEST(SaturnFsPath, ValidNamesAreNormalizedToUpper8_3)
{
    const ValidCase cases[] = {
        {"CUBE.MSH", "CUBE.MSH"},
        {"cube.msh", "CUBE.MSH"},
        {"TEX0.TEX", "TEX0.TEX"},
        {"0.BIN", "0.BIN"},
        {"A", "A"},
        {"ABCDEFGH.EXT", "ABCDEFGH.EXT"},
        {"tex_0.tex", "TEX_0.TEX"},
    };

    for (const ValidCase& test : cases) {
        char out[saturn::SATURN_FS_NAME_MAX + 1u];
        EXPECT_EQ(saturn::path_normalize(test.input, out), saturn::PATH_OK) << test.input;
        EXPECT_STREQ(out, test.expected) << test.input;
    }
}

TEST(SaturnFsPath, NullEmptyAndTooLongAreRejected)
{
    char out[saturn::SATURN_FS_NAME_MAX + 1u];

    EXPECT_EQ(saturn::path_normalize(nullptr, out), saturn::PATH_ERR_NULL_ARGUMENT);
    EXPECT_EQ(out[0], '\0');

    EXPECT_EQ(saturn::path_normalize("", out), saturn::PATH_ERR_EMPTY);
    EXPECT_EQ(saturn::path_normalize("ABCDEFGHIJKLM", out), saturn::PATH_ERR_TOO_LONG);
    EXPECT_EQ(saturn::path_normalize("ABCDEFGHI.M", out), saturn::PATH_ERR_INVALID_83);
}

TEST(SaturnFsPath, HostPathsAndIsoSuffixAreRejected)
{
    const char* rejected[] = {
        "../CUBE.MSH", "DIR/CUBE.MSH", "DIR\\CUBE.MSH", "C:\\CUBE.MSH", "C:CUBE.MSH", "CUBE.MSH;1",
    };

    for (const char* path : rejected) {
        char out[saturn::SATURN_FS_NAME_MAX + 1u];
        const saturn::PathDiag diag = saturn::path_normalize(path, out);
        EXPECT_NE(diag, saturn::PATH_OK) << path;
        EXPECT_EQ(out[0], '\0') << path;
    }
}

TEST(SaturnFsPath, InvalidDotsAndCharactersAreRejected)
{
    const char* rejected[] = {
        ".HIDDEN", "A.", "..", "A..B", "A B.TXT", "A*B.TXT", "A+B.TXT", "A.LONG",
    };

    for (const char* path : rejected) {
        char out[saturn::SATURN_FS_NAME_MAX + 1u];
        EXPECT_NE(saturn::path_normalize(path, out), saturn::PATH_OK) << path;
    }
}

TEST(SaturnFsPath, NullOutputIsRejected)
{
    EXPECT_EQ(saturn::path_normalize("CUBE.MSH", nullptr), saturn::PATH_ERR_NULL_ARGUMENT);
    EXPECT_STREQ(saturn::path_diag_string(saturn::PATH_ERR_HOST_PATH), "host path or separator");
}
