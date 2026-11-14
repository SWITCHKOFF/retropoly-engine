// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/vdp1_texture_test.cpp
//
// VDP1 texture registry: CI8 upload, CLUT, 256→255 clamp, overflow и
// полный rollback при ошибке (0 persistent-байт).
#include "hal/saturn/vdp1_texture.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

#include <string.h>

namespace {

using namespace saturn;

alignas(32) uint8_t s_pixels[256u * 256u];
uint16_t s_palette[256];

void prepare_inputs()
{
    for (uint32_t i = 0u; i < 256u * 256u; ++i) {
        s_pixels[i] = static_cast<uint8_t>((i & 0xFEu) + 1u);  // 1..255
    }
    for (uint32_t i = 0u; i < 256u; ++i) {
        s_palette[i] =
            static_cast<uint16_t>((i & 0x1Fu) | ((i & 0x1Fu) << 5) | ((i & 0x1Fu) << 10));
    }
}

class Vdp1TextureTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fake_saturn::reset();
        prepare_inputs();
        vdp1_textures().init();
    }

    void TearDown() override { vdp1_textures().shutdown(); }
};

TEST_F(Vdp1TextureTest, CreatesAndUploadsCi8WithClut)
{
    ASSERT_TRUE(vdp1_textures().create(1u, 64u, 64u, s_pixels, s_palette));
    const Vdp1Texture* tex = vdp1_textures().find(1u);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->width, 64u);
    EXPECT_EQ(tex->height, 64u);
    EXPECT_EQ(tex->stride, 64u);
    EXPECT_EQ(tex->clut, 1u);
    EXPECT_EQ(tex->src_addr8, (VDP1_TEXTURE_BASE + tex->vram_offset) / 8u);
    EXPECT_EQ(vdp1_textures().vram_used(), 64u * 64u);
    EXPECT_EQ(vdp1_textures().clut_count(), 1u);
    EXPECT_EQ(fake_saturn::vdp1_upload_calls, 1);
    EXPECT_EQ(fake_saturn::vdp1_palette_calls, 1);
    EXPECT_EQ(fake_saturn::vdp1_vram[VDP1_TEXTURE_BASE + tex->vram_offset], s_pixels[0]);
    EXPECT_EQ(fake_saturn::vdp1_vram[VDP1_TEXTURE_BASE + tex->vram_offset + 64u * 63u + 63u],
              s_pixels[64u * 63u + 63u]);
    EXPECT_EQ(fake_saturn::vdp1_cram[256u], s_palette[0] | 0x8000u);
    EXPECT_EQ(fake_saturn::vdp1_cram[256u + 255u], s_palette[255] | 0x8000u);
}

TEST_F(Vdp1TextureTest, Height256IsClampedTo255WithDiagnostic)
{
    ASSERT_TRUE(vdp1_textures().create(2u, 256u, 256u, s_pixels, s_palette));
    const Vdp1Texture* tex = vdp1_textures().find(2u);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(tex->source_height, 256u);
    EXPECT_EQ(tex->height, 255u);
    EXPECT_EQ(vdp1_textures().vram_used(), 256u * 255u);
    EXPECT_EQ(vdp1_textures().height_clamped_count(), 1u);
}

TEST_F(Vdp1TextureTest, BadArgumentsAreRejectedWithoutStateChange)
{
    EXPECT_FALSE(vdp1_textures().create(0u, 64u, 64u, s_pixels, s_palette));
    EXPECT_EQ(vdp1_textures().diag(), VDP1_TEX_NULL_ARGUMENT);
    EXPECT_FALSE(vdp1_textures().create(1u, 12u, 64u, s_pixels, s_palette));
    EXPECT_EQ(vdp1_textures().diag(), VDP1_TEX_BAD_SIZE);
    EXPECT_FALSE(vdp1_textures().create(1u, 64u, 0u, s_pixels, s_palette));
    EXPECT_EQ(vdp1_textures().diag(), VDP1_TEX_BAD_SIZE);
    EXPECT_FALSE(vdp1_textures().create(1u, 64u, 64u, nullptr, s_palette));
    EXPECT_EQ(vdp1_textures().diag(), VDP1_TEX_NULL_ARGUMENT);
    EXPECT_FALSE(vdp1_textures().create(1u, 64u, 64u, s_pixels, nullptr));
    EXPECT_EQ(vdp1_textures().vram_used(), 0u);
    EXPECT_EQ(vdp1_textures().clut_count(), 0u);
}

