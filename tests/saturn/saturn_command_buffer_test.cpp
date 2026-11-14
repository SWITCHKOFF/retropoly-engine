// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_command_buffer_test.cpp
//
// Каркас командного буфера: явный overflow (false + диагностика), reset,
// alignment и отказ до init.
#include "hal/saturn/saturn_command_buffer.h"

#include <gtest/gtest.h>

#include <stdint.h>

namespace {

alignas(8) uint8_t s_buffer[64];

}  // namespace

TEST(SaturnCommandBuffer, AppendUntilOverflowWithoutSilentDrop)
{
    saturn::CommandBuffer buffer;
    buffer.init(s_buffer, sizeof(s_buffer));

    const uint32_t packet[2] = {0x11223344u, 0x55667788u};
    size_t appended = 0u;
    while (buffer.append(packet, sizeof(packet), 8u)) {
        ++appended;
    }

    EXPECT_EQ(appended, sizeof(s_buffer) / sizeof(packet));
    EXPECT_EQ(buffer.used(), sizeof(s_buffer));
    EXPECT_EQ(buffer.remaining(), 0u);
    EXPECT_EQ(buffer.diag(), saturn::COMMAND_BUFFER_OVERFLOW);
    const size_t used_at_overflow = buffer.used();

    EXPECT_FALSE(buffer.append(packet, sizeof(packet), 8u));
    EXPECT_EQ(buffer.used(), used_at_overflow);  // отказ не меняет состояние
}

TEST(SaturnCommandBuffer, ResetReusesArena)
{
    saturn::CommandBuffer buffer;
    buffer.init(s_buffer, sizeof(s_buffer));

    const uint8_t byte = 0xABu;
    ASSERT_TRUE(buffer.append(&byte, sizeof(byte), 4u));
    EXPECT_GT(buffer.used(), 0u);

    buffer.reset();
    EXPECT_EQ(buffer.used(), 0u);
    EXPECT_EQ(buffer.remaining(), buffer.capacity());
    EXPECT_TRUE(buffer.append(&byte, sizeof(byte), 4u));
}

TEST(SaturnCommandBuffer, InvalidArgumentsAreRejected)
{
    saturn::CommandBuffer buffer;

    EXPECT_FALSE(buffer.append("x", 1u));
    EXPECT_EQ(buffer.diag(), saturn::COMMAND_BUFFER_NOT_INITIALIZED);

    buffer.init(s_buffer, sizeof(s_buffer));
    EXPECT_FALSE(buffer.append(nullptr, 4u));
    EXPECT_EQ(buffer.diag(), saturn::COMMAND_BUFFER_NULL_ARGUMENT);

    EXPECT_FALSE(buffer.append("x", 1u, 3u));
    EXPECT_EQ(buffer.diag(), saturn::COMMAND_BUFFER_BAD_ALIGNMENT);

    EXPECT_TRUE(buffer.append(nullptr, 0u));
    EXPECT_EQ(buffer.used(), 0u);
    EXPECT_STREQ(saturn::CommandBuffer::diag_string(saturn::COMMAND_BUFFER_OVERFLOW), "overflow");
}
