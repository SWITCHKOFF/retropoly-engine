// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/render/texture_test.cpp
//
// Юнит-тесты текстурного слоя ядра (Спринт 7–8, неделя 15):
//   - НОРМАТИВНЫЙ bit-layout RGB555+STP (§1.2.5): тест-векторы
//     pack_rgb555_stp(31,0,0,true)==0x801F, (0,31,0,true)==0x83E0,
//     (0,0,31,true)==0xFC00 — бит-порядок и STP-бит;
//   - честны unpack 5→8 (PS1-способ: (c<<3)|(c>>2), 31→255, 0→0);
//   - пул CI8-текстур: границы (0/256), отказы (NULL, переполнение
//     слотов/арены), release/повторный acquire, поля по хэндлу.
// Тексты не требует GL — чисто CPU-контракты ядра.
#include <gtest/gtest.h>

#include "render/texture.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Нормативный pack/unpack (§1.2.5 — тест-векторы 1:1)
// ---------------------------------------------------------------------------

TEST(TextureRgb555, PackNormativeVectors)
{
    // R[4:0] | G[9:5] | B[14:10] | STP[15].
    EXPECT_EQ(pack_rgb555_stp(31, 0, 0, true), UINT16_C(0x801F));
    EXPECT_EQ(pack_rgb555_stp(0, 31, 0, true), UINT16_C(0x83E0));
    EXPECT_EQ(pack_rgb555_stp(0, 0, 31, true), UINT16_C(0xFC00));
}

TEST(TextureRgb555, PackStpBitTogglesBit15Only)
{
    const uint16_t no_stp = pack_rgb555_stp(1, 2, 3, false);
    const uint16_t with_stp = pack_rgb555_stp(1, 2, 3, true);
    EXPECT_EQ(with_stp, (uint16_t)(no_stp | 0x8000u));
    EXPECT_EQ(no_stp & 0x8000u, 0u);
}

TEST(TextureRgb555, PackMasksOverrangeInputs)
{
    // Компоненты маскируются к 5 битам (безопасный контракт).
    EXPECT_EQ(pack_rgb555_stp(32, 33, 34, false),
              pack_rgb555_stp(0, 1, 2, false));
    EXPECT_EQ(pack_rgb555_stp(255, 255, 255, false),
              pack_rgb555_stp(31, 31, 31, false));
}

TEST(TextureRgb555, UnpackChannel5To8Ps1Expansion)
{
    // PS1-расширение: (c<<3)|(c>>2) — 31→255, 0→0, 16→(128|4)=132.
    EXPECT_EQ(unpack_channel5_to_8(31), 255);
    EXPECT_EQ(unpack_channel5_to_8(0), 0);
    EXPECT_EQ(unpack_channel5_to_8(16), 132);
    EXPECT_EQ(unpack_channel5_to_8(1), 8); // (1<<3)|(0) — ровно.
}

TEST(TextureRgb555, UnpackFullWordRoundtrip)
{
    uint8_t r = 0, g = 0, b = 0;
    unpack_rgb555_stp(pack_rgb555_stp(31, 0, 0, true), &r, &g, &b);
    EXPECT_EQ(r, 255);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(b, 0);

    unpack_rgb555_stp(pack_rgb555_stp(0, 31, 0, true), &r, &g, &b);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(g, 255);
    EXPECT_EQ(b, 0);

    unpack_rgb555_stp(pack_rgb555_stp(0, 0, 31, true), &r, &g, &b);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(b, 255);
}

// ---------------------------------------------------------------------------
// Пул CI8-текстур (границы, отказы, повторное использование)
// ---------------------------------------------------------------------------

namespace {

/** Фиксирует рабочие пиксели/палитру теста (детерминированные патерны). */
struct TestFixtureData
{
    // 256×256 = полный max-dim блок: render_texture_acquire читает ровно
    // w*h байт (контракт), а кейсы 256×256 ниже — иначе было бы
    // чтение за границу меньшего буфера (heap-overread, гейт pc-asan).
    uint8_t pixels[256 * 256];
    uint16_t palette[256];
};

/** Заполняет fixture (rising pattern — проверяется копия, не эталон). */
void fill_fixture(TestFixtureData* fixture)
{
    for (uint32_t i = 0u; i < 256u * 256u; ++i) {
        fixture->pixels[i] = (uint8_t)(i & 0xFFu);
    }
    for (uint32_t i = 0u; i < 256u; ++i) {
        fixture->palette[i] = pack_rgb555_stp((uint8_t)i, (uint8_t)(i >> 1),
                                              (uint8_t)(255u - i), true);
    }
}

} // namespace

class TexturePoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        render_texture_pool_reset(); // Детерминированное старт-состояние.
        fill_fixture(&fixture_);
    }

    TestFixtureData fixture_;
};

TEST_F(TexturePoolTest, AcquireStoresDimensionsAndCopiesData)
{
    const TextureHandle h =
        render_texture_acquire(64, 64, fixture_.pixels, fixture_.palette);
    ASSERT_NE(h, RENDER_TEXTURE_INVALID);
    EXPECT_EQ(render_texture_width(h), 64u);
    EXPECT_EQ(render_texture_height(h), 64u);

    const uint8_t* pixels = render_texture_pixels(h);
    ASSERT_NE(pixels, nullptr);
    EXPECT_EQ(memcmp(pixels, fixture_.pixels, 64u * 64u), 0);

    const uint16_t* palette = render_texture_palette(h);
    ASSERT_NE(palette, nullptr);
    EXPECT_EQ(memcmp(palette, fixture_.palette, 256u * sizeof(uint16_t)), 0);
}

