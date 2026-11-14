// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/n64_memory_test.cpp
//
// Memory ledger N64: 4 MiB base, статические арены, peak, Expansion Pak
// detection, профилировщик и отсутствие роста бюджета на 8 MiB.
#include <gtest/gtest.h>

#include "fake_n64_bridge.h"
#include "hal/n64/n64_memory.h"
#include "hal/platform.h"

#include <string.h>

namespace {

class N64MemoryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Platform_Shutdown();
        rpe_n64_fake_reset();
        n64::memory_shutdown();
    }

    void TearDown() override { n64::memory_shutdown(); }
};

}  // namespace

TEST_F(N64MemoryTest, LedgerFitsFourMiBBudget)
{
    ASSERT_TRUE(n64::memory_init());
    EXPECT_TRUE(n64::memory_ledger_check());
    EXPECT_EQ(n64::memory_rdram_size(), n64::RDRAM_BASE_BYTES);
    EXPECT_FALSE(n64::memory_expansion_pak());

    EXPECT_EQ(n64::asset_arena().capacity(), n64::ASSET_ARENA_BYTES);
    EXPECT_EQ(n64::frame_arena().capacity(), n64::FRAME_ARENA_BYTES);
    EXPECT_EQ(n64::command_arena().capacity(), n64::COMMAND_ARENA_BYTES);
    EXPECT_EQ(n64::fs_staging_arena().capacity(), n64::FS_STAGING_BYTES);
    EXPECT_EQ(n64::ENGINE_ARENAS_BYTES, 2u * 1024u * 1024u);

    const uint32_t ledger = n64::ENGINE_ARENAS_BYTES + n64::CPU_STACK_BYTES +
                            n64::FRAMEBUFFER_BYTES + n64::CODE_DATA_SDK_RESERVE_BYTES;
    EXPECT_EQ(ledger, n64::RDRAM_BASE_BYTES);
}

TEST_F(N64MemoryTest, ExpansionPakIsDetectedButDoesNotGrowArenas)
{
    rpe_n64_fake_rdram_bytes = 8u * 1024u * 1024u;
    ASSERT_TRUE(n64::memory_init());
    EXPECT_TRUE(n64::memory_expansion_pak());
    EXPECT_EQ(n64::asset_arena().capacity(), n64::ASSET_ARENA_BYTES);
    EXPECT_EQ(n64::memory_rdram_size(), 8u * 1024u * 1024u);
    EXPECT_EQ(n64::ENGINE_ARENAS_BYTES, 2u * 1024u * 1024u);
}

TEST_F(N64MemoryTest, InsufficientRdramFailsLedger)
{
    rpe_n64_fake_rdram_bytes = 2u * 1024u * 1024u;
    EXPECT_FALSE(n64::memory_init());
    EXPECT_FALSE(n64::memory_ready());
}

TEST_F(N64MemoryTest, ArenaTracksCurrentAndPeak)
{
    ASSERT_TRUE(n64::memory_init());
    n64::Arena& arena = n64::asset_arena();

    void* first = arena.allocate(100u, 8u);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(arena.used(), 100u);
    EXPECT_GE(arena.peak(), 100u);

    arena.reset();
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.peak(), 0u);

    ASSERT_NE(arena.allocate(48u, 8u), nullptr);
    ASSERT_NE(arena.allocate(16u, 8u), nullptr);
    EXPECT_EQ(arena.used(), 64u);
    EXPECT_EQ(arena.peak(), 64u);

    // Переполнение не меняет used/peak.
    EXPECT_EQ(arena.allocate(n64::ASSET_ARENA_BYTES, 8u), nullptr);
    EXPECT_EQ(arena.used(), 64u);
    EXPECT_EQ(arena.peak(), 64u);
}

TEST_F(N64MemoryTest, OverlayLineIsCompactAndBounded)
{
    ASSERT_TRUE(n64::memory_init());
    ASSERT_NE(n64::asset_arena().allocate(4096u, 8u), nullptr);

    char line[96];
    const size_t written = n64::memory_overlay_line(line, sizeof(line));
    EXPECT_GT(written, 0u);
    EXPECT_LT(written, sizeof(line));
    EXPECT_NE(strstr(line, "A4/4K"), nullptr);
    EXPECT_EQ(strlen(line), written);

    EXPECT_EQ(n64::memory_overlay_line(nullptr, sizeof(line)), 0u);
    EXPECT_EQ(n64::memory_overlay_line(line, 0u), 0u);
}

TEST_F(N64MemoryTest, ShutdownResetsArenas)
{
    ASSERT_TRUE(n64::memory_init());
    ASSERT_NE(n64::asset_arena().allocate(128u, 8u), nullptr);
    n64::memory_shutdown();
    EXPECT_FALSE(n64::memory_ready());
    EXPECT_EQ(n64::asset_arena().used(), 0u);
}
