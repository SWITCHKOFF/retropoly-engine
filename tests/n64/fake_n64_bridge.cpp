// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/fake_n64_bridge.cpp
//
// Реализация C-моста N64 для host-тестов: без libdragon, счётчики
// Init/Shutdown/Sleep/input/frame и управляемые тики/DFS. Семантика
// совпадает с n64_bridge.c: Init/Shutdown идемпотентны, sleep двигает время,
// input_poll ровно один раз за вызов, FS читает точный размер.
#include "fake_n64_bridge.h"

#include "hal/n64/n64_bridge.h"

#include <string.h>

#include <string>
#include <vector>

namespace {

struct FakeFile
{
    std::string name;
    std::vector<uint8_t> data;
    int32_t reported_size;
    bool fail_read;
};

std::vector<FakeFile> s_files;

FakeFile* find_file(const char* name)
{
    if (name == nullptr) {
        return nullptr;
    }
    for (FakeFile& file : s_files) {
        if (file.name == name) {
            return &file;
        }
    }
    return nullptr;
}

}  // namespace

extern "C" {
int rpe_n64_fake_init_calls = 0;
int rpe_n64_fake_shutdown_calls = 0;
int rpe_n64_fake_sleep_calls = 0;
uint32_t rpe_n64_fake_ticks = 0u;
int rpe_n64_fake_init_result = 1;
int rpe_n64_fake_ready = 0;
uint32_t rpe_n64_fake_rdram_bytes = 4u * 1024u * 1024u;

int rpe_n64_fake_input_poll_calls = 0;
RpeN64PadState rpe_n64_fake_pads[4] = {};

int rpe_n64_fake_fs_ready = 0;
int rpe_n64_fake_fs_init_result = 1;

int rpe_n64_fake_render_available = 1;
int rpe_n64_fake_frame_begin_calls = 0;
int rpe_n64_fake_frame_end_calls = 0;
int rpe_n64_fake_frame_clear_calls = 0;
int rpe_n64_fake_text_calls = 0;
int rpe_n64_fake_frame_begin_result = 1;
int rpe_n64_fake_frame_active = 0;
}

void rpe_n64_fake_reset(void)
{
    rpe_n64_fake_init_calls = 0;
    rpe_n64_fake_shutdown_calls = 0;
    rpe_n64_fake_sleep_calls = 0;
    rpe_n64_fake_ticks = 0u;
    rpe_n64_fake_init_result = 1;
    rpe_n64_fake_ready = 0;
    rpe_n64_fake_rdram_bytes = 4u * 1024u * 1024u;

    rpe_n64_fake_input_poll_calls = 0;
    memset(rpe_n64_fake_pads, 0, sizeof(rpe_n64_fake_pads));

    rpe_n64_fake_fs_ready = 0;
    rpe_n64_fake_fs_init_result = 1;
    s_files.clear();

    rpe_n64_fake_render_available = 1;
    rpe_n64_fake_frame_begin_calls = 0;
    rpe_n64_fake_frame_end_calls = 0;
    rpe_n64_fake_frame_clear_calls = 0;
    rpe_n64_fake_text_calls = 0;
    rpe_n64_fake_frame_begin_result = 1;
    rpe_n64_fake_frame_active = 0;
}

void rpe_n64_fake_add_file(const char* name, const uint8_t* data, uint32_t size,
                           int32_t reported_size)
{
    if (name == nullptr) {
        return;
    }
    rpe_n64_fake_remove_file(name);
    FakeFile file;
    file.name = name;
    if (data != nullptr && size != 0u) {
        file.data.assign(data, data + size);
    }
    file.reported_size = reported_size;
    file.fail_read = false;
    s_files.push_back(file);
}

void rpe_n64_fake_remove_file(const char* name)
{
    if (name == nullptr) {
        return;
    }
    for (size_t i = 0u; i < s_files.size(); ++i) {
        if (s_files[i].name == name) {
            s_files.erase(s_files.begin() + (std::ptrdiff_t)i);
            return;
        }
    }
}

void rpe_n64_fake_fail_read(const char* name, int fail)
{
    FakeFile* file = find_file(name);
    if (file != nullptr) {
        file->fail_read = fail != 0;
    }
}

uint32_t rpe_n64_fake_file_count(void)
{
    return (uint32_t)s_files.size();
}

