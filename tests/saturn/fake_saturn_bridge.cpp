// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/fake_saturn_bridge.cpp
#include "tests/saturn/fake_saturn_bridge.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

namespace fake_saturn {

bool core_init_result = true;
int core_init_calls = 0;
int core_shutdown_calls = 0;

bool fs_available = true;
std::map<std::string, FakeFile> files;

uint16_t background_color = 0u;
RpeSaturnPadState pad = {0xFFFFu, 0u, 0u};
int pad_read_calls = 0;
uint32_t ticks = 0u;

bool slave_available = false;
bool slave_post_result = true;
bool slave_execute_posted = true;
int slave_post_calls = 0;
uint16_t frt_ticks = 0u;
uint16_t frt_step = 1u;
int cpu_relax_calls = 0;
int cache_purge_calls = 0;
int cache_through_calls = 0;

uint8_t vdp1_vram[512u * 1024u];
uint16_t vdp1_cram[2048u];
uint8_t vdp1_fb[2u][256u * 1024u];
uint16_t vdp1_erase_color = 0u;
uint8_t vdp1_displayed_buffer = 0u;
std::vector<RpeVdp1Command> vdp1_commands;
std::vector<uint16_t> vdp1_keys;
uint32_t vdp1_command_capacity = 1000u;
bool vdp1_upload_result = true;
int vdp1_init_calls = 0;
int vdp1_shutdown_calls = 0;
int vdp1_upload_calls = 0;
int vdp1_palette_calls = 0;
int vdp1_gouraud_calls = 0;

bool vdp2_available = false;
bool vdp2_init_result = true;
int vdp2_init_calls = 0;
int vdp2_shutdown_calls = 0;
int vdp2_palette_calls = 0;
int vdp2_upload_calls = 0;
int vdp2_scroll_calls = 0;
uint16_t vdp2_cram[2048u];
std::vector<uint8_t> vdp2_bitmap(1024u * 256u, 0u);
int16_t vdp2_scroll_x = 0;
int16_t vdp2_scroll_y = 0;

std::vector<void (*)(void)> posted_callbacks;

int log_calls = 0;
int last_log_level = -1;
std::string last_log_domain;
std::string last_log_message;

void (*frame_callback)(void) = nullptr;
void (*vblank_callback)(void) = nullptr;
int vblank_callback_calls = 0;

void reset(void)
{
    core_init_result = true;
    core_init_calls = 0;
    core_shutdown_calls = 0;
    fs_available = true;
    files.clear();
    background_color = 0u;
    pad.data = 0xFFFFu;
    pad.connected = 0u;
    pad.regular = 0u;
    pad_read_calls = 0;
    ticks = 0u;
    slave_available = false;
    slave_post_result = true;
    slave_execute_posted = true;
    slave_post_calls = 0;
    frt_ticks = 0u;
    frt_step = 1u;
    cpu_relax_calls = 0;
    cache_purge_calls = 0;
    cache_through_calls = 0;
    memset(vdp1_vram, 0, sizeof(vdp1_vram));
    memset(vdp1_cram, 0, sizeof(vdp1_cram));
    memset(vdp1_fb, 0, sizeof(vdp1_fb));
    vdp1_erase_color = 0u;
    vdp1_displayed_buffer = 0u;
    vdp1_commands.clear();
    vdp1_keys.clear();
    vdp1_command_capacity = 1000u;
    vdp1_upload_result = true;
    vdp1_init_calls = 0;
    vdp1_shutdown_calls = 0;
    vdp1_upload_calls = 0;
    vdp1_palette_calls = 0;
    vdp1_gouraud_calls = 0;
    vdp2_available = false;
    vdp2_init_result = true;
    vdp2_init_calls = 0;
    vdp2_shutdown_calls = 0;
    vdp2_palette_calls = 0;
    vdp2_upload_calls = 0;
    vdp2_scroll_calls = 0;
    memset(vdp2_cram, 0, sizeof(vdp2_cram));
    std::fill(vdp2_bitmap.begin(), vdp2_bitmap.end(), 0u);
    vdp2_scroll_x = 0;
    vdp2_scroll_y = 0;
    posted_callbacks.clear();
    log_calls = 0;
    last_log_level = -1;
    last_log_domain.clear();
    last_log_message.clear();
    frame_callback = nullptr;
    vblank_callback = nullptr;
    vblank_callback_calls = 0;
}

void add_file(const char* name, const void* data, uint32_t size)
{
    add_file(name, data, size, size, true);
}

void add_file(const char* name, const void* data, uint32_t data_size, uint32_t reported_size,
              bool read_ok)
{
    FakeFile file;
    if (data != nullptr && data_size != 0u) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        file.data.assign(bytes, bytes + data_size);
    }
    file.reported_size = reported_size;
    file.read_ok = read_ok;
    files[name] = file;
}