TEST_F(Vdp1TextureTest, DuplicateHandleIsRejected)
{
    ASSERT_TRUE(vdp1_textures().create(3u, 64u, 64u, s_pixels, s_palette));
    const uint32_t used = vdp1_textures().vram_used();
    EXPECT_FALSE(vdp1_textures().create(3u, 64u, 64u, s_pixels, s_palette));
    EXPECT_EQ(vdp1_textures().diag(), VDP1_TEX_DUPLICATE_HANDLE);
    EXPECT_EQ(vdp1_textures().vram_used(), used);
}

TEST_F(Vdp1TextureTest, DestroyFreesVramAndClut)
{
    ASSERT_TRUE(vdp1_textures().create(4u, 64u, 64u, s_pixels, s_palette));
    ASSERT_EQ(vdp1_textures().vram_used(), 4096u);
    ASSERT_EQ(vdp1_textures().clut_count(), 1u);
    EXPECT_TRUE(vdp1_textures().destroy(4u));
    EXPECT_EQ(vdp1_textures().vram_used(), 0u);
    EXPECT_EQ(vdp1_textures().clut_count(), 0u);
    EXPECT_EQ(vdp1_textures().find(4u), nullptr);
    EXPECT_FALSE(vdp1_textures().destroy(4u));
    EXPECT_EQ(vdp1_textures().diag(), VDP1_TEX_BAD_HANDLE);
}

TEST_F(Vdp1TextureTest, ClutExhaustionRollsBackVram)
{
    for (uint32_t i = 1u; i <= VDP1_CLUT_COUNT; ++i) {
        ASSERT_TRUE(vdp1_textures().create(i, 64u, 64u, s_pixels, s_palette)) << i;
    }
    EXPECT_EQ(vdp1_textures().clut_count(), VDP1_CLUT_COUNT);
    const uint32_t used = vdp1_textures().vram_used();
    EXPECT_FALSE(vdp1_textures().create(8u, 64u, 64u, s_pixels, s_palette));
    EXPECT_EQ(vdp1_textures().diag(), VDP1_TEX_CLUT_OUT_OF_MEMORY);
    EXPECT_EQ(vdp1_textures().vram_used(), used);
    EXPECT_EQ(vdp1_textures().texture_count(), VDP1_CLUT_COUNT);
}

TEST_F(Vdp1TextureTest, UploadFailureLeavesNoPersistentState)
{
    fake_saturn::vdp1_upload_result = false;
    EXPECT_FALSE(vdp1_textures().create(5u, 64u, 64u, s_pixels, s_palette));
    EXPECT_EQ(vdp1_textures().diag(), VDP1_TEX_UPLOAD_FAILED);
    EXPECT_EQ(vdp1_textures().vram_used(), 0u);
    EXPECT_EQ(vdp1_textures().clut_count(), 0u);
    EXPECT_EQ(vdp1_textures().texture_count(), 0u);
}

TEST_F(Vdp1TextureTest, VramOverflowRollsBackClut)
{
    // 7 × 256×255 = 456 960 > 451 832; шестая помещается, седьмая — нет.
    for (uint32_t i = 1u; i <= 6u; ++i) {
        ASSERT_TRUE(vdp1_textures().create(i, 256u, 256u, s_pixels, s_palette)) << i;
    }
    const uint32_t used = vdp1_textures().vram_used();
    const uint32_t cluts = vdp1_textures().clut_count();
    EXPECT_FALSE(vdp1_textures().create(7u, 256u, 256u, s_pixels, s_palette));
    EXPECT_EQ(vdp1_textures().diag(), VDP1_TEX_VRAM_OUT_OF_MEMORY);
    EXPECT_EQ(vdp1_textures().vram_used(), used);
    EXPECT_EQ(vdp1_textures().clut_count(), cluts);
    EXPECT_EQ(vdp1_textures().find(7u), nullptr);
}

TEST_F(Vdp1TextureTest, FreedClutIsReused)
{
    ASSERT_TRUE(vdp1_textures().create(1u, 64u, 64u, s_pixels, s_palette));
    const uint8_t first = vdp1_textures().find(1u)->clut;
    ASSERT_TRUE(vdp1_textures().destroy(1u));
    ASSERT_TRUE(vdp1_textures().create(2u, 64u, 64u, s_pixels, s_palette));
    EXPECT_EQ(vdp1_textures().find(2u)->clut, first);
}

TEST_F(Vdp1TextureTest, InitClearsRegistry)
{
    ASSERT_TRUE(vdp1_textures().create(1u, 64u, 64u, s_pixels, s_palette));
    vdp1_textures().init();
    EXPECT_EQ(vdp1_textures().texture_count(), 0u);
    EXPECT_EQ(vdp1_textures().vram_used(), 0u);
    EXPECT_EQ(vdp1_textures().clut_count(), 0u);
}

}  // namespace
