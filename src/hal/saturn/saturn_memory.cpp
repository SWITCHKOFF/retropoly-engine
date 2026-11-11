// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_memory.cpp
//
// Арены Work RAM-L и регистрация в MemoryProfiler (Спринт 23–24, неделя 45).
// Контракты — saturn_memory.h; runtime heap (new/delete/malloc/free) не
// используется (ADR-007).
#include "hal/saturn/saturn_memory.h"

#include "core/log/log.h"
#include "core/memory/memory_profiler.h"
#include "hal/saturn/saturn_command_buffer.h"
#include "hal/saturn/vdp1_command_list.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace saturn {

struct AssetHeap::Block
{
    uint32_t magic;
    uint32_t size; /**< Полный размер блока, включая этот header. */
    Block* prev;
    Block* next;
    uint32_t is_free;
    uint32_t reserved;
};

namespace {

constexpr uint32_t kBlockMagic = 0x52504542u;  // "BEPR" little-endian
constexpr size_t kMinPayloadBytes = 8u;

AssetHeap s_asset_heap;
StackAllocator s_frame_scratch;

size_t align_up_size(size_t value, size_t alignment)
{
    return (value + (alignment - 1u)) & ~(alignment - 1u);
}

#if defined(PLATFORM_SATURN) && defined(__sh__)
// Work RAM-L на общей шине: 0x00200000..0x002FFFFF (SCU DMA сюда не ходит).
uint8_t* host_wram_l(void)
{
    return reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(0x00200000u));
}
#else
// Host-сборка (saturn_hal_tests): та же раскладка в static storage.
alignas(32) uint8_t s_host_wram_l[WRAM_L_BYTES];

uint8_t* host_wram_l(void)
{
    return s_host_wram_l;
}
#endif

size_t profiler_asset_used(void* user)
{
    return static_cast<AssetHeap*>(user)->used();
}

size_t profiler_frame_used(void* user)
{
    return static_cast<StackAllocator*>(user)->used();
}

size_t profiler_command_used(void* user)
{
    (void)user;
    const size_t legacy = command_buffer().used();
    const size_t vdp1 = vdp1_command_list().used_bytes();
    return (legacy > vdp1) ? legacy : vdp1;
}

}  // namespace

size_t AssetHeap::header_size()
{
    return align_up_size(sizeof(Block), 8u);
}

void* AssetHeap::payload_of(Block* block)
{
    return reinterpret_cast<uint8_t*>(block) + header_size();
}

bool AssetHeap::init(void* buffer, size_t capacity)
{
    m_buffer = nullptr;
    m_capacity = 0u;
    m_inited = false;
    m_head = nullptr;
    m_diag = ASSET_HEAP_OK;

    if (buffer == nullptr) {
        m_diag = ASSET_HEAP_NULL_ARGUMENT;
        return false;
    }

    const uintptr_t begin = reinterpret_cast<uintptr_t>(buffer);
    const uintptr_t aligned_begin = (begin + 7u) & ~static_cast<uintptr_t>(7u);
    const size_t skipped = static_cast<size_t>(aligned_begin - begin);
    if (capacity <= skipped) {
        m_diag = ASSET_HEAP_NULL_ARGUMENT;
        return false;
    }

    const size_t usable = (capacity - skipped) & ~static_cast<size_t>(7u);
    if (usable < header_size() + kMinPayloadBytes) {
        m_diag = ASSET_HEAP_OUT_OF_MEMORY;
        return false;
    }

    m_buffer = reinterpret_cast<uint8_t*>(aligned_begin);
    m_capacity = usable;
    m_head = reinterpret_cast<Block*>(m_buffer);
    m_head->magic = kBlockMagic;
    m_head->size = static_cast<uint32_t>(usable);
    m_head->prev = nullptr;
    m_head->next = nullptr;
    m_head->is_free = 1u;
    m_head->reserved = 0u;
    m_inited = true;
    m_diag = ASSET_HEAP_OK;
    return true;
}

