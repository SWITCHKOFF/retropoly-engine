// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/render/contract_test.cpp
//
// Контракты рендер-ядра (Спринт 7–8):
//   - depth_key_from_camera_z (§1.2.6, ЗАКОНОДАТЕЛЬНО 1:1): z<0 →
//     DEPTH_REJECT; z вне [depth_min..depth_max] → DEPTH_REJECT (без
//     modulo-aliasing); 1024 в [0..4095] → 1024; 4095 на верхней
//     границе — валидный bucket; 4096 → REJECT;
//   - SORT-порядок Painter's (§7.2): far→near по depth_key, tie-breaker
//     submission_index ASC (единая точка правды — src/render/sort_key.h);
//   - DEPTH_REJECT — нормативный sentinel (UINT32_MAX), не 0-подобный.
#include <gtest/gtest.h>

#include "render/renderer.h"
#include "render/sort_key.h"

#include <algorithm>

// ---------------------------------------------------------------------------
// depth_key_from_camera_z (§1.2.6 — normative тест-векторы 1:1)
// ---------------------------------------------------------------------------

TEST(DepthKeyNormative, SentinelIsMaxU32)
{
    EXPECT_EQ(DEPTH_REJECT, UINT32_C(0xFFFFFFFF));
}

TEST(DepthKeyNormative, NegativeZRejects)
{
    // Normative: z = −1 → DEPTH_REJECT (без modulo-aliasing).
    EXPECT_EQ(depth_key_from_camera_z(-1, 0u, 4095u), DEPTH_REJECT);
    EXPECT_EQ(depth_key_from_camera_z(INT32_MIN, 0u, 4095u), DEPTH_REJECT);
}

TEST(DepthKeyNormative, OutOfRangeRejects)
{
    // Normative: z = 4096 вне [0..4095] → DEPTH_REJECT.
    EXPECT_EQ(depth_key_from_camera_z(4096, 0u, 4095u), DEPTH_REJECT);
    EXPECT_EQ(depth_key_from_camera_z(7777, 0u, 4095u), DEPTH_REJECT);
    EXPECT_EQ(depth_key_from_camera_z(0, 10u, 20u), DEPTH_REJECT); // < min.
    EXPECT_EQ(depth_key_from_camera_z(21, 10u, 20u), DEPTH_REJECT); // > max.
    EXPECT_EQ(depth_key_from_camera_z(9, 10u, 20u), DEPTH_REJECT);
}

TEST(DepthKeyNormative, InRangeMapsToBucket)
{
    // Normative: 1024 в [0..4095] — валидный bucket (значение = z − min).
    EXPECT_EQ(depth_key_from_camera_z(1024, 0u, 4095u), 1024u);
    EXPECT_EQ(depth_key_from_camera_z(1024, 1024u, 4095u), 0u); // Burn = min.
    EXPECT_EQ(depth_key_from_camera_z(0, 0u, 4095u), 0u);       // Near-plane.
    EXPECT_EQ(depth_key_from_camera_z(4095, 0u, 4095u), 4095u); // Far-plane
    // Внешние границы диапазона: min/max — валидные buckets.
    EXPECT_EQ(depth_key_from_camera_z(20, 10u, 20u), 10u);
    EXPECT_EQ(depth_key_from_camera_z(10, 10u, 20u), 0u);
}

TEST(DepthKeyNormative, MonotonicAcrossRange)
{
    // Монотонность: большие z давали НЕ меньшие ключи ( Painter's base).
    uint32_t prev = 0u;
    for (int32_t z = 0; z <= 4095; z += 64) {
        const uint32_t key = depth_key_from_camera_z(z, 0u, 4095u);
        ASSERT_NE(key, DEPTH_REJECT);
        EXPECT_GE(key, prev);
        prev = key;
    }
}

// ---------------------------------------------------------------------------
// SORT-порядок Painter's (§7.2 — единая точка правды sort_key.h)
// ---------------------------------------------------------------------------

TEST(SortKeyContract, FarFirstBackToFront)
{
    const SortRef far_ref{3000u, 5u, 0u};   // Дальний.
    const SortRef near_ref{10u, 0u, 1u};    // Ближний (вставлен раньше!).
    // Порядок подачи НЕ по вставке, а по удалённости: far → first.
    EXPECT_TRUE(sort_ref_before(far_ref, near_ref));
    EXPECT_FALSE(sort_ref_before(near_ref, far_ref));
}

TEST(SortKeyContract, TieBreakerIsSubmissionAsc)
{
    const SortRef a{100u, 2u, 0u};
    const SortRef b{100u, 7u, 1u};
    // Равный ключ: раньше подаётся МЕНЬШИЙ submission_index.
    EXPECT_TRUE(sort_ref_before(a, b));
    EXPECT_FALSE(sort_ref_before(b, a));
}

TEST(SortKeyContract, StrictWeakOrderingProperties)
{
    // Свойства строгого слабого порядка (для std::sort — UB-безопасность):
    // несравнимость (a,b), транзитивность несравнимости.
    const SortRef same1{500u, 4u, 0u};
    const SortRef same2{500u, 4u, 1u};     // Эквивалентны (оба поля равны).
    const SortRef other{500u, 6u, 2u};
    EXPECT_FALSE(sort_ref_before(same1, same2));
    EXPECT_FALSE(sort_ref_before(same2, same1)); // Несравнимы/эквивалентны.
    EXPECT_TRUE(sort_ref_before(same1, other));
    EXPECT_FALSE(sort_ref_before(other, same1));
    // Транзитивность эквивалентности: same2 ~ same1, same1 < other ⇒
    // same2 < other.
    EXPECT_TRUE(sort_ref_before(same2, other));
}

TEST(SortKeyContract, SortedSequenceIsBackToFront)
{
    // Демонстрация итогового порядка: сортировка входа с перемешанными
    // ключами даёт far→near с устойчивым tie-breaker (детерминизм).
    SortRef seq[6] = {
        {100u, 0u, 0u}, // near, вставлен раньше
        {400u, 1u, 1u}, // far
        {100u, 2u, 2u}, // near, вставлен позже (эквивалентен [0])
        {250u, 3u, 3u}, // середина
        {400u, 4u, 4u}, // far, вставлен позже [1]
    };
    std::sort(seq, seq + 5, sort_ref_before);

    EXPECT_EQ(seq[0].depth_key, 400u);
    EXPECT_EQ(seq[0].submission_index, 1u); // Far: раньше вставленный.
    EXPECT_EQ(seq[1].depth_key, 400u);
    EXPECT_EQ(seq[1].submission_index, 4u);
    EXPECT_EQ(seq[2].depth_key, 250u);
    EXPECT_EQ(seq[3].depth_key, 100u);
    EXPECT_EQ(seq[3].submission_index, 0u); // Near: порядок вставки сохранён.
    EXPECT_EQ(seq[4].depth_key, 100u);
    EXPECT_EQ(seq[4].submission_index, 2u);
}
