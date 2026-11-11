// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/vdp1_vram_test.cpp
//
// VDP1 VRAM map + first-fit texture allocator (S27–28).
#include "hal/saturn/vdp1_vram.h"

#include <gtest/gtest.h>

namespace {

using namespace saturn;

TEST(Vdp1VramLayout, RegionsCoverVramWithoutOverlap)
{
    EXPECT_EQ(VDP1_VRAM_SIZE, 512u * 1024u);
    EXPECT_EQ(VDP1_JUMP_TABLE_OFFSET, 0u);
    EXPECT_EQ(VDP1_COMMAND_TABLE_A_OFFSET, 0x20u);
    EXPECT_EQ(VDP1_COMMAND_TABLE_B_OFFSET, 0x8000u);
    EXPECT_EQ(VDP1_TEXTURE_BASE, 0x10000u);
    EXPECT_EQ(VDP1_TEXTURE_END, 0x7E4F8u);
    EXPECT_EQ(VDP1_GOURAUD_BASE, 0x7E4F8u);
    EXPECT_EQ(VDP1_GOURAUD_END, 0x7E6F8u);
    EXPECT_LE(VDP1_GOURAUD_END, VDP1_JO3D_CLIP_BASE);
    EXPECT_EQ(VDP1_LIGHT_END, VDP1_VRAM_SIZE);
    EXPECT_EQ(VDP1_TEXTURE_BYTES, 451832u);
    EXPECT_EQ(VDP1_TEXTURE_USABLE_BYTES, 451808u);
}

TEST(Vdp1TextureAllocatorTest, InitExposesWholeRegion)
{
    Vdp1TextureAllocator alloc;
    alloc.init();
    EXPECT_TRUE(alloc.inited());
    EXPECT_EQ(alloc.capacity(), VDP1_TEXTURE_USABLE_BYTES);
    EXPECT_EQ(alloc.used(), 0u);
    EXPECT_EQ(alloc.available(), VDP1_TEXTURE_USABLE_BYTES);
    EXPECT_EQ(alloc.diag(), VDP1_VRAM_OK);
}

TEST(Vdp1TextureAllocatorTest, AllocateIs32ByteAlignedAndFirstFit)
{
    Vdp1TextureAllocator alloc;
    alloc.init();
    const uint32_t a = alloc.allocate(100u);
    EXPECT_EQ(a, 0u);
    const uint32_t b = alloc.allocate(64u);
    EXPECT_EQ(b, 128u);  // 100 → 128
    EXPECT_EQ(alloc.used(), 192u);
    EXPECT_EQ(alloc.available(), VDP1_TEXTURE_USABLE_BYTES - 192u);
}

TEST(Vdp1TextureAllocatorTest, FreeCoalescesNeighbours)
{
    Vdp1TextureAllocator alloc;
    alloc.init();
    const uint32_t a = alloc.allocate(128u);
    const uint32_t b = alloc.allocate(256u);
    const uint32_t c = alloc.allocate(64u);
    ASSERT_EQ(a, 0u);
    ASSERT_EQ(b, 128u);
    ASSERT_EQ(c, 384u);

    EXPECT_TRUE(alloc.free(b));
    EXPECT_TRUE(alloc.free(a));
    EXPECT_EQ(alloc.used(), 64u);  // c
    // После coalescing свободный блок снова с начала: новый alloc получает 0.
    EXPECT_EQ(alloc.allocate(300u), 0u);
    EXPECT_TRUE(alloc.free(c));
}

TEST(Vdp1TextureAllocatorTest, OverflowDoesNotChangeState)
{
    Vdp1TextureAllocator alloc;
    alloc.init();
    // 451 808 = 14 119 × 32; остаётся 24 байта, которых мало даже на
    // минимальный выровненный блок.
    EXPECT_EQ(alloc.allocate(VDP1_TEXTURE_BYTES - 24u), 0u);
    const uint32_t used = alloc.used();
    EXPECT_EQ(alloc.allocate(32u), VDP1_TEXTURE_INVALID_OFFSET);
    EXPECT_EQ(alloc.diag(), VDP1_VRAM_OUT_OF_MEMORY);
    EXPECT_EQ(alloc.used(), used);
}

TEST(Vdp1TextureAllocatorTest, ForeignAndDoubleFree)
{
    Vdp1TextureAllocator alloc;
    alloc.init();
    const uint32_t a = alloc.allocate(64u);
    EXPECT_FALSE(alloc.free(a + 32u));
    EXPECT_EQ(alloc.diag(), VDP1_VRAM_FOREIGN_OFFSET);
    EXPECT_TRUE(alloc.free(a));
    EXPECT_FALSE(alloc.free(a));
    EXPECT_EQ(alloc.diag(), VDP1_VRAM_DOUBLE_FREE);
}

TEST(Vdp1TextureAllocatorTest, ZeroSizeRejected)
{
    Vdp1TextureAllocator alloc;
    alloc.init();
    EXPECT_EQ(alloc.allocate(0u), VDP1_TEXTURE_INVALID_OFFSET);
    EXPECT_EQ(alloc.diag(), VDP1_VRAM_BAD_ARGUMENT);
}

TEST(Vdp1TextureAllocatorTest, AllocationIsDeterministic)
{
    Vdp1TextureAllocator a;
    Vdp1TextureAllocator b;
    a.init();
    b.init();
    for (uint32_t i = 0u; i < 8u; ++i) {
        EXPECT_EQ(a.allocate(64u + i * 32u), b.allocate(64u + i * 32u));
    }
    EXPECT_EQ(a.used(), b.used());
}

}  // namespace
