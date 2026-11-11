// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/fake_saturn_bridge.h
//
// Host-double C-моста Jo Engine для saturn_hal_tests. Позволяет тестировать
// Saturn HAL без SH-2/эмулятора: core init/rollback, pad, FS, VDP2-цвет и
// VDP2-log sink наблюдаются из тестов.
#pragma once

#include "hal/saturn/jo_saturn_bridge.h"

#include <map>
#include <string>
#include <vector>

namespace fake_saturn {

struct FakeFile
{
    std::vector<uint8_t> data;
    uint32_t reported_size;
    bool read_ok;
};

void reset(void);

extern bool core_init_result;
extern int core_init_calls;
extern int core_shutdown_calls;

extern bool fs_available;
extern std::map<std::string, FakeFile> files;

extern uint16_t background_color;
extern RpeSaturnPadState pad;
extern int pad_read_calls;
extern uint32_t ticks;

// --- S25–26: fake Master+Slave ---------------------------------------------
extern bool slave_available;
extern bool slave_post_result;
extern bool slave_execute_posted;
extern int slave_post_calls;
extern uint16_t frt_ticks;
extern uint16_t frt_step;
extern int cpu_relax_calls;
extern int cache_purge_calls;
extern int cache_through_calls;

// --- S27–28: fake VDP1 -------------------------------------------------------
extern uint8_t vdp1_vram[512u * 1024u];
extern uint16_t vdp1_cram[2048u];
extern uint8_t vdp1_fb[2u][256u * 1024u];
extern uint16_t vdp1_erase_color;
extern uint8_t vdp1_displayed_buffer;
extern std::vector<RpeVdp1Command> vdp1_commands;
extern std::vector<uint16_t> vdp1_keys;
extern uint32_t vdp1_command_capacity;
extern bool vdp1_upload_result;
extern int vdp1_init_calls;
extern int vdp1_shutdown_calls;
extern int vdp1_upload_calls;
extern int vdp1_palette_calls;
extern int vdp1_gouraud_calls;

// --- S29–30: fake VDP2 NBG1 bitmap ------------------------------------------
extern bool vdp2_available;
extern bool vdp2_init_result;
extern int vdp2_init_calls;
extern int vdp2_shutdown_calls;
extern int vdp2_palette_calls;
extern int vdp2_upload_calls;
extern int vdp2_scroll_calls;
extern uint16_t vdp2_cram[2048u];
extern std::vector<uint8_t> vdp2_bitmap;
extern int16_t vdp2_scroll_x;
extern int16_t vdp2_scroll_y;

/** Заполнить 320×224 область framebuffer словом @p word. */
void fill_framebuffer(uint8_t buffer, uint16_t word);

/** @return FNV-1a 32 по 320×224 словам (как rpe_saturn_vdp1_fb_hash). */
uint32_t framebuffer_hash(uint8_t buffer);

/** Выполнить накопленные callback'и (когда slave_execute_posted == false). */
void run_posted_callbacks(void);

extern int log_calls;
extern int last_log_level;
extern std::string last_log_domain;
extern std::string last_log_message;

extern void (*frame_callback)(void);
extern void (*vblank_callback)(void);
extern int vblank_callback_calls;

/** Добавить корректный файл (reported_size == data.size()). */
void add_file(const char* name, const void* data, uint32_t size);

/** Добавить файл с «лживым» размером/сбоем чтения. */
void add_file(const char* name, const void* data, uint32_t data_size, uint32_t reported_size,
              bool read_ok);

}  // namespace fake_saturn
