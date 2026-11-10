// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/fake_saturn_bridge.cpp
#include "tests/saturn/fake_saturn_bridge.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

void (*frame_callback)(void) = nullptr;

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
    log_calls = 0;
    last_log_level = -1;
    last_log_domain.clear();
    last_log_message.clear();
    frame_callback = nullptr;
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

bool rpe_saturn_add_frame_callback(void (*callback)(void))
{
    fake_saturn::frame_callback = callback;
    return true;
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
