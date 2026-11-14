// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/vdp2_backend_test.cpp
//
// S29–30 VDP2 NBG1 bitmap: init/palette/upload тайлами, VBlank scroll,
// диагностика отказов. Host-тесты без SH-2/Jo Engine.
#include "hal/saturn/vdp2_backend.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

#include <string.h>

namespace {

using namespace saturn;

alignas(8) uint8_t s_pixels[256u * 256u];
uint16_t s_palette[256];

void prepare_texture()
{
    for (uint32_t y = 0u; y < 256u; ++y) {
        for (uint32_t x = 0u; x < 256u; ++x) {
            s_pixels[y * 256u + x] = static_cast<uint8_t>(((x + y) & 0xFEu) + 1u);
        }
    }
    for (uint32_t i = 0u; i < 256u; ++i) {
        s_palette[i] = static_cast<uint16_t>(i & 0x7FFFu);
    }
}

TextureAsset make_texture(uint32_t width = 256u, uint32_t height = 256u)
{
    TextureAsset tex;
    tex.pixels = s_pixels;
    tex.palette = s_palette;
    tex.width = width;
    tex.height = height;
    return tex;
}

class Vdp2BackendTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fake_saturn::reset();
        prepare_texture();
        vdp2_backend_shutdown();
        vdp2_backend_reset_stats();
    }

    void TearDown() override { vdp2_backend_shutdown(); }
};

TEST_F(Vdp2BackendTest, InitUsesReservedPaletteAndIsIdempotent)
{
    ASSERT_TRUE(vdp2_backend_init());
    EXPECT_TRUE(vdp2_backend_available());
    EXPECT_EQ(fake_saturn::vdp2_init_calls, 1);
    EXPECT_TRUE(vdp2_backend_init());
    EXPECT_EQ(fake_saturn::vdp2_init_calls, 1);
    vdp2_backend_shutdown();
    EXPECT_FALSE(vdp2_backend_available());
    EXPECT_EQ(fake_saturn::vdp2_shutdown_calls, 1);
    vdp2_backend_shutdown();
    EXPECT_EQ(fake_saturn::vdp2_shutdown_calls, 1);
}

TEST_F(Vdp2BackendTest, InitFailureIsExplicit)
{
    fake_saturn::vdp2_init_result = false;
    EXPECT_FALSE(vdp2_backend_init());
    EXPECT_FALSE(vdp2_backend_available());
    EXPECT_EQ(fake_saturn::vdp2_init_calls, 1);
}

TEST_F(Vdp2BackendTest, UploadTilesTextureAcrossBitmap)
{
    ASSERT_TRUE(vdp2_backend_init());
    const TextureAsset tex = make_texture();
    ASSERT_TRUE(vdp2_backend_upload_background(&tex));

    EXPECT_EQ(fake_saturn::vdp2_palette_calls, 1);
    EXPECT_EQ(fake_saturn::vdp2_cram[VDP2_BACKGROUND_PALETTE * 256u],
              static_cast<uint16_t>(s_palette[0] | 0x8000u));
    EXPECT_EQ(fake_saturn::vdp2_cram[VDP2_BACKGROUND_PALETTE * 256u + 255u],
              static_cast<uint16_t>(s_palette[255] | 0x8000u));

    // 4 горизонтальных тайла 256×256 на bitmap 1024×256.
    EXPECT_EQ(fake_saturn::vdp2_bitmap[0], s_pixels[0]);
    EXPECT_EQ(fake_saturn::vdp2_bitmap[256u], s_pixels[0]);
    EXPECT_EQ(fake_saturn::vdp2_bitmap[768u], s_pixels[0]);
    EXPECT_EQ(fake_saturn::vdp2_bitmap[1024u * 255u + 1023u],
              s_pixels[255u * 256u + 255u]);
    EXPECT_EQ(vdp2_backend_stats().uploads, 1u);
    EXPECT_EQ(vdp2_backend_stats().upload_failures, 0u);
}

TEST_F(Vdp2BackendTest, InvalidTextureIsRejectedWithoutPaletteWrite)
{
    ASSERT_TRUE(vdp2_backend_init());
    EXPECT_FALSE(vdp2_backend_upload_background(nullptr));
    TextureAsset tex = make_texture(12u, 256u);
    EXPECT_FALSE(vdp2_backend_upload_background(&tex));
    tex = make_texture(256u, 0u);
    EXPECT_FALSE(vdp2_backend_upload_background(&tex));
    tex = make_texture(2048u, 256u);
    EXPECT_FALSE(vdp2_backend_upload_background(&tex));
    EXPECT_EQ(fake_saturn::vdp2_palette_calls, 0);
    EXPECT_EQ(vdp2_backend_stats().upload_failures, 4u);
}

TEST_F(Vdp2BackendTest, ScrollIsCommittedOnlyOnVblank)
{
    ASSERT_TRUE(vdp2_backend_init());
    vdp2_backend_set_scroll(fixed_from_int(3) + FIXED_ONE / 2, -fixed_from_int(2) - FIXED_ONE / 5);
    EXPECT_EQ(fake_saturn::vdp2_scroll_calls, 0);
    vdp2_backend_commit_vblank();
    EXPECT_EQ(fake_saturn::vdp2_scroll_calls, 1);
    EXPECT_EQ(fake_saturn::vdp2_scroll_x, 4);
    EXPECT_EQ(fake_saturn::vdp2_scroll_y, -2);
    EXPECT_EQ(vdp2_backend_stats().scroll_commits, 1u);
    EXPECT_EQ(vdp2_backend_stats().last_scroll_x, 4);
    EXPECT_EQ(vdp2_backend_stats().last_scroll_y, -2);
}

TEST_F(Vdp2BackendTest, CommitBeforeInitIsNoOp)
{
    vdp2_backend_set_scroll(fixed_from_int(10), fixed_from_int(20));
    vdp2_backend_commit_vblank();
    EXPECT_EQ(fake_saturn::vdp2_scroll_calls, 0);
}

}  // namespace
