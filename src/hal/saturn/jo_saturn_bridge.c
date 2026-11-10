// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/jo_saturn_bridge.c
//
// Единственный translation unit HAL Saturn, включающий <jo/jo.h> (C99).
// См. jo_saturn_bridge.h: C++ HAL не видит C-заголовки Jo Engine напрямую.
#include "hal/saturn/jo_saturn_bridge.h"

#include <jo/jo.h>
#include "internal/fs.h"

static bool s_core_inited = false;

static void core_disable_interrupts(void)
{
    jo_core_tv_off();
    JO_SCU_IMS &= ~((unsigned int)JO_SCU_INT_ALL);
    JO_BIOS_SCU_IMS &= ~((unsigned int)JO_SCU_INT_ALL);
}

bool rpe_saturn_core_init(uint16_t backdrop_color)
{
    if (s_core_inited) return (true);

    // jo_core_init() не возвращает статус. В release (без JO_DEBUG) сбой
    // инициализации VDP/FS молчалив, поэтому проверяем наблюдаемые
    // инварианты: разрешение, NTSC TVSTAT и наличие 0.BIN в ISO9660.
    jo_core_init((jo_color)backdrop_color);

    bool ok = (jo_core_get_screen_width() == 320 && jo_core_get_screen_height() == 224);
    if (ok) ok = ((JO_VDP2_TVSTAT & JO_VDP2_TVSTAT_PAL) == 0);
    if (ok) ok = (__jo_fs_get_file_id("0.BIN") >= 0);

    if (!ok) {
        core_disable_interrupts();
        return (false);
    }

    s_core_inited = true;
    return (true);
}

void rpe_saturn_core_shutdown(void)
{
    core_disable_interrupts();
    s_core_inited = false;
}

void rpe_saturn_core_run(void)
{
    jo_core_run();
}

uint32_t rpe_saturn_ticks(void)
{
    return ((uint32_t)jo_get_ticks());
}

void rpe_saturn_set_background_color(uint16_t color)
{
    if (!s_core_inited) return;
    jo_set_default_background_color((jo_color)color);
}

RpeSaturnPadState rpe_saturn_pad_read(void)
{
    RpeSaturnPadState state = {0xFFFFu, 0u, 0u};

    if (!jo_is_pad1_available()) return (state);

    state.connected = 1u;
    if (jo_get_input_type(0) == JoRegularGamepad) {
        state.regular = 1u;
        state.data = jo_inputs[0].data;
    }
    return (state);
}

bool rpe_saturn_add_frame_callback(void (*callback)(void))
{
    if (callback == 0) return (false);
    return (jo_core_add_callback((jo_event_callback)callback) >= 0);
}

bool rpe_saturn_fs_available(void)
{
    return (__jo_fs_get_file_id("0.BIN") >= 0);
}

bool rpe_saturn_fs_get_size(const char* name, uint32_t* out_size)
{
    if (name == JO_NULL || out_size == JO_NULL) return (false);

    const int file_id = __jo_fs_get_file_id(name);
    if (file_id < 0) return (false);

    void* handle = __jo_fs_open(file_id);
    if (handle == JO_NULL) return (false);

    const int size = __jo_fs_get_file_size(handle);
    __jo_fs_close(handle);
    if (size < 0) return (false);

    *out_size = (uint32_t)size;
    return (true);
}

bool rpe_saturn_fs_read(const char* name, uint8_t* dst, uint32_t size)
{
    if (name == JO_NULL || (dst == JO_NULL && size != 0u)) return (false);

    const int file_id = __jo_fs_get_file_id(name);
    if (file_id < 0) return (false);

    void* handle = __jo_fs_open(file_id);
    if (handle == JO_NULL) return (false);

    if ((uint32_t)__jo_fs_get_file_size(handle) != size) {
        __jo_fs_close(handle);
        return (false);
    }

    uint32_t offset = 0u;
    while (offset < size) {
        const uint32_t remaining = size - offset;
        // ISO9660 CD-сектор — 2048 байт (JO_SECTOR_SIZE — приватный fs.c).
        int sectors = (int)((remaining + 2047u) / 2048u);
        if (sectors > 16) sectors = 16;

        const int read = __jo_fs_read_sectors(handle, sectors, dst + offset, (int)remaining);
        if (read <= 0) {
            __jo_fs_close(handle);
            return (false);
        }
        offset += (uint32_t)read;
    }

    __jo_fs_close(handle);
    return (offset == size);
}

void rpe_saturn_log_write_va(int level, const char* domain, const char* format, va_list* args)
{
    static const char* const tags[] = {"INFO", "WARNING", "ERROR"};
    const char* tag = (level >= 0 && level <= 2) ? tags[level] : tags[2];

    // Отдельный буфер сообщения: jo_vsnprintf не использует newlib/heap.
    char message[256];
    message[0] = '\0';
    if (args != JO_NULL) {
        const int written = jo_vsnprintf(message, (unsigned int)sizeof(message),
                                         (format != JO_NULL) ? format : "(null)", *args);
        if (written >= (int)sizeof(message) - 1) {
            // Контракт log.h: усечение помечается ASCII-суффиксом «....».
            message[sizeof(message) - 5] = '.';
            message[sizeof(message) - 4] = '.';
            message[sizeof(message) - 3] = '.';
            message[sizeof(message) - 2] = '.';
            message[sizeof(message) - 1] = '\0';
        }
    }

    char line[41];
    const int written =
        jo_snprintf(line, (unsigned int)sizeof(line), "[%s][%s] %s", tag,
                    (domain != JO_NULL && domain[0] != '\0') ? domain : "?", message);
    int length = (written > 0) ? written : 0;
    if (length > 40) length = 40;
    for (int i = length; i < 40; ++i)
        line[i] = ' ';
    line[40] = '\0';
    jo_print(0, 27, line);
}