void run_posted_callbacks(void)
{
    // Один проход: callback может добавить новый (drain → handshake).
    std::vector<void (*)(void)> pending;
    pending.swap(posted_callbacks);
    for (void (*callback)(void) : pending) {
        if (callback != nullptr) {
            callback();
        }
    }
}

void fill_framebuffer(uint8_t buffer, uint16_t word)
{
    if (buffer > 1u) {
        return;
    }
    for (uint32_t y = 0u; y < 224u; ++y) {
        uint16_t* line = reinterpret_cast<uint16_t*>(vdp1_fb[buffer] + y * 1024u);
        for (uint32_t x = 0u; x < 320u; ++x) {
            line[x] = word;
        }
    }
}

uint32_t framebuffer_hash(uint8_t buffer)
{
    if (buffer > 1u) {
        return 0u;
    }
    uint32_t hash = 2166136261u;
    for (uint32_t y = 0u; y < 224u; ++y) {
        const uint16_t* line = reinterpret_cast<const uint16_t*>(vdp1_fb[buffer] + y * 1024u);
        for (uint32_t x = 0u; x < 320u; ++x) {
            const uint16_t word = line[x];
            hash ^= static_cast<uint32_t>(word & 0xFFu);
            hash *= 16777619u;
            hash ^= static_cast<uint32_t>((word >> 8) & 0xFFu);
            hash *= 16777619u;
        }
    }
    return hash;
}

}  // namespace fake_saturn

