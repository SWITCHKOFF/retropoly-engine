// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/ecs/entity_test.cpp
//
// Юнит-тесты канонической кодировки хэндла (docs/plans/sprint_03_04.md,
// Неделя 8; engineering doc v1.43 §1.2.4, §6.1).
#include "core/ecs/entity.h"

#include <gtest/gtest.h>

#include <stdint.h>

TEST(Entity, CanonicalConstants)
{
    EXPECT_EQ(ENTITY_INDEX_BITS, 24u);
    EXPECT_EQ(ENTITY_GENERATION_BITS, 8u);
    EXPECT_EQ(ENTITY_INDEX_MASK, 0x00FFFFFFu);
    EXPECT_EQ(ENTITY_GENERATION_SHIFT, 24u);
    EXPECT_EQ(INVALID_ENTITY, 0u);
    EXPECT_EQ(MAX_ENTITIES, 10000u);
}

TEST(Entity, MakeIndexGenerationRoundTrip)
{
    const uint32_t index = 0x00123456u;
    const uint32_t generation = 0xABu;
    const Entity handle = entity_make(index, generation);

    EXPECT_EQ(entity_index(handle), index);
    EXPECT_EQ(entity_generation(handle), generation);
    EXPECT_EQ(handle, (generation << 24) | index);
}

TEST(Entity, InvalidHandleIsZero)
{
    EXPECT_EQ(entity_make(0, 0), INVALID_ENTITY);
    EXPECT_EQ(entity_index(INVALID_ENTITY), 0u);
    EXPECT_EQ(entity_generation(INVALID_ENTITY), 0u);
}

TEST(Entity, MasksOutOfRangeFields)
{
    const Entity handle = entity_make(0x00FFFFFFu, 0xFFu);
    EXPECT_EQ(entity_index(handle), 0x00FFFFFFu);
    EXPECT_EQ(entity_generation(handle), 0xFFu);

    const Entity truncated = entity_make(0x01000001u, 0x1FFu);
    EXPECT_EQ(entity_index(truncated), 1u);
    EXPECT_EQ(entity_generation(truncated), 0xFFu);
}
