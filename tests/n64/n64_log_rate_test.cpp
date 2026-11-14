// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/n64_log_rate_test.cpp
//
// Rate limit debug-канала N64: фиксированное окно, счётчик dropped,
// wrap-safe переход через границу uint32.
#include <gtest/gtest.h>

#include "hal/n64/n64_log_rate.h"

#include <stdint.h>

namespace {

class N64LogRateTest : public ::testing::Test {
protected:
    void SetUp() override { rpe_n64_log_rate_reset(); }
    void TearDown() override { rpe_n64_log_rate_reset(); }
};

}  // namespace

TEST_F(N64LogRateTest, LimitIsEnforcedPerWindow)
{
    for (uint32_t i = 0u; i < RPE_N64_LOG_RATE_LIMIT; ++i) {
        EXPECT_TRUE(rpe_n64_log_rate_allow(1000u)) << "message " << i;
    }
    EXPECT_FALSE(rpe_n64_log_rate_allow(1000u));
    EXPECT_EQ(rpe_n64_log_rate_dropped(), 1u);

    // Новое окно — снова доступно.
    EXPECT_TRUE(rpe_n64_log_rate_allow(2000u));
    EXPECT_EQ(rpe_n64_log_rate_dropped(), 1u);
}

TEST_F(N64LogRateTest, WindowRolloverIsWrapSafe)
{
    const uint32_t start = UINT32_MAX - 500u;
    EXPECT_TRUE(rpe_n64_log_rate_allow(start));  // первое сообщение окна
    for (uint32_t i = 1u; i < RPE_N64_LOG_RATE_LIMIT; ++i) {
        EXPECT_TRUE(rpe_n64_log_rate_allow(start + 500u));  // окно через wrap
    }
    EXPECT_FALSE(rpe_n64_log_rate_allow(start + 500u));
    EXPECT_EQ(rpe_n64_log_rate_dropped(), 1u);

    // Ровно 1000 мс от старта окна — окно сброшено.
    EXPECT_TRUE(rpe_n64_log_rate_allow(start + 1000u));
}

TEST_F(N64LogRateTest, ResetClearsCounters)
{
    for (uint32_t i = 0u; i < RPE_N64_LOG_RATE_LIMIT; ++i) {
        EXPECT_TRUE(rpe_n64_log_rate_allow(0u));
    }
    EXPECT_FALSE(rpe_n64_log_rate_allow(0u));
    rpe_n64_log_rate_reset();
    EXPECT_EQ(rpe_n64_log_rate_dropped(), 0u);
    EXPECT_TRUE(rpe_n64_log_rate_allow(0u));
}
