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

extern int log_calls;
extern int last_log_level;
extern std::string last_log_domain;
extern std::string last_log_message;

extern void (*frame_callback)(void);

/** Добавить корректный файл (reported_size == data.size()). */
void add_file(const char* name, const void* data, uint32_t size);

/** Добавить файл с «лживым» размером/сбоем чтения. */
void add_file(const char* name, const void* data, uint32_t data_size, uint32_t reported_size,
              bool read_ok);

}  // namespace fake_saturn