void AssetHeap::reset()
{
    if (!m_inited || m_head == nullptr) {
        return;
    }
    m_head->size = static_cast<uint32_t>(m_capacity);
    m_head->prev = nullptr;
    m_head->next = nullptr;
    m_head->is_free = 1u;
    m_head->reserved = 0u;
    m_head->magic = kBlockMagic;
    m_diag = ASSET_HEAP_OK;
}

size_t AssetHeap::used() const
{
    if (!m_inited) {
        return 0u;
    }
    size_t total = 0u;
    const Block* block = m_head;
    while (block != nullptr) {
        if (block->is_free == 0u) {
            total += block->size;
        }
        block = block->next;
    }
    return total;
}

size_t AssetHeap::available() const
{
    if (!m_inited) {
        return 0u;
    }
    size_t total = 0u;
    const Block* block = m_head;
    while (block != nullptr) {
        if (block->is_free != 0u) {
            total += block->size;
        }
        block = block->next;
    }
    return total;
}

void AssetHeap::split(Block* block, size_t total_bytes)
{
    const size_t remainder = block->size - total_bytes;
    if (remainder < header_size() + kMinPayloadBytes) {
        return;  // Остаток слишком мал — отдаём целиком.
    }

    Block* next = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(block) + total_bytes);
    next->magic = kBlockMagic;
    next->size = static_cast<uint32_t>(remainder);
    next->prev = block;
    next->next = block->next;
    next->is_free = 1u;
    next->reserved = 0u;
    if (next->next != nullptr) {
        next->next->prev = next;
    }
    block->next = next;
    block->size = static_cast<uint32_t>(total_bytes);
}

void* AssetHeap::allocate(size_t size, size_t alignment)
{
    if (!m_inited || m_head == nullptr) {
        m_diag = ASSET_HEAP_NOT_INITIALIZED;
        return nullptr;
    }
    if (size == 0u) {
        m_diag = ASSET_HEAP_NULL_ARGUMENT;
        return nullptr;
    }
    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
        m_diag = ASSET_HEAP_BAD_ALIGNMENT;
        return nullptr;
    }
    if (alignment < 8u) {
        alignment = 8u;
    }
    if (size > static_cast<size_t>(UINT32_MAX)) {
        m_diag = ASSET_HEAP_OUT_OF_MEMORY;
        return nullptr;
    }

    const size_t payload_offset = header_size();
    Block* block = m_head;
    while (block != nullptr) {
        if (block->is_free == 0u) {
            block = block->next;
            continue;
        }

        const uintptr_t payload_addr = reinterpret_cast<uintptr_t>(payload_of(block));
        const uintptr_t padding = (static_cast<uintptr_t>(alignment) -
                                   (payload_addr & static_cast<uintptr_t>(alignment - 1u))) &
                                  static_cast<uintptr_t>(alignment - 1u);
        if (size > SIZE_MAX - static_cast<size_t>(padding)) {
            block = block->next;
            continue;
        }

        const size_t total = payload_offset + static_cast<size_t>(padding) + size;
        if (total < payload_offset || total > block->size) {
            block = block->next;
            continue;
        }

        split(block, total);
        block->is_free = 0u;
        block->reserved = static_cast<uint32_t>(padding);
        m_diag = ASSET_HEAP_OK;
        return reinterpret_cast<void*>(payload_addr + padding);
    }

    m_diag = ASSET_HEAP_OUT_OF_MEMORY;
    return nullptr;
}

bool AssetHeap::free(void* pointer)
{
    if (!m_inited || m_head == nullptr) {
        m_diag = ASSET_HEAP_NOT_INITIALIZED;
        return false;
    }
    if (pointer == nullptr) {
        m_diag = ASSET_HEAP_NULL_ARGUMENT;
        return false;
    }

    Block* block = m_head;
    while (block != nullptr) {
        void* block_payload = reinterpret_cast<uint8_t*>(payload_of(block)) + block->reserved;
        if (block_payload == pointer) {
            if (block->is_free != 0u) {
                m_diag = ASSET_HEAP_DOUBLE_FREE;
                return false;
            }
            block->is_free = 1u;

            Block* next = block->next;
            if (next != nullptr && next->is_free != 0u) {
                block->size += next->size;
                block->next = next->next;
                if (block->next != nullptr) {
                    block->next->prev = block;
                }
            }

            Block* prev = block->prev;
            if (prev != nullptr && prev->is_free != 0u) {
                prev->size += block->size;
                prev->next = block->next;
                if (prev->next != nullptr) {
                    prev->next->prev = prev;
                }
            }

            m_diag = ASSET_HEAP_OK;
            return true;
        }
        block = block->next;
    }

    m_diag = ASSET_HEAP_FOREIGN_POINTER;
    return false;
}

