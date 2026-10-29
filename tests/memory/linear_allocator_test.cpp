// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/memory/linear_allocator_test.cpp
//
// Юнит-тесты LinearAllocator — docs/plans/sprint_03_04.md, Неделя 5;
// engineering doc v1.43 §5.2 + ALLOCATOR SAFETY CONTRACT (§5).
#include "core/memory/linear_allocator.h"

#include <gtest/gtest.h>

#include <stddef.h>
#include <stdint.h>

namespace
{

alignas(64) uint8_t g_storage[512];

uintptr_t address_of(const void* pointer) { return reinterpret_cast<uintptr_t>(pointer); }

} // namespace

TEST(LinearAllocator, BasicAllocation)
{
    LinearAllocator allocator(g_storage, sizeof(g_storage));

    void* first = allocator.allocate(10);
    void* second = allocator.allocate(20);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    const uintptr_t first_address = address_of(first);
    const uintptr_t second_address = address_of(second);
    EXPECT_LE(first_address + 10u, second_address);
    EXPECT_GE(first_address, address_of(g_storage));
    EXPECT_LT(second_address, address_of(g_storage) + sizeof(g_storage));
    EXPECT_EQ(allocator.used(), static_cast<size_t>(second_address - address_of(g_storage)) + 20u);
    EXPECT_EQ(allocator.remaining(), sizeof(g_storage) - allocator.used());
}

TEST(LinearAllocator, AlignmentUsesReturnedAddress)
{
    LinearAllocator allocator(g_storage, sizeof(g_storage));

    for (size_t alignment = 1; alignment <= 64; alignment <<= 1)
    {
        void* pointer = allocator.allocate(1, alignment);
        ASSERT_NE(pointer, nullptr) << "alignment = " << alignment;
        EXPECT_EQ(address_of(pointer) % alignment, 0u) << "alignment = " << alignment;
        EXPECT_EQ(allocator.used() % 1u, 0u);
    }
}

TEST(LinearAllocator, OutOfMemoryLeavesStateUntouched)
{
    LinearAllocator allocator(g_storage, 16);

    void* full = allocator.allocate(16);
    ASSERT_NE(full, nullptr);
    const size_t used_before = allocator.used();

    EXPECT_EQ(allocator.allocate(1), nullptr);
    EXPECT_EQ(allocator.used(), used_before);
    EXPECT_EQ(allocator.remaining(), 0u);

    LinearAllocator padded(g_storage, 9);
    ASSERT_NE(padded.allocate(1, 1), nullptr);
    const size_t padded_before = padded.used();
    EXPECT_EQ(padded.allocate(8, 8), nullptr);
    EXPECT_EQ(padded.used(), padded_before);
}

TEST(LinearAllocator, RejectsBadAlignmentAndHugeSize)
{
    LinearAllocator allocator(g_storage, sizeof(g_storage));

    EXPECT_EQ(allocator.allocate(1, 0), nullptr);
    EXPECT_EQ(allocator.allocate(1, 3), nullptr);
    EXPECT_EQ(allocator.allocate(static_cast<size_t>(-1), 8), nullptr);
    EXPECT_EQ(allocator.used(), 0u);
}

TEST(LinearAllocator, NullBufferRejected)
{
    LinearAllocator allocator(nullptr, 128);
    EXPECT_EQ(allocator.allocate(8), nullptr);
    EXPECT_EQ(allocator.used(), 0u);
    EXPECT_EQ(allocator.remaining(), 0u);
}

TEST(LinearAllocator, ResetRestartsAllocation)
{
    LinearAllocator allocator(g_storage, sizeof(g_storage));

    ASSERT_NE(allocator.allocate(64), nullptr);
    ASSERT_GT(allocator.used(), 0u);

    allocator.reset();
    EXPECT_EQ(allocator.used(), 0u);
    EXPECT_EQ(allocator.remaining(), sizeof(g_storage));

    void* again = allocator.allocate(32, 32);
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again, g_storage);
}

TEST(LinearAllocator, RollbackRestoresMarker)
{
    LinearAllocator allocator(g_storage, sizeof(g_storage));

    ASSERT_NE(allocator.allocate(8), nullptr);
    const LinearAllocator::Marker first = allocator.mark();

    ASSERT_NE(allocator.allocate(16), nullptr);
    const LinearAllocator::Marker second = allocator.mark();
    ASSERT_NE(allocator.allocate(32), nullptr);

    EXPECT_TRUE(allocator.rollback(second));
    EXPECT_EQ(allocator.used(), second.offset);

    EXPECT_TRUE(allocator.rollback(first));
    EXPECT_EQ(allocator.used(), first.offset);

    EXPECT_NE(allocator.allocate(8), nullptr);
}

TEST(LinearAllocator, RejectsForeignAndFutureMarkers)
{
    LinearAllocator allocator(g_storage, sizeof(g_storage));
    LinearAllocator other(g_storage, sizeof(g_storage));

    ASSERT_NE(allocator.allocate(8), nullptr);
    const LinearAllocator::Marker earlier = allocator.mark();
    ASSERT_NE(allocator.allocate(8), nullptr);
    const LinearAllocator::Marker future = allocator.mark();
    ASSERT_NE(allocator.allocate(8), nullptr);

    EXPECT_FALSE(allocator.rollback(other.mark()));
    EXPECT_FALSE(allocator.rollback(LinearAllocator::Marker{nullptr, 0}));
    EXPECT_FALSE(allocator.rollback(LinearAllocator::Marker{&allocator, allocator.capacity() + 1}));

    EXPECT_TRUE(allocator.rollback(earlier));
    EXPECT_FALSE(allocator.rollback(future));
    EXPECT_TRUE(allocator.rollback(earlier));
}

TEST(LinearAllocator, UnalignedArenaBase)
{
    LinearAllocator allocator(g_storage + 1, sizeof(g_storage) - 1);

    void* eight = allocator.allocate(5, 8);
    ASSERT_NE(eight, nullptr);
    EXPECT_EQ(address_of(eight) % 8u, 0u);

    void* sixteen = allocator.allocate(5, 16);
    ASSERT_NE(sixteen, nullptr);
    EXPECT_EQ(address_of(sixteen) % 16u, 0u);
    EXPECT_LE(address_of(eight) + 5u, address_of(sixteen));
}
