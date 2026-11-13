// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: semver/compatibility.
#include <gtest/gtest.h>

#include "version.h"

using namespace rpe::ui;

TEST(VersionTest, ParsesStrictSemVer)
{
    SemVer version;
    ASSERT_TRUE(parseSemVer("0.2.1", &version));
    EXPECT_EQ(version.major, 0);
    EXPECT_EQ(version.minor, 2);
    EXPECT_EQ(version.patch, 1);
    EXPECT_FALSE(parseSemVer("0.2", &version));
    EXPECT_FALSE(parseSemVer("0.2.1-rc1", &version));
    EXPECT_FALSE(parseSemVer("v0.2.1", &version));
}

TEST(VersionTest, Compares)
{
    SemVer a;
    SemVer b;
    ASSERT_TRUE(parseSemVer("0.2.1", &a));
    ASSERT_TRUE(parseSemVer("0.2.2", &b));
    EXPECT_LT(compareSemVer(a, b), 0);
    EXPECT_GT(compareSemVer(b, a), 0);
    EXPECT_EQ(compareSemVer(a, a), 0);
}

TEST(VersionTest, CompatibilityPolicy)
{
    std::string detail;
    EXPECT_EQ(checkVersionCompatibility("0.2.1", "0.2.1", &detail),
              VersionCompatibility::Compatible);
    EXPECT_EQ(checkVersionCompatibility("0.2.1", "0.2.2", &detail),
              VersionCompatibility::PatchMismatch);
    EXPECT_EQ(checkVersionCompatibility("0.2.1", "0.3.0", &detail),
              VersionCompatibility::MinorMismatch);
    EXPECT_EQ(checkVersionCompatibility("0.2.1", "1.0.0", &detail),
              VersionCompatibility::MajorMismatch);
    EXPECT_EQ(checkVersionCompatibility("bad", "0.2.1", &detail), VersionCompatibility::Invalid);
}
