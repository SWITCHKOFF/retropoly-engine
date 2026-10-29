// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/memory/stack_allocator_test.cpp
//
// Юнит-тесты StackAllocator — docs/plans/sprint_03_04.md, Неделя 7;
// engineering doc v1.43 §5.4 + ALLOCATOR SAFETY CONTRACT (§5).
#include "core/memory/stack_allocator.h"

#include <gtest/gtest.h>

#include <stddef.h>
#include <stdint.h>

namespace
{

alignas(64) uint8_t g_storage[512];
alignas(64) uint8_t g_other_storage[128];

uintptr_t address_of(const void* pointer) { return reinterpret_cast<uintptr_t>(pointer); }

} // namespace

TEST(StackAllocator, PushPopRestoresMarker)
{
    StackAllocator allocator(g_storage, sizeof(g_storage));

    const StackAllocator::Marker frame = allocator.mark();
    void* first = allocator.push(8, 8);
    void* second = allocator.push(16, 8);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    const StackAllocator::Marker after_two = allocator.mark();
    void* third = allocator.push(8, 8);
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(third, g_storage + after_two.offset);

    EXPECT_TRUE(allocator.pop(after_two));
    EXPECT_EQ(allocator.used(), after_two.offset);
    EXPECT_EQ(address_of(third) - address_of(g_storage), after_two.offset);

    EXPECT_TRUE(allocator.pop(frame));
    EXPECT_EQ(allocator.used(), 0u);
}

TEST(StackAllocator, RejectsForeignAndFutureMarkers)
{
    StackAllocator allocator(g_storage, sizeof(g_storage));
    StackAllocator other(g_other_storage, sizeof(g_other_storage));

    ASSERT_NE(allocator.push(8), nullptr);
    const StackAllocator::Marker earlier = allocator.mark();
    ASSERT_NE(allocator.push(8), nullptr);
    const StackAllocator::Marker future = allocator.mark();
    ASSERT_NE(allocator.push(8), nullptr);

    EXPECT_FALSE(allocator.pop(other.mark()));
    EXPECT_FALSE(allocator.pop(StackAllocator::Marker{nullptr, 0}));
    EXPECT_FALSE(allocator.pop(StackAllocator::Marker{&allocator, allocator.capacity() + 1}));

    EXPECT_TRUE(allocator.pop(earlier));
    EXPECT_FALSE(allocator.pop(future));
    EXPECT_TRUE(allocator.pop(earlier));
}

TEST(StackAllocator, ResetFrameRestarts)
{
    StackAllocator allocator(g_storage, sizeof(g_storage));

    ASSERT_NE(allocator.push(64), nullptr);
    ASSERT_GT(allocator.used(), 0u);

    allocator.reset_frame();
    EXPECT_EQ(allocator.used(), 0u);
    EXPECT_EQ(allocator.remaining(), sizeof(g_storage));

    void* again = allocator.push(32, 32);
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again, g_storage);
}

TEST(StackAllocator, UnalignedArenaBase)
{
    StackAllocator allocator(g_storage + 3, sizeof(g_storage) - 3);

    void* eight = allocator.push(5, 8);
    ASSERT_NE(eight, nullptr);
    EXPECT_EQ(address_of(eight) % 8u, 0u);

    void* sixteen = allocator.push(5, 16);
    ASSERT_NE(sixteen, nullptr);
    EXPECT_EQ(address_of(sixteen) % 16u, 0u);
    EXPECT_LE(address_of(eight) + 5u, address_of(sixteen));
}

TEST(StackAllocator, OutOfMemoryAndBadInput)
{
    StackAllocator allocator(g_storage, 16);

    EXPECT_EQ(allocator.push(1, 0), nullptr);
    EXPECT_EQ(allocator.push(1, 3), nullptr);
    EXPECT_EQ(allocator.push(static_cast<size_t>(-1), 8), nullptr);
    EXPECT_EQ(allocator.used(), 0u);

    ASSERT_NE(allocator.push(16), nullptr);
    const size_t used_before = allocator.used();
    EXPECT_EQ(allocator.push(1), nullptr);
    EXPECT_EQ(allocator.used(), used_before);

    StackAllocator empty(nullptr, 128);
    EXPECT_EQ(empty.push(8), nullptr);
    EXPECT_EQ(empty.used(), 0u);
}

TEST(StackAllocator, NestedFrames)
{
    StackAllocator allocator(g_storage, sizeof(g_storage));

    const StackAllocator::Marker outer = allocator.mark();
    ASSERT_NE(allocator.push(16, 8), nullptr);

    const StackAllocator::Marker inner = allocator.mark();
    ASSERT_NE(allocator.push(32, 16), nullptr);
    ASSERT_NE(allocator.push(64, 16), nullptr);

    EXPECT_TRUE(allocator.pop(inner));
    EXPECT_EQ(allocator.used(), inner.offset);
    EXPECT_TRUE(allocator.pop(outer));
    EXPECT_EQ(allocator.used(), 0u);
}
