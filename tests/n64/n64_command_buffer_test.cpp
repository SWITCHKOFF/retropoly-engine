// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/n64_command_buffer_test.cpp
//
// Bounded RDP command/display-list arena: append/reset, валидация
// аргументов, явный overflow и сохранение состояния при отказе.
#include <gtest/gtest.h>

#include "fake_n64_bridge.h"
#include "hal/n64/n64_memory.h"
#include "hal/n64/rdp_command.h"
#include "hal/platform.h"

namespace {

class N64CommandBufferTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Platform_Shutdown();
        rpe_n64_fake_reset();
        n64::memory_shutdown();
        n64::rdp_command_buffer().reset();
        ASSERT_TRUE(n64::memory_init());
        n64::rdp_command_buffer().reset();
    }

    void TearDown() override { n64::memory_shutdown(); }
};

}  // namespace

TEST_F(N64CommandBufferTest, AppendStoresRecordsAndResets)
{
    const uint8_t payload[4] = {1u, 2u, 3u, 4u};
    EXPECT_TRUE(n64::rdp_command_buffer().append(n64::RDP_OPCODE_CLEAR, payload, sizeof(payload)));
    EXPECT_TRUE(n64::rdp_command_buffer().append(n64::RDP_OPCODE_BEGIN_FRAME, nullptr, 0u));
    EXPECT_TRUE(n64::rdp_command_buffer().append(n64::RDP_OPCODE_END_FRAME, nullptr, 0u));

    EXPECT_EQ(n64::rdp_command_buffer().count(), 3u);
    EXPECT_GT(n64::rdp_command_buffer().used(), 0u);
    EXPECT_LT(n64::rdp_command_buffer().used(), n64::rdp_command_buffer().capacity());
    EXPECT_EQ(n64::rdp_command_buffer().diag(), n64::RDP_COMMAND_OK);

    const size_t peak = n64::rdp_command_buffer().peak();
    n64::rdp_command_buffer().reset();
    EXPECT_EQ(n64::rdp_command_buffer().used(), 0u);
    EXPECT_EQ(n64::rdp_command_buffer().count(), 0u);
    EXPECT_EQ(n64::rdp_command_buffer().diag(), n64::RDP_COMMAND_OK);
    EXPECT_GE(peak, n64::rdp_command_buffer().used() + 1u);
}

TEST_F(N64CommandBufferTest, InvalidArgumentsAreRejected)
{
    const uint8_t payload[4] = {};

    EXPECT_FALSE(n64::rdp_command_buffer().append(0u, payload, sizeof(payload)));
    EXPECT_EQ(n64::rdp_command_buffer().diag(), n64::RDP_COMMAND_NULL_ARGUMENT);

    EXPECT_FALSE(n64::rdp_command_buffer().append(n64::RDP_OPCODE_CLEAR, nullptr, 4u));
    EXPECT_EQ(n64::rdp_command_buffer().diag(), n64::RDP_COMMAND_NULL_ARGUMENT);

    EXPECT_FALSE(n64::rdp_command_buffer().append(n64::RDP_OPCODE_CLEAR, payload, 4u, 3u));
    EXPECT_EQ(n64::rdp_command_buffer().diag(), n64::RDP_COMMAND_BAD_ALIGNMENT);

    EXPECT_EQ(n64::rdp_command_buffer().used(), 0u);
    EXPECT_EQ(n64::rdp_command_buffer().count(), 0u);
}

TEST_F(N64CommandBufferTest, NotInitializedIsExplicit)
{
    n64::memory_shutdown();
    n64::rdp_command_buffer().reset();
    EXPECT_FALSE(n64::rdp_command_buffer().append(n64::RDP_OPCODE_CLEAR, nullptr, 0u));
    EXPECT_EQ(n64::rdp_command_buffer().diag(), n64::RDP_COMMAND_NOT_INITIALIZED);
}

TEST_F(N64CommandBufferTest, OverflowDoesNotPartiallyWrite)
{
    uint8_t payload[600] = {};
    while (n64::rdp_command_buffer().append(n64::RDP_OPCODE_CLEAR, payload, sizeof(payload))) {
    }
    EXPECT_EQ(n64::rdp_command_buffer().diag(), n64::RDP_COMMAND_OVERFLOW);

    const size_t used = n64::rdp_command_buffer().used();
    const uint32_t count = n64::rdp_command_buffer().count();
    EXPECT_FALSE(n64::rdp_command_buffer().append(n64::RDP_OPCODE_CLEAR, payload, sizeof(payload)));
    EXPECT_EQ(n64::rdp_command_buffer().used(), used);
    EXPECT_EQ(n64::rdp_command_buffer().count(), count);
    EXPECT_LE(used, n64::rdp_command_buffer().capacity());
}
