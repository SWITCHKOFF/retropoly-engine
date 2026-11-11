// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/vdp1_command_list_test.cpp
//
// Командный список VDP1: capacity/END/link-цепочка/overflow/reset.
#include "hal/saturn/vdp1_command_list.h"

#include <gtest/gtest.h>

#include <string.h>

namespace {

using namespace saturn;

alignas(32) uint8_t s_buffer[16u * 1024u];

Vdp1Command make_command(uint16_t ctrl)
{
    Vdp1Command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.ctrl = ctrl;
    return cmd;
}

TEST(Vdp1CommandListTest, InitReservesEndSlot)
{
    Vdp1CommandList list;
    ASSERT_TRUE(list.init(s_buffer, sizeof(s_buffer)));
    EXPECT_TRUE(list.inited());
    EXPECT_EQ(list.capacity(), (sizeof(s_buffer) / sizeof(Vdp1Command)) - 1u);
    EXPECT_EQ(list.count(), 0u);
    EXPECT_EQ(list.total_count(), 0u);
}

TEST(Vdp1CommandListTest, AppendFinalizeBuildsLinkChain)
{
    Vdp1CommandList list;
    ASSERT_TRUE(list.init(s_buffer, sizeof(s_buffer)));
    ASSERT_TRUE(list.append(make_command(VDP1_CMD_POLYGON)));
    ASSERT_TRUE(list.append(make_command(VDP1_CMD_DISTORTED_SPRITE)));
    ASSERT_TRUE(list.append(make_command(VDP1_CMD_POLYGON)));

    ASSERT_TRUE(list.finalize());
    EXPECT_TRUE(list.finalized());
    EXPECT_EQ(list.count(), 3u);
    EXPECT_EQ(list.total_count(), 4u);
    EXPECT_EQ(list.used_bytes(), 3u * sizeof(Vdp1Command));

    const Vdp1Command* c0 = list.command(0u);
    const Vdp1Command* c1 = list.command(1u);
    const Vdp1Command* c2 = list.command(2u);
    ASSERT_NE(c0, nullptr);
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);
    EXPECT_EQ(c0->link, 4u);
    EXPECT_EQ(c1->link, 8u);
    EXPECT_EQ(c2->link, 12u);

    const Vdp1Command* end = list.end_command();
    ASSERT_NE(end, nullptr);
    EXPECT_EQ(end->ctrl, VDP1_CMD_END);
    EXPECT_EQ(end->link, 0u);
}

TEST(Vdp1CommandListTest, LinkRespectsTableBase)
{
    Vdp1CommandList list;
    ASSERT_TRUE(list.init(s_buffer, sizeof(s_buffer)));
    ASSERT_TRUE(list.append(make_command(VDP1_CMD_POLYGON)));
    ASSERT_TRUE(list.finalize(0x20u));
    EXPECT_EQ(list.command(0u)->link, (0x20u + 32u) / 8u);
    EXPECT_EQ(list.end_command()->link, 0u);
}

TEST(Vdp1CommandListTest, OverflowIsExplicitAndKeepsEndSlot)
{
    Vdp1CommandList list;
    ASSERT_TRUE(list.init(s_buffer, sizeof(s_buffer)));
    for (uint32_t i = 0u; i < list.capacity(); ++i) {
        ASSERT_TRUE(list.append(make_command(VDP1_CMD_POLYGON))) << i;
    }
    EXPECT_EQ(list.count(), list.capacity());
    EXPECT_FALSE(list.append(make_command(VDP1_CMD_POLYGON)));
    EXPECT_EQ(list.diag(), VDP1_CMD_LIST_OVERFLOW);
    EXPECT_TRUE(list.finalize());
    EXPECT_EQ(list.end_command()->ctrl, VDP1_CMD_END);
}

TEST(Vdp1CommandListTest, AppendAfterFinalizeIsRejected)
{
    Vdp1CommandList list;
    ASSERT_TRUE(list.init(s_buffer, sizeof(s_buffer)));
    ASSERT_TRUE(list.finalize());
    EXPECT_FALSE(list.append(make_command(VDP1_CMD_POLYGON)));
    EXPECT_EQ(list.diag(), VDP1_CMD_LIST_ALREADY_FINALIZED);
}

TEST(Vdp1CommandListTest, ResetReusesBuffer)
{
    Vdp1CommandList list;
    ASSERT_TRUE(list.init(s_buffer, sizeof(s_buffer)));
    ASSERT_TRUE(list.append(make_command(VDP1_CMD_POLYGON)));
    ASSERT_TRUE(list.finalize());
    list.reset();
    EXPECT_EQ(list.count(), 0u);
    EXPECT_FALSE(list.finalized());
    EXPECT_EQ(list.total_count(), 0u);
    EXPECT_TRUE(list.append(make_command(VDP1_CMD_DISTORTED_SPRITE)));
}

TEST(Vdp1CommandListTest, NullOrTooSmallBufferIsRejected)
{
    Vdp1CommandList list;
    EXPECT_FALSE(list.init(nullptr, 64u));
    EXPECT_EQ(list.diag(), VDP1_CMD_LIST_NULL_ARGUMENT);
    EXPECT_FALSE(list.init(s_buffer, 32u));
    EXPECT_FALSE(list.inited());
}

TEST(Vdp1CommandListTest, CommandOutOfRangeIsNull)
{
    Vdp1CommandList list;
    ASSERT_TRUE(list.init(s_buffer, sizeof(s_buffer)));
    EXPECT_EQ(list.command(0u), nullptr);
    EXPECT_EQ(list.end_command(), nullptr);
}

}  // namespace
