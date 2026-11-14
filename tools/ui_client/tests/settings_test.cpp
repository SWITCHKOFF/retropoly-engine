// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: settings/recent projects.
#include <gtest/gtest.h>

#include <cstdlib>

#include "settings.h"
#include "fs_util.h"
#include "test_util.h"

using namespace rpe::ui;
using namespace rpe::ui::test;

namespace {

class SettingsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        const char* previous = std::getenv("RPE_UI_CONFIG_DIR");
        had_previous_ = previous != nullptr;
        if (had_previous_) previous_ = previous;
        setEnv("RPE_UI_CONFIG_DIR", dir_.str());
    }
    void TearDown() override
    {
        if (had_previous_) {
            setEnv("RPE_UI_CONFIG_DIR", previous_);
        } else {
            unsetEnv("RPE_UI_CONFIG_DIR");
        }
    }

    TempDir dir_;
    bool had_previous_ = false;
    std::string previous_;
};

} // namespace

TEST_F(SettingsTest, RoundTripAndRecent)
{
    AppSettings settings;
    settings.sdk_root = "/opt/sdk";
    settings.sdk_pc = "/opt/sdk/pc";
    settings.emulator_ps1 = "/usr/bin/duckstation";
    settings.theme = "light";
    settings.language = Lang::En;
    addRecentProject(&settings, "/tmp/p1/.rpeproject");
    addRecentProject(&settings, "/tmp/p2/.rpeproject");
    addRecentProject(&settings, "/tmp/p1/.rpeproject");
    ASSERT_EQ(settings.recent_projects.size(), 2u);
    EXPECT_EQ(fileName(settings.recent_projects.front()), ".rpeproject");

    std::string error;
    ASSERT_TRUE(saveSettings(settings, &error)) << error;
    std::string warning;
    const AppSettings loaded = loadSettings(&warning);
    EXPECT_TRUE(warning.empty());
    EXPECT_EQ(loaded.sdk_root, "/opt/sdk");
    EXPECT_EQ(loaded.theme, "light");
    EXPECT_EQ(loaded.language, Lang::En);
    EXPECT_EQ(loaded.recent_projects.size(), 2u);
}

TEST_F(SettingsTest, MalformedFileDoesNotCrash)
{
    writeFile(settingsFilePath(), "{not json");
    std::string warning;
    const AppSettings settings = loadSettings(&warning);
    EXPECT_FALSE(warning.empty());
    EXPECT_EQ(settings.schema, 1);
}

TEST_F(SettingsTest, RecentIsCapped)
{
    AppSettings settings;
    for (int i = 0; i < 20; ++i) {
        addRecentProject(&settings, "/tmp/project_" + std::to_string(i) + "/.rpeproject");
    }
    EXPECT_EQ(settings.recent_projects.size(), 10u);
}