TEST_F(TexturePoolTest, AcquireRejectsMalformedInput)
{
    // NULL-входы (Failure policy: malformed reject before dereference).
    EXPECT_EQ(render_texture_acquire(64, 64, NULL, fixture_.palette),
              RENDER_TEXTURE_INVALID);
    EXPECT_EQ(render_texture_acquire(64, 64, fixture_.pixels, NULL),
              RENDER_TEXTURE_INVALID);
    // Границы размера: 0 и > RENDER_TEXTURE_MAX_DIM.
    EXPECT_EQ(render_texture_acquire(0, 64, fixture_.pixels, fixture_.palette),
              RENDER_TEXTURE_INVALID);
    EXPECT_EQ(render_texture_acquire(64, 0, fixture_.pixels, fixture_.palette),
              RENDER_TEXTURE_INVALID);
    EXPECT_EQ(
        render_texture_acquire(257, 64, fixture_.pixels, fixture_.palette),
        RENDER_TEXTURE_INVALID);
    EXPECT_EQ(
        render_texture_acquire(64, 257, fixture_.pixels, fixture_.palette),
        RENDER_TEXTURE_INVALID);
    // 256×256 — на границе, ДОПУСТИМО (без содержимого в арена влезет
    // ровно одна: 65536 байт из 131072).
    EXPECT_EQ(render_texture_acquire(256, 256, fixture_.pixels,
                                     fixture_.palette),
              1u); // Первый хэндл = slot 0 + 1.
}

TEST_F(TexturePoolTest, InvalidHandlesAreSafeNoOps)
{
    // Невалидные хэндлы: безопасный no-op (platform.h контракт).
    render_texture_release(0u);
    render_texture_release(RENDER_TEXTURE_MAX + 1u);
    EXPECT_EQ(render_texture_width(0u), 0u);
    EXPECT_EQ(render_texture_height(0u), 0u);
    EXPECT_EQ(render_texture_pixels(0u), nullptr);
    EXPECT_EQ(render_texture_palette(0u), nullptr);
    EXPECT_EQ(render_texture_width(RENDER_TEXTURE_MAX + 1u), 0u);
    EXPECT_EQ(render_texture_pixels(RENDER_TEXTURE_MAX + 1u), nullptr);
}

TEST_F(TexturePoolTest, SlotsExhaustToLimitAndRefuse)
{
    TextureHandle handles[RENDER_TEXTURE_MAX];
    for (uint32_t i = 0u; i < RENDER_TEXTURE_MAX; ++i) {
        handles[i] = render_texture_acquire(8, 8, fixture_.pixels,
                                            fixture_.palette);
        ASSERT_NE(handles[i], RENDER_TEXTURE_INVALID);
        EXPECT_EQ(handles[i], i + 1u); // Хэндл = index + 1 (контракт).
    }
    // 17-я текстура — явный отказ (пул полен), расширение запрещено.
    EXPECT_EQ(render_texture_acquire(8, 8, fixture_.pixels, fixture_.palette),
              RENDER_TEXTURE_INVALID);
}

TEST_F(TexturePoolTest, ArenaExhaustRefuses)
{
    // Арена 128 KiB: одна 256×256 (64 KiB) + вторая — ровно 128 KiB.
    EXPECT_NE(render_texture_acquire(256, 256, fixture_.pixels,
                                     fixture_.palette),
              RENDER_TEXTURE_INVALID);
    EXPECT_NE(render_texture_acquire(256, 256, fixture_.pixels,
                                     fixture_.palette),
              RENDER_TEXTURE_INVALID);
    // Третья — явный отказ: арена исчерпана, никакого silent drop.
    EXPECT_EQ(render_texture_acquire(256, 256, fixture_.pixels,
                                     fixture_.palette),
              RENDER_TEXTURE_INVALID);
    // Частичное место (1 байт) не создаёт текстуру 2×256 (512 байт).
    EXPECT_EQ(render_texture_acquire(2, 256, fixture_.pixels, fixture_.palette),
              RENDER_TEXTURE_INVALID);
}

TEST_F(TexturePoolTest, ReleaseFreesSlotForReuse)
{
    const TextureHandle first =
        render_texture_acquire(16, 16, fixture_.pixels, fixture_.palette);
    ASSERT_NE(first, RENDER_TEXTURE_INVALID);

    render_texture_release(first);
    // Доступ после release — NULL/0 (безопасно).
    EXPECT_EQ(render_texture_pixels(first), nullptr);
    EXPECT_EQ(render_texture_width(first), 0u);

    // Слот переиспользуется (same handle value — first-fit).
    const TextureHandle second =
        render_texture_acquire(16, 16, fixture_.pixels, fixture_.palette);
    ASSERT_NE(second, RENDER_TEXTURE_INVALID);
    EXPECT_EQ(second, first);
}

TEST_F(TexturePoolTest, DoubleReleaseIsSafeNoOp)
{
    const TextureHandle h =
        render_texture_acquire(16, 16, fixture_.pixels, fixture_.palette);
    ASSERT_NE(h, RENDER_TEXTURE_INVALID);
    render_texture_release(h);
    render_texture_release(h); // Повторное освобождение — no-op.
}

TEST_F(TexturePoolTest, PoolResetClearsEverything)
{
    ASSERT_NE(render_texture_acquire(16, 16, fixture_.pixels, fixture_.palette),
              RENDER_TEXTURE_INVALID);
    render_texture_pool_reset();

    // После reset поля по валидному ранее хэндлу пусты (договора contract).
    EXPECT_EQ(render_texture_pixels(1u), nullptr);
    EXPECT_EQ(render_texture_width(1u), 0u);
}