const char* AssetHeap::diag_string(AssetHeapDiag diag)
{
    switch (diag) {
        case ASSET_HEAP_OK:
            return "ok";
        case ASSET_HEAP_NOT_INITIALIZED:
            return "not initialized";
        case ASSET_HEAP_NULL_ARGUMENT:
            return "null argument";
        case ASSET_HEAP_BAD_ALIGNMENT:
            return "bad alignment";
        case ASSET_HEAP_OUT_OF_MEMORY:
            return "out of memory";
        case ASSET_HEAP_FOREIGN_POINTER:
            return "foreign pointer";
        case ASSET_HEAP_DOUBLE_FREE:
            return "double free";
        case ASSET_HEAP_COUNT:
        default:
            return "unknown";
    }
}

bool memory_init(void)
{
    uint8_t* base = host_wram_l();

    if (!s_asset_heap.init(base, ASSET_HEAP_BYTES)) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "asset heap init failed: %s",
                    AssetHeap::diag_string(s_asset_heap.diag()));
        return false;
    }

    s_frame_scratch.init(base + ASSET_HEAP_BYTES, FRAME_SCRATCH_BYTES);
    command_buffer().init(base + ASSET_HEAP_BYTES + FRAME_SCRATCH_BYTES, COMMAND_BUFFER_BYTES);
    if (!vdp1_command_list().init(base + ASSET_HEAP_BYTES + FRAME_SCRATCH_BYTES,
                                  COMMAND_BUFFER_BYTES)) {
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1 command list init failed");
        return false;
    }
    // S25–26: shared Master/Slave region — детерминированно обнуляется при
    // инициализации; worker/workload подключают его через cache-through alias.
    memset(parallel_work_base(), 0, PARALLEL_WORK_BYTES);

    memory_profiler_reset();
    memory_profiler_set_budget(MEMORY_PROFILER_RAM, WORK_RAM_BYTES);
    memory_profiler_set_budget(MEMORY_PROFILER_VRAM,
                               VDP1_VRAM_BYTES + VDP2_VRAM_BYTES + VDP2_COLOR_RAM_BYTES);
    memory_profiler_register_source("saturn_asset_heap", MEMORY_PROFILER_RAM, ASSET_HEAP_BYTES,
                                    profiler_asset_used, &s_asset_heap);
    memory_profiler_register_source("saturn_frame_scratch", MEMORY_PROFILER_RAM,
                                    FRAME_SCRATCH_BYTES, profiler_frame_used, &s_frame_scratch);
    memory_profiler_register_source("saturn_command_arena", MEMORY_PROFILER_RAM,
                                    COMMAND_BUFFER_BYTES, profiler_command_used, &command_buffer());
    return true;
}

void memory_shutdown(void)
{
    s_asset_heap.reset();
    s_frame_scratch.reset_frame();
    command_buffer().reset();
    vdp1_command_list().shutdown();
    memset(parallel_work_base(), 0, PARALLEL_WORK_BYTES);
}

AssetHeap& asset_heap(void)
{
    return s_asset_heap;
}

StackAllocator& frame_scratch(void)
{
    return s_frame_scratch;
}

uint8_t* wram_l_base(void)
{
    return host_wram_l();
}

uint8_t* parallel_work_base(void)
{
    return host_wram_l() + ASSET_HEAP_BYTES + FRAME_SCRATCH_BYTES + COMMAND_BUFFER_BYTES;
}

}  // namespace saturn
