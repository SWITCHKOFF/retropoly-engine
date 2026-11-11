// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp1_vram.cpp
//
// First-fit аллокатор области текстур VDP1 (контракты — заголовок).
#include "hal/saturn/vdp1_vram.h"

namespace saturn {

namespace {

constexpr uint32_t kAlignment = 32u;

uint32_t align_up(uint32_t value)
{
    return (value + (kAlignment - 1u)) & ~(kAlignment - 1u);
}

}  // namespace

void Vdp1TextureAllocator::init()
{
    m_count = 0u;
    m_inited = true;
    m_diag = VDP1_VRAM_OK;
    insert_block(0u, VDP1_TEXTURE_USABLE_BYTES, 0u);
}

void Vdp1TextureAllocator::shutdown()
{
    m_count = 0u;
    m_inited = false;
    m_diag = VDP1_VRAM_OK;
}

void Vdp1TextureAllocator::insert_block(uint32_t offset, uint32_t size, uint8_t used)
{
    if (m_count >= VDP1_TEXTURE_MAX_BLOCKS) {
        m_diag = VDP1_VRAM_NO_BLOCKS;
        return;
    }
    // Список держится в порядке адресов: вставка с хвоста (alloc/free
    // вызываются по одному блоку и обычно примыкают к уже существующим).
    uint32_t i = m_count;
    while (i > 0u && m_blocks[i - 1u].offset > offset) {
        m_blocks[i] = m_blocks[i - 1u];
        --i;
    }
    m_blocks[i].offset = offset;
    m_blocks[i].size = size;
    m_blocks[i].used = used;
    ++m_count;
}

void Vdp1TextureAllocator::coalesce()
{
    for (uint32_t i = 0u; i + 1u < m_count;) {
        Block& a = m_blocks[i];
        Block& b = m_blocks[i + 1u];
        if (a.used == 0u && b.used == 0u && a.offset + a.size == b.offset) {
            a.size += b.size;
            for (uint32_t j = i + 1u; j + 1u < m_count; ++j) {
                m_blocks[j] = m_blocks[j + 1u];
            }
            --m_count;
        } else {
            ++i;
        }
    }
}

uint32_t Vdp1TextureAllocator::allocate(uint32_t bytes)
{
    if (!m_inited) {
        m_diag = VDP1_VRAM_BAD_ARGUMENT;
        return VDP1_TEXTURE_INVALID_OFFSET;
    }
    if (bytes == 0u) {
        m_diag = VDP1_VRAM_BAD_ARGUMENT;
        return VDP1_TEXTURE_INVALID_OFFSET;
    }
    const uint32_t aligned = align_up(bytes);
    if (aligned < bytes) {  // переполнение align_up
        m_diag = VDP1_VRAM_OUT_OF_MEMORY;
        return VDP1_TEXTURE_INVALID_OFFSET;
    }

    for (uint32_t i = 0u; i < m_count; ++i) {
        Block& block = m_blocks[i];
        if (block.used != 0u || block.size < aligned) {
            continue;
        }
        if (block.size == aligned) {
            block.used = 1u;
            m_diag = VDP1_VRAM_OK;
            return block.offset;
        }

        // Разделение: занятый блок + свободный остаток.
        if (m_count >= VDP1_TEXTURE_MAX_BLOCKS) {
            m_diag = VDP1_VRAM_NO_BLOCKS;
            return VDP1_TEXTURE_INVALID_OFFSET;
        }
        for (uint32_t j = m_count; j > i + 1u; --j) {
            m_blocks[j] = m_blocks[j - 1u];
        }
        Block& tail = m_blocks[i + 1u];
        tail.offset = block.offset + aligned;
        tail.size = block.size - aligned;
        tail.used = 0u;
        ++m_count;
        block.size = aligned;
        block.used = 1u;
        m_diag = VDP1_VRAM_OK;
        return block.offset;
    }

    m_diag = VDP1_VRAM_OUT_OF_MEMORY;
    return VDP1_TEXTURE_INVALID_OFFSET;
}

bool Vdp1TextureAllocator::free(uint32_t offset)
{
    if (!m_inited) {
        m_diag = VDP1_VRAM_BAD_ARGUMENT;
        return false;
    }
    for (uint32_t i = 0u; i < m_count; ++i) {
        Block& block = m_blocks[i];
        if (block.offset != offset) {
            continue;
        }
        if (block.used == 0u) {
            m_diag = VDP1_VRAM_DOUBLE_FREE;
            return false;
        }
        block.used = 0u;
        coalesce();
        m_diag = VDP1_VRAM_OK;
        return true;
    }
    m_diag = VDP1_VRAM_FOREIGN_OFFSET;
    return false;
}

uint32_t Vdp1TextureAllocator::used() const
{
    uint32_t total = 0u;
    for (uint32_t i = 0u; i < m_count; ++i) {
        if (m_blocks[i].used != 0u) {
            total += m_blocks[i].size;
        }
    }
    return total;
}

uint32_t Vdp1TextureAllocator::available() const
{
    uint32_t total = 0u;
    for (uint32_t i = 0u; i < m_count; ++i) {
        if (m_blocks[i].used == 0u) {
            total += m_blocks[i].size;
        }
    }
    return total;
}

const char* Vdp1TextureAllocator::diag_string(Vdp1VramDiag diag)
{
    switch (diag) {
        case VDP1_VRAM_OK:
            return "ok";
        case VDP1_VRAM_BAD_ARGUMENT:
            return "bad argument";
        case VDP1_VRAM_OUT_OF_MEMORY:
            return "out of memory";
        case VDP1_VRAM_FOREIGN_OFFSET:
            return "foreign offset";
        case VDP1_VRAM_DOUBLE_FREE:
            return "double free";
        case VDP1_VRAM_NO_BLOCKS:
            return "block table full";
        case VDP1_VRAM_COUNT:
        default:
            return "unknown";
    }
}

}  // namespace saturn
