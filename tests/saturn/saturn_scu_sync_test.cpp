// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_scu_sync_test.cpp
//
// S25–26: shared-атомики, bounded wait и FRT-профилирование.
#include "hal/saturn/scu_sync.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

namespace {

class ScuSyncTest : public ::testing::Test {
protected:
    void SetUp() override { fake_saturn::reset(); }
};

}  // namespace

TEST_F(ScuSyncTest, SharedLoadStoreRoundTrip)
{
    volatile uint32_t cell = 0u;
    saturn::shared_store_u32(&cell, 0xDEADBEEFu);
    EXPECT_EQ(saturn::shared_load_u32(&cell), 0xDEADBEEFu);

    saturn::shared_store_u32(&cell, 0u);
    EXPECT_EQ(saturn::shared_load_u32(&cell), 0u);
}

TEST_F(ScuSyncTest, WaitReturnsImmediatelyWhenValueMatches)
{
    volatile uint32_t cell = 7u;
    saturn::ScuSyncDiag diag = saturn::SCU_SYNC_DISABLED;
    EXPECT_TRUE(saturn::wait_u32_until(&cell, 7u, 10u, &diag));
    EXPECT_EQ(diag, saturn::SCU_SYNC_OK);
    EXPECT_EQ(fake_saturn::cpu_relax_calls, 0);
}

TEST_F(ScuSyncTest, WaitTimesOutWithoutInfiniteBusyWait)
{
    volatile uint32_t cell = 0u;
    saturn::ScuSyncDiag diag = saturn::SCU_SYNC_OK;
    EXPECT_FALSE(saturn::wait_u32_until(&cell, 1u, 5u, &diag));
    EXPECT_EQ(diag, saturn::SCU_SYNC_TIMEOUT);
    EXPECT_GT(fake_saturn::cpu_relax_calls, 0);
}

TEST_F(ScuSyncTest, NullCellIsRejected)
{
    saturn::ScuSyncDiag diag = saturn::SCU_SYNC_OK;
    EXPECT_FALSE(saturn::wait_u32_until(nullptr, 1u, 5u, &diag));
    EXPECT_EQ(diag, saturn::SCU_SYNC_NULL_ARGUMENT);
}

TEST_F(ScuSyncTest, ProfileElapsedHandlesFrtWrap)
{
    // 0xFF00 → 0x0100 = 512 ticks; 1 tick = 32 такта.
    const uint32_t us = saturn::profile_elapsed_us(0xFF00u, 0x0100u);
    EXPECT_EQ(us, (512u * 1000u) / 839u);
}

TEST_F(ScuSyncTest, SharedPointerAndCachePurgeGoThroughBridge)
{
    uint32_t value = 0u;
    EXPECT_EQ(saturn::shared_ptr(&value), static_cast<void*>(&value));
    saturn::cache_invalidate_local();
    EXPECT_EQ(fake_saturn::cache_through_calls, 1);
    EXPECT_EQ(fake_saturn::cache_purge_calls, 1);
}

TEST_F(ScuSyncTest, DiagnosticStringsAreStable)
{
    EXPECT_STREQ(saturn::diag_string(saturn::SCU_SYNC_OK), "ok");
    EXPECT_STREQ(saturn::diag_string(saturn::SCU_SYNC_NULL_ARGUMENT), "null argument");
    EXPECT_STREQ(saturn::diag_string(saturn::SCU_SYNC_TIMEOUT), "timeout");
    EXPECT_STREQ(saturn::diag_string(saturn::SCU_SYNC_DISABLED), "disabled");
}