extern "C" {

bool rpe_saturn_core_init(uint16_t backdrop_color)
{
    ++fake_saturn::core_init_calls;
    fake_saturn::background_color = backdrop_color;
    return fake_saturn::core_init_result;
}

void rpe_saturn_core_shutdown(void)
{
    ++fake_saturn::core_shutdown_calls;
}

void rpe_saturn_core_run(void)
{
}

uint32_t rpe_saturn_ticks(void)
{
    return fake_saturn::ticks;
}

void rpe_saturn_set_background_color(uint16_t color)
{
    fake_saturn::background_color = color;
}

RpeSaturnPadState rpe_saturn_pad_read(void)
{
    ++fake_saturn::pad_read_calls;
    return fake_saturn::pad;
}

bool rpe_saturn_add_vblank_callback(void (*callback)(void))
{
    fake_saturn::vblank_callback = callback;
    ++fake_saturn::vblank_callback_calls;
    return callback != nullptr;
}

bool rpe_saturn_add_frame_callback(void (*callback)(void))
{
    fake_saturn::frame_callback = callback;
    return true;
}

bool rpe_saturn_slave_available(void)
{
    return fake_saturn::slave_available;
}

bool rpe_saturn_slave_post(void (*callback)(void))
{
    ++fake_saturn::slave_post_calls;
    if (!fake_saturn::slave_available || !fake_saturn::slave_post_result || callback == nullptr) {
        return false;
    }
    if (fake_saturn::slave_execute_posted) {
        callback();
    } else {
        fake_saturn::posted_callbacks.push_back(callback);
    }
    return true;
}

void* rpe_saturn_cache_through(const void* pointer)
{
    ++fake_saturn::cache_through_calls;
    return const_cast<void*>(pointer);
}

void rpe_saturn_cache_purge(void)
{
    ++fake_saturn::cache_purge_calls;
}

uint16_t rpe_saturn_frt_ticks(void)
{
    const uint16_t now = fake_saturn::frt_ticks;
    fake_saturn::frt_ticks = static_cast<uint16_t>(fake_saturn::frt_ticks + fake_saturn::frt_step);
    return now;
}

uint32_t rpe_saturn_frt_ticks_to_us(uint16_t ticks)
{
    return (static_cast<uint32_t>(ticks) * 1000u) / 839u;
}

void rpe_saturn_cpu_relax(void)
{
    ++fake_saturn::cpu_relax_calls;
    fake_saturn::frt_ticks = static_cast<uint16_t>(fake_saturn::frt_ticks + fake_saturn::frt_step);
}

bool rpe_saturn_fs_available(void)
{
    return fake_saturn::fs_available;
}
bool rpe_saturn_fs_get_size(const char* name, uint32_t* out_size)
{
    if (!fake_saturn::fs_available || name == nullptr || out_size == nullptr) {
        return false;
    }
    const auto it = fake_saturn::files.find(name);
    if (it == fake_saturn::files.end()) {
        return false;
    }
    *out_size = it->second.reported_size;
    return true;
}

bool rpe_saturn_fs_read(const char* name, uint8_t* dst, uint32_t size)
{
    if (!fake_saturn::fs_available || name == nullptr || dst == nullptr) {
        return false;
    }
    const auto it = fake_saturn::files.find(name);
    if (it == fake_saturn::files.end()) {
        return false;
    }
    const fake_saturn::FakeFile& file = it->second;
    if (!file.read_ok || file.reported_size != size || size > file.data.size()) {
        return false;
    }
    if (size != 0u) {
        memcpy(dst, file.data.data(), size);
    }
    return true;
}

bool rpe_saturn_vdp1_init(void)
{
    ++fake_saturn::vdp1_init_calls;
    return true;
}

void rpe_saturn_vdp1_shutdown(void)
{
    ++fake_saturn::vdp1_shutdown_calls;
}

void rpe_saturn_vdp1_begin_frame(void)
{
    fake_saturn::vdp1_commands.clear();
    fake_saturn::vdp1_keys.clear();
}

void rpe_saturn_vdp1_set_erase_color(uint16_t color)
{
    fake_saturn::vdp1_erase_color = color;
}

bool rpe_saturn_vdp1_upload_ci8(uint32_t vram_offset, const uint8_t* pixels, uint16_t width,
                                uint16_t height, uint16_t stride)
{
    ++fake_saturn::vdp1_upload_calls;
    if (pixels == nullptr || width == 0u || height == 0u || stride != width) {
        return false;
    }
    const uint32_t bytes = static_cast<uint32_t>(stride) * height;
    if (vram_offset > 0x7E4F8u || bytes > 0x7E4F8u - vram_offset) {
        return false;
    }
    if (!fake_saturn::vdp1_upload_result) {
        return false;
    }
    memcpy(fake_saturn::vdp1_vram + vram_offset, pixels, bytes);
    return true;
}

void rpe_saturn_vdp1_set_palette(uint8_t palette_id, const uint16_t* colors)
{
    ++fake_saturn::vdp1_palette_calls;
    if (colors == nullptr || palette_id < 1u || palette_id > 7u) {
        return;
    }
    for (uint32_t i = 0u; i < 256u; ++i) {
        fake_saturn::vdp1_cram[palette_id * 256u + i] = static_cast<uint16_t>(colors[i] | 0x8000u);
    }
}

void rpe_saturn_vdp1_write_gouraud(uint16_t table_index, const uint16_t colors[4])
{
    ++fake_saturn::vdp1_gouraud_calls;
    if (colors == nullptr || table_index >= 64u) {
        return;
    }
    uint16_t* dst =
        reinterpret_cast<uint16_t*>(fake_saturn::vdp1_vram + 0x7E4F8u + table_index * 8u);
    for (uint32_t i = 0u; i < 4u; ++i) {
        dst[i] = static_cast<uint16_t>(colors[i] & 0x7FFFu);
    }
}

bool rpe_saturn_vdp1_add_command(uint16_t sort_key, const RpeVdp1Command* command)
{
    if (command == nullptr ||
        fake_saturn::vdp1_commands.size() >= fake_saturn::vdp1_command_capacity) {
        return false;
    }
    fake_saturn::vdp1_commands.push_back(*command);
    fake_saturn::vdp1_keys.push_back(sort_key);
    return true;
}

bool rpe_saturn_vdp1_fb_hash(uint8_t buffer, uint32_t* out_hash)
{
    if (out_hash == nullptr || buffer > 1u) {
        return false;
    }
    *out_hash = fake_saturn::framebuffer_hash(buffer);
    return true;
}

uint8_t rpe_saturn_vdp1_displayed_buffer(void)
{
    return fake_saturn::vdp1_displayed_buffer;
}

uint32_t rpe_saturn_vdp1_added_commands(void)
{
    return static_cast<uint32_t>(fake_saturn::vdp1_commands.size());
}

bool rpe_saturn_vdp2_init(uint8_t palette_id)
{
    ++fake_saturn::vdp2_init_calls;
    if (!fake_saturn::vdp2_init_result || palette_id < 1u || palette_id > 7u) {
        return false;
    }
    fake_saturn::vdp2_available = true;
    return true;
}

void rpe_saturn_vdp2_shutdown(void)
{
    ++fake_saturn::vdp2_shutdown_calls;
    fake_saturn::vdp2_available = false;
}

void rpe_saturn_vdp2_set_palette(uint8_t palette_id, const uint16_t* colors)
{
    ++fake_saturn::vdp2_palette_calls;
    if (colors == nullptr || palette_id < 1u || palette_id > 7u) {
        return;
    }
    for (uint32_t i = 0u; i < 256u; ++i) {
        fake_saturn::vdp2_cram[palette_id * 256u + i] =
            static_cast<uint16_t>(colors[i] | 0x8000u);
    }
}

bool rpe_saturn_vdp2_upload_background(const uint8_t* pixels, uint16_t width, uint16_t height)
{
    ++fake_saturn::vdp2_upload_calls;
    if (!fake_saturn::vdp2_available || pixels == nullptr || width == 0u || height == 0u ||
        (width & 7u) != 0u || (height & 7u) != 0u || width > 1024u || height > 256u) {
        return false;
    }
    for (uint32_t y = 0u; y < 256u; y += height) {
        for (uint32_t x = 0u; x < 1024u; x += width) {
            const uint32_t copy_w = (x + width > 1024u) ? (1024u - x) : width;
            const uint32_t copy_h = (y + height > 256u) ? (256u - y) : height;
            for (uint32_t row = 0u; row < copy_h; ++row) {
                std::copy(pixels + row * width, pixels + row * width + copy_w,
                          fake_saturn::vdp2_bitmap.begin() + (y + row) * 1024u + x);
            }
        }
    }
    return true;
}

void rpe_saturn_vdp2_set_scroll(int16_t x, int16_t y)
{
    ++fake_saturn::vdp2_scroll_calls;
    fake_saturn::vdp2_scroll_x = x;
    fake_saturn::vdp2_scroll_y = y;
}

bool rpe_saturn_vdp2_available(void)
{
    return fake_saturn::vdp2_available;
}

void rpe_saturn_log_write_va(int level, const char* domain, const char* format, va_list* args)
{
    char message[256];
    message[0] = '\0';
    if (args != nullptr) {
        const int written =
            vsnprintf(message, sizeof(message), (format != nullptr) ? format : "(null)", *args);
        if (written >= (int)sizeof(message) - 1) {
            message[sizeof(message) - 5] = '.';
            message[sizeof(message) - 4] = '.';
            message[sizeof(message) - 3] = '.';
            message[sizeof(message) - 2] = '.';
            message[sizeof(message) - 1] = '\0';
        }
    }
    ++fake_saturn::log_calls;
    fake_saturn::last_log_level = level;
    fake_saturn::last_log_domain = (domain != nullptr) ? domain : "";
    fake_saturn::last_log_message = message;
}

}  // extern "C"
