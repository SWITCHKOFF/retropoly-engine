// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_memory.h
//
// Бюджет памяти Sega Saturn (ADR-008, Спринт 21–22, неделя 43).
//
// Reference-факты сверены по первоисточникам (docs/SATURN.md):
//   - Work RAM: 1 MiB WRAM-L (0x06000000, CPU-bus) + 1 MiB WRAM-H
//     (0x00200000, общая шина) = 2 MiB;
//   - Sound RAM: 512 KiB (SCSP);
//   - VDP1: 512 KiB VRAM + два 256 KiB frame buffer (A/B);
//   - VDP2: 512 KiB VRAM + 4 KiB Color RAM (CRAM);
//   - SCU DSP: 32 KiB SRAM (в бюджет движка не входит).
//
// Runtime new/delete/malloc/free запрещены (ADR-007): все долгоживущие
// данные — static storage/арены, кадровые — StackAllocator/LinearAllocator.
// Заголовок не включает Jo Engine/SH-2: только числовые контракты бюджета.
#pragma once

#include <stdint.h>

namespace saturn {

constexpr uint32_t WRAM_L_BYTES = 1024u * 1024u;
constexpr uint32_t WRAM_H_BYTES = 1024u * 1024u;
constexpr uint32_t WORK_RAM_BYTES = WRAM_L_BYTES + WRAM_H_BYTES;

constexpr uint32_t SOUND_RAM_BYTES = 512u * 1024u;
constexpr uint32_t SCU_DSP_SRAM_BYTES = 32u * 1024u;

constexpr uint32_t VDP1_VRAM_BYTES = 512u * 1024u;
constexpr uint32_t VDP1_FRAME_BUFFER_BYTES = 256u * 1024u;
constexpr uint32_t VDP1_FRAME_BUFFER_COUNT = 2u;
constexpr uint32_t VDP2_VRAM_BYTES = 512u * 1024u;
constexpr uint32_t VDP2_COLOR_RAM_BYTES = 4u * 1024u;

// Jo Engine linker script: программа начинается с 0x06004000, master stack
// занимает верхние 16 KiB WRAM-L (0x060FBC00..0x060FFC00). Верх WRAM-L
// между концом образа и 0x060FBC00 отдаётся jo_malloc().
constexpr uint32_t MASTER_STACK_BYTES = 16u * 1024u;
constexpr uint32_t WRAM_L_PROGRAM_LIMIT_BYTES = WRAM_L_BYTES - MASTER_STACK_BYTES;

// v0.2.0: ассеты (mesh + CI8-текстуры + палитры) живут в WRAM-H; бюджет —
// весь 1 MiB. Фактическое потребление проверяется MemoryProfiler (S23–24).
constexpr uint32_t ASSET_ARENA_BYTES = WRAM_H_BYTES;

static_assert(WORK_RAM_BYTES == 2u * 1024u * 1024u,
              "Saturn Work RAM contract must stay 2 MiB (WRAM-L + WRAM-H)");
static_assert(WRAM_L_BYTES == WRAM_H_BYTES, "Saturn WRAM-L/WRAM-H split must stay 1 MiB + 1 MiB");
static_assert(SOUND_RAM_BYTES == 512u * 1024u, "Saturn sound RAM contract must stay 512 KiB");
static_assert(VDP1_VRAM_BYTES == 512u * 1024u, "VDP1 VRAM contract must stay 512 KiB (4 Mbit)");
static_assert(VDP1_FRAME_BUFFER_BYTES * VDP1_FRAME_BUFFER_COUNT == 512u * 1024u,
              "VDP1 frame buffers are 2 x 256 KiB (A/B page flip)");
static_assert(VDP2_VRAM_BYTES == 512u * 1024u,
              "VDP2 VRAM contract must stay 512 KiB (4 Mbit standard fit)");
static_assert(VDP2_COLOR_RAM_BYTES == 4u * 1024u,
              "VDP2 Color RAM contract must stay 4 KiB (32 Kbit)");
static_assert(SCU_DSP_SRAM_BYTES == 32u * 1024u, "SCU DSP SRAM contract must stay 32 KiB");
static_assert(WRAM_L_PROGRAM_LIMIT_BYTES + MASTER_STACK_BYTES == WRAM_L_BYTES,
              "WRAM-L ledger must cover program + master stack");
static_assert(ASSET_ARENA_BYTES == WRAM_H_BYTES, "v0.2.0 asset arena must fit entirely in WRAM-H");

}  // namespace saturn
