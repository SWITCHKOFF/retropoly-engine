// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/memory/pool_allocator_test.cpp
//
// Юнит-тесты Pool<T, N> — docs/plans/sprint_03_04.md, Неделя 6;
// engineering doc v1.43 §5.3 + ALLOCATOR SAFETY CONTRACT (§5).
#include "core/memory/pool_allocator.h"

#include <gtest/gtest.h>

#include <stdint.h>

namespace
{

struct PoolItem
{
    uint32_t id;
    uint32_t payload;
};

} // namespace

TEST(PoolAllocator, BasicAndReuse)
{
    Pool<PoolItem, 8> pool;
    EXPECT_EQ(pool.used(), 0u);

    PoolItem* first = pool.allocate();
    PoolItem* second = pool.allocate();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
    first->id = 1;
    second->id = 2;
    EXPECT_EQ(pool.used(), 2u);

    EXPECT_TRUE(pool.deallocate(first));
    PoolItem* reused = pool.allocate();
    EXPECT_EQ(reused, first);
    EXPECT_EQ(pool.used(), 2u);

    EXPECT_TRUE(pool.deallocate(second));
    EXPECT_TRUE(pool.deallocate(reused));
    EXPECT_EQ(pool.used(), 0u);
}

TEST(PoolAllocator, OutOfMemory)
{
    Pool<PoolItem, 4> pool;
    PoolItem* slots[4] = {};

    for (size_t i = 0; i < 4; ++i)
    {
        slots[i] = pool.allocate();
        ASSERT_NE(slots[i], nullptr);
        slots[i]->id = static_cast<uint32_t>(i);
    }

    EXPECT_EQ(pool.allocate(), nullptr);
    EXPECT_EQ(pool.used(), 4u);

    for (size_t i = 0; i < 4; ++i)
        EXPECT_TRUE(pool.deallocate(slots[i]));
    EXPECT_EQ(pool.used(), 0u);
}

TEST(PoolAllocator, IteratesOnlyLiveObjects)
{
    Pool<PoolItem, 8> pool;
    PoolItem* slots[4] = {};
    for (uint32_t i = 0; i < 4; ++i)
    {
        slots[i] = pool.allocate();
        ASSERT_NE(slots[i], nullptr);
        slots[i]->id = i + 1;
    }

    EXPECT_TRUE(pool.deallocate(slots[1]));
    EXPECT_TRUE(pool.deallocate(slots[3]));

    uint32_t count = 0;
    uint32_t sum = 0;
    pool.for_each([&](PoolItem& item) {
        ++count;
        sum += item.id;
    });
    EXPECT_EQ(count, 2u);
    EXPECT_EQ(sum, 1u + 3u);

    count = 0;
    const Pool<PoolItem, 8>& const_pool = pool;
    const_pool.for_each([&](const PoolItem&) { ++count; });
    EXPECT_EQ(count, 2u);
}

TEST(PoolAllocator, RejectsForeignInteriorAndDoubleFree)
{
    Pool<PoolItem, 8> pool;
    PoolItem foreign{0, 0};

    EXPECT_FALSE(pool.deallocate(nullptr));
    EXPECT_FALSE(pool.deallocate(&foreign));

    PoolItem* pointer = pool.allocate();
    ASSERT_NE(pointer, nullptr);
    uint8_t* raw = reinterpret_cast<uint8_t*>(pointer);
    EXPECT_FALSE(pool.deallocate(reinterpret_cast<PoolItem*>(raw + 1)));

    EXPECT_TRUE(pool.deallocate(pointer));
    EXPECT_FALSE(pool.deallocate(pointer));
    EXPECT_EQ(pool.used(), 0u);
}

TEST(PoolAllocator, StressAllocFreeCycles)
{
    Pool<PoolItem, 256> pool;
    PoolItem* slots[256] = {};

    for (size_t i = 0; i < 256; ++i)
    {
        slots[i] = pool.allocate();
        ASSERT_NE(slots[i], nullptr);
        slots[i]->id = static_cast<uint32_t>(i);
    }
    EXPECT_EQ(pool.allocate(), nullptr);

    // Освобождаем каждый третий слот, затем снова заполняем пул.
    for (size_t i = 0; i < 256; i += 3)
        EXPECT_TRUE(pool.deallocate(slots[i]));

    size_t free_count = 0;
    for (size_t i = 0; i < 256; ++i)
    {
        if (i % 3 == 0)
        {
            ++free_count;
        }
    }

    PoolItem* refilled[256] = {};
    for (size_t i = 0; i < free_count; ++i)
    {
        refilled[i] = pool.allocate();
        ASSERT_NE(refilled[i], nullptr);
    }
    EXPECT_EQ(pool.used(), 256u);

    for (size_t i = 0; i < free_count; ++i)
        EXPECT_TRUE(pool.deallocate(refilled[i]));

    for (size_t i = 0; i < 256; ++i)
    {
        if (i % 3 != 0)
        {
            EXPECT_TRUE(pool.deallocate(slots[i]));
        }
    }

    EXPECT_EQ(pool.used(), 0u);

    for (uint32_t cycle = 0; cycle < 10000; ++cycle)
    {
        PoolItem* item = pool.allocate();
        ASSERT_NE(item, nullptr);
        item->id = cycle;
        EXPECT_TRUE(pool.deallocate(item));
    }
    EXPECT_EQ(pool.used(), 0u);
}