extern "C" bool rpe_n64_core_init(void)
{
    ++rpe_n64_fake_init_calls;
    if (!rpe_n64_fake_init_result) {
        return false;
    }
    rpe_n64_fake_ready = 1;
    return true;
}

extern "C" void rpe_n64_core_shutdown(void)
{
    ++rpe_n64_fake_shutdown_calls;
    rpe_n64_fake_ready = 0;
}

extern "C" bool rpe_n64_core_ready(void)
{
    return rpe_n64_fake_ready != 0;
}

extern "C" uint32_t rpe_n64_ticks_ms(void)
{
    return rpe_n64_fake_ticks;
}

extern "C" void rpe_n64_sleep_ms(uint32_t ms)
{
    ++rpe_n64_fake_sleep_calls;
    rpe_n64_fake_ticks += ms;
}

extern "C" uint32_t rpe_n64_rdram_size(void)
{
    return rpe_n64_fake_rdram_bytes;
}

extern "C" void rpe_n64_log_write_va(int level, const char* domain, const char* format,
                                     va_list* args)
{
    (void)level;
    (void)domain;
    (void)format;
    (void)args;
}

extern "C" void rpe_n64_input_poll(void)
{
    ++rpe_n64_fake_input_poll_calls;
}

extern "C" bool rpe_n64_input_read(uint32_t port, RpeN64PadState* out)
{
    if (out == nullptr || port >= 4u) {
        return false;
    }
    *out = rpe_n64_fake_pads[port];
    if (!out->connected) {
        *out = RpeN64PadState{};
        return false;
    }
    return true;
}

extern "C" bool rpe_n64_fs_init(void)
{
    if (!rpe_n64_fake_fs_init_result) {
        rpe_n64_fake_fs_ready = 0;
        return false;
    }
    rpe_n64_fake_fs_ready = 1;
    return true;
}

extern "C" void rpe_n64_fs_shutdown(void)
{
    rpe_n64_fake_fs_ready = 0;
}

extern "C" bool rpe_n64_fs_ready(void)
{
    return rpe_n64_fake_fs_ready != 0;
}

extern "C" int32_t rpe_n64_fs_size(const char* name)
{
    if (!rpe_n64_fake_fs_ready) {
        return -1;
    }
    const FakeFile* file = find_file(name);
    if (file == nullptr) {
        return -1;
    }
    if (file->reported_size >= 0) {
        return file->reported_size;
    }
    return (int32_t)file->data.size();
}

extern "C" int32_t rpe_n64_fs_read(const char* name, uint8_t* dst, uint32_t size)
{
    if (!rpe_n64_fake_fs_ready || dst == nullptr) {
        return -1;
    }
    const FakeFile* file = find_file(name);
    if (file == nullptr || file->fail_read || file->data.size() < size) {
        return -1;
    }
    memcpy(dst, file->data.data(), size);
    return (int32_t)size;
}

extern "C" bool rpe_n64_render_available(void)
{
    return rpe_n64_fake_render_available != 0;
}

extern "C" bool rpe_n64_frame_begin(uint8_t r, uint8_t g, uint8_t b)
{
    (void)r;
    (void)g;
    (void)b;
    ++rpe_n64_fake_frame_begin_calls;
    if (!rpe_n64_fake_frame_begin_result || rpe_n64_fake_frame_active) {
        return false;
    }
    rpe_n64_fake_frame_active = 1;
    return true;
}

extern "C" void rpe_n64_frame_clear(uint8_t r, uint8_t g, uint8_t b)
{
    (void)r;
    (void)g;
    (void)b;
    if (rpe_n64_fake_frame_active) {
        ++rpe_n64_fake_frame_clear_calls;
    }
}

extern "C" void rpe_n64_frame_end(void)
{
    ++rpe_n64_fake_frame_end_calls;
    rpe_n64_fake_frame_active = 0;
}

extern "C" bool rpe_n64_frame_active(void)
{
    return rpe_n64_fake_frame_active != 0;
}

extern "C" bool rpe_n64_text_print(int x, int y, const char* text)
{
    (void)x;
    (void)y;
    if (!rpe_n64_fake_frame_active || text == nullptr) {
        return false;
    }
    ++rpe_n64_fake_text_calls;
    return true;
}
