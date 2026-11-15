// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: semver/compatibility.
#include <gtest/gtest.h>

#include "version.h"

using namespace rpe::ui;

TEST(VersionTest, ParsesStrictSemVer)
{
    SemVer version;
    ASSERT_TRUE(parseSemVer("0.3.0", &version));
    EXPECT_EQ(version.major, 0);
    EXPECT_EQ(version.minor, 3);
    EXPECT_EQ(version.patch, 0);
    EXPECT_FALSE(parseSemVer("0.3", &version));
    EXPECT_FALSE(parseSemVer("0.3.0-rc1", &version));
    EXPECT_FALSE(parseSemVer("v0.3.0", &version));
}

TEST(VersionTest, Compares)
{
    SemVer a;
    SemVer b;
    ASSERT_TRUE(parseSemVer("0.3.0", &a));
    ASSERT_TRUE(parseSemVer("0.2.2", &b));
    EXPECT_GT(compareSemVer(a, b), 0);
    EXPECT_LT(compareSemVer(b, a), 0);
    EXPECT_EQ(compareSemVer(a, a), 0);
}

TEST(VersionTest, CompatibilityPolicy)
{
    std::string detail;
    EXPECT_EQ(checkVersionCompatibility("0.3.0", "0.3.0", &detail),
              VersionCompatibility::Compatible);
    EXPECT_EQ(checkVersionCompatibility("0.3.0", "0.3.1", &detail),
              VersionCompatibility::PatchMismatch);
    EXPECT_EQ(checkVersionCompatibility("0.3.0", "0.2.9", &detail),
              VersionCompatibility::MinorMismatch);
    EXPECT_EQ(checkVersionCompatibility("0.3.0", "1.0.0", &detail),
              VersionCompatibility::MajorMismatch);
    EXPECT_EQ(checkVersionCompatibility("bad", "0.3.0", &detail), VersionCompatibility::Invalid);
}
