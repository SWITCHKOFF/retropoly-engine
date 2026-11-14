// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_render_utils.cpp
#include "hal/saturn/saturn_render_utils.h"

namespace saturn {

uint16_t rgb888_to_saturn(uint8_t r, uint8_t g, uint8_t b)
{
    const uint16_t r5 = static_cast<uint16_t>((r / 8u) & 0x1Fu);
    const uint16_t g5 = static_cast<uint16_t>((g / 8u) & 0x1Fu);
    const uint16_t b5 = static_cast<uint16_t>((b / 8u) & 0x1Fu);
    return static_cast<uint16_t>(0x8000u | (b5 << 10) | (g5 << 5) | r5);
}

uint32_t frame_word_hash(uint16_t word, uint32_t width, uint32_t height)
{
    uint32_t hash = 2166136261u;  // FNV offset basis
    const uint8_t bytes[2] = {static_cast<uint8_t>(word & 0xFFu),
                              static_cast<uint8_t>((word >> 8) & 0xFFu)};
    const uint32_t pixels = width * height;
    for (uint32_t i = 0u; i < pixels; ++i) {
        hash ^= bytes[0];
        hash *= 16777619u;
        hash ^= bytes[1];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t clear_golden_hash(void)
{
    return frame_word_hash(CLEAR_GOLDEN_WORD, SCREEN_WIDTH, SCREEN_HEIGHT);
}

}  // namespace saturn
