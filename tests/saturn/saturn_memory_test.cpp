// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_memory_test.cpp
//
// Арены Work RAM-L Saturn: first-fit/coalescing, double-free/foreign/OOM,
// раскладка регионов и регистрация в MemoryProfiler.
#include "hal/saturn/saturn_memory.h"

#include "core/memory/memory_profiler.h"
#include "hal/saturn/saturn_command_buffer.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

#include <stdint.h>

#include <cstddef>

namespace {

alignas(16) uint8_t s_heap_buffer[512];

class AssetHeapTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_TRUE(m_heap.init(s_heap_buffer, sizeof(s_heap_buffer))); }

    saturn::AssetHeap m_heap;
};

}  // namespace

TEST_F(AssetHeapTest, AllocateAlignAndFree)
{
    void* a = m_heap.allocate(17u, 64u);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(a) % 64u, 0u);

    void* b = m_heap.allocate(17u, 8u);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);

    EXPECT_GT(m_heap.used(), 0u);
    EXPECT_TRUE(m_heap.free(a));
    EXPECT_TRUE(m_heap.free(b));
    EXPECT_EQ(m_heap.used(), 0u);
    EXPECT_EQ(m_heap.available(), m_heap.capacity());
}

TEST_F(AssetHeapTest, FreeCoalescesNeighbors)
{
    void* a = m_heap.allocate(100u);
    void* b = m_heap.allocate(100u);
    void* c = m_heap.allocate(100u);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    ASSERT_TRUE(m_heap.free(a));
    ASSERT_TRUE(m_heap.free(b));

    // 200 байт помещаются только в объединённый блок a+b.
    void* merged = m_heap.allocate(200u);
    EXPECT_NE(merged, nullptr);
    EXPECT_EQ(m_heap.diag(), saturn::ASSET_HEAP_OK);
    EXPECT_TRUE(m_heap.free(merged));
    EXPECT_TRUE(m_heap.free(c));
}

TEST_F(AssetHeapTest, DoubleFreeIsRejected)
{
    void* a = m_heap.allocate(32u);
    ASSERT_NE(a, nullptr);
    ASSERT_TRUE(m_heap.free(a));
    EXPECT_FALSE(m_heap.free(a));
    EXPECT_EQ(m_heap.diag(), saturn::ASSET_HEAP_DOUBLE_FREE);
}

TEST_F(AssetHeapTest, ForeignPointerIsRejected)
{
    int local = 0;
    EXPECT_FALSE(m_heap.free(&local));
    EXPECT_EQ(m_heap.diag(), saturn::ASSET_HEAP_FOREIGN_POINTER);
}

TEST_F(AssetHeapTest, OutOfMemoryAndInvalidArguments)
{
    EXPECT_EQ(m_heap.allocate(0u), nullptr);
    EXPECT_EQ(m_heap.diag(), saturn::ASSET_HEAP_NULL_ARGUMENT);

    EXPECT_EQ(m_heap.allocate(64u, 3u), nullptr);
    EXPECT_EQ(m_heap.diag(), saturn::ASSET_HEAP_BAD_ALIGNMENT);

    EXPECT_EQ(m_heap.allocate(sizeof(s_heap_buffer) * 4u), nullptr);
    EXPECT_EQ(m_heap.diag(), saturn::ASSET_HEAP_OUT_OF_MEMORY);
    EXPECT_EQ(m_heap.used(), 0u);
}

TEST(SaturnMemory, WramLRegionsAreContiguousAndProfiled)
{
    fake_saturn::reset();  // fake bridge не нужен, но reset держит тесты изолированными.
    saturn::memory_shutdown();
    ASSERT_TRUE(saturn::memory_init());

    EXPECT_EQ(saturn::asset_heap().capacity(), saturn::ASSET_HEAP_BYTES);
    EXPECT_EQ(saturn::frame_scratch().capacity(), saturn::FRAME_SCRATCH_BYTES);
    EXPECT_EQ(saturn::command_buffer().capacity(), saturn::COMMAND_BUFFER_BYTES);

    uint8_t* base = saturn::wram_l_base();
    EXPECT_EQ(saturn::frame_scratch().capacity() + saturn::asset_heap().capacity() +
                  saturn::command_buffer().capacity() + saturn::parallel_work_bytes(),
              saturn::WRAM_L_BYTES);
    EXPECT_NE(base, nullptr);
    EXPECT_EQ(memory_profiler_total_used(MEMORY_PROFILER_RAM), 0u);

    void* block = saturn::asset_heap().allocate(128u);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(memory_profiler_total_used(MEMORY_PROFILER_RAM), saturn::asset_heap().used());
    EXPECT_TRUE(saturn::asset_heap().free(block));

    saturn::memory_shutdown();
}
