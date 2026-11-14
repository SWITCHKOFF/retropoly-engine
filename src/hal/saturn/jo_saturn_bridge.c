// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/jo_saturn_bridge.c
//
// Единственный translation unit HAL Saturn, включающий <jo/jo.h> (C99).
// См. jo_saturn_bridge.h: C++ HAL не видит C-заголовки Jo Engine напрямую.
#include "hal/saturn/jo_saturn_bridge.h"

#include <jo/jo.h>
#include "internal/core.h"
#include "internal/fs.h"
#include "internal/vdp1_command_pipeline.h"

#include <string.h>

static bool s_core_inited = false;
static uint32_t s_vdp1_added_commands = 0u;

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

bool rpe_saturn_add_vblank_callback(void (*callback)(void))
{
    if (callback == 0) return (false);
    return (jo_core_add_vblank_callback((jo_event_callback)callback) >= 0);
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

// ---------------------------------------------------------------------------
// Master + Slave SH-2 (S25–26)
// ---------------------------------------------------------------------------

bool rpe_saturn_slave_available(void)
{
#ifdef JO_COMPILE_WITH_DUAL_CPU_SUPPORT
    return (s_core_inited && __jo_core_is_slave_running());
#else
    return (false);
#endif
}

bool rpe_saturn_slave_post(void (*callback)(void))
{
#ifdef JO_COMPILE_WITH_DUAL_CPU_SUPPORT
    if (callback == 0 || !rpe_saturn_slave_available()) return (false);
    jo_core_exec_on_slave((jo_slave_callback)callback);
    return (true);
#else
    (void)callback;
    return (false);
#endif
}

void* rpe_saturn_cache_through(const void* pointer)
{
    if (pointer == JO_NULL) return (JO_NULL);
    return ((void*)JO_CACHE_THROUGH(pointer));
}

void rpe_saturn_cache_purge(void)
{
    unsigned char ccr = JO_SH2_CCR;

    // Последовательность Jo Engine: выключить кэш, purge, восстановить CCR.
    JO_SH2_CCR = (unsigned char)(ccr & ~JO_SH2_CCR_CACHE_ENABLE);
    JO_SH2_CCR = (unsigned char)((ccr & ~JO_SH2_CCR_CACHE_ENABLE) | JO_SH2_CCR_CACHE_PURGE);
    JO_SH2_CCR = ccr;
}

uint16_t rpe_saturn_frt_ticks(void)
{
    return ((uint16_t)jo_time_get_frc());
}

uint32_t rpe_saturn_frt_ticks_to_us(uint16_t ticks)
{
    // Та же константа, что в slave.c Jo Engine: FRT = clock / 32.
#if (JO_TV_WIDTH == 352) || (JO_TV_WIDTH == 704)
    return (((uint32_t)ticks * 1000u) / 895u);
#else
    return (((uint32_t)ticks * 1000u) / 839u);
#endif
}

void rpe_saturn_cpu_relax(void)
{
    __jo_core_pause();
}

// ---------------------------------------------------------------------------
// VDP1: полигоны, текстуры, CLUT, Gouraud (S27–28)
// ---------------------------------------------------------------------------

// Карта RPE в VDP1 VRAM (должна совпадать с vdp1_vram.h; C-мост не видит C++).
#define RPE_VDP1_GOURAUD_BASE 0x7E4F8u
#define RPE_VDP1_GOURAUD_SLOTS 64u
#define RPE_VDP1_TEXTURE_END 0x7E4F8u
#define RPE_VDP1_FB_LINE_BYTES 1024u  // 512 пикселей × 2 байта
#define RPE_VDP1_FB_PLANE_BYTES 0x40000u

typedef char rpe_vdp1_command_size_check[(sizeof(RpeVdp1Command) == 32u) ? 1 : -1];
typedef char jo_vdp1_command_size_check[(sizeof(jo_vdp1_command) == 32u) ? 1 : -1];

bool rpe_saturn_vdp1_init(void)
{
    if (!s_core_inited) return (false);
    s_vdp1_added_commands = 0u;
    return (true);
}

void rpe_saturn_vdp1_shutdown(void)
{
    s_vdp1_added_commands = 0u;
}

void rpe_saturn_vdp1_begin_frame(void)
{
    s_vdp1_added_commands = 0u;
}

void rpe_saturn_vdp1_set_erase_color(uint16_t color)
{
    if (!s_core_inited) return;
    JO_VDP1_EWDR = color;
}

bool rpe_saturn_vdp1_upload_ci8(uint32_t vram_offset, const uint8_t* pixels, uint16_t width,
                                uint16_t height, uint16_t stride)
{
    if (!s_core_inited || pixels == JO_NULL) return (false);
    if (width == 0u || height == 0u || stride != width) return (false);
    const uint32_t bytes = (uint32_t)stride * (uint32_t)height;
    if (vram_offset > RPE_VDP1_TEXTURE_END || bytes > RPE_VDP1_TEXTURE_END - vram_offset)
        return (false);
    if ((bytes & 1u) != 0u) return (false);

    // DMA-копия в VDP1 VRAM (как jo_sprite_add_8bits_image): SCU DMA не
    // зависит от кэша SH-2 и гарантирует видимость данных для VDP1. Для
    // мелких/невыровненных буферов Jo Engine сам откатывается на memcpy.
    jo_dma_copy((const void*)pixels, (void*)(JO_VDP1_VRAM + vram_offset), bytes);
    return (true);
}

void rpe_saturn_vdp1_set_palette(uint8_t palette_id, const uint16_t* colors)
{
    if (!s_core_inited || colors == JO_NULL) return;
    if (palette_id < 1u || palette_id > 7u) return;

    volatile uint16_t* cram = (volatile uint16_t*)JO_VDP2_CRAM;
    const uint32_t base = (uint32_t)palette_id * 256u;
    uint32_t i;
    for (i = 0u; i < 256u; ++i)
    {
        // VDP1 CLUT: RGB555 + MSB (как jo_color Jo Engine); STP MVP выключен.
        cram[base + i] = (uint16_t)(colors[i] | 0x8000u);
    }
}

void rpe_saturn_vdp1_write_gouraud(uint16_t table_index, const uint16_t colors[4])
{
    if (!s_core_inited || colors == JO_NULL) return;
    if (table_index >= RPE_VDP1_GOURAUD_SLOTS) return;

    volatile uint16_t* dst =
        (volatile uint16_t*)(JO_VDP1_VRAM + RPE_VDP1_GOURAUD_BASE + (uint32_t)table_index * 8u);
    uint32_t i;
    for (i = 0u; i < 4u; ++i)
        dst[i] = (uint16_t)(colors[i] | 0x8000u);
}

bool rpe_saturn_vdp1_add_command(uint16_t sort_key, const RpeVdp1Command* command)
{
    if (!s_core_inited || command == JO_NULL) return (false);

    jo_vdp1_command* cmd = __jo_vdp1_add_command(jo_int2fixed((int)sort_key));
    if (cmd == JO_NULL) return (false);

    memcpy(cmd, command, sizeof(RpeVdp1Command));
    // link строит pipeline при отправке кадра; 0 — безопасное значение.
    cmd->link = 0;
    ++s_vdp1_added_commands;
    return (true);
}

bool rpe_saturn_vdp1_fb_hash(uint8_t buffer, uint32_t* out_hash)
{
    if (out_hash == JO_NULL || buffer > 1u) return (false);

    // Вызывать только когда VDP1 не рисует (golden/QA): чтение идёт
    // напрямую из VDP1 framebuffer без синхронизации с VDP1.

    const volatile uint16_t* fb =
        (const volatile uint16_t*)(JO_VDP1_FB + (uint32_t)buffer * RPE_VDP1_FB_PLANE_BYTES);
    uint32_t hash = 2166136261u;
    uint32_t x;
    uint32_t y;
    for (y = 0u; y < 224u; ++y)
    {
        const volatile uint16_t* line =
            (const volatile uint16_t*)((const volatile uint8_t*)fb + y * RPE_VDP1_FB_LINE_BYTES);
        for (x = 0u; x < 320u; ++x)
        {
            const uint16_t word = line[x];
            hash ^= (uint32_t)(word & 0xFFu);
            hash *= 16777619u;
            hash ^= (uint32_t)((word >> 8) & 0xFFu);
            hash *= 16777619u;
        }
    }
    *out_hash = hash;
    return (true);
}

uint8_t rpe_saturn_vdp1_displayed_buffer(void)
{
    // FBCR bit 0 (FBS): текущий framebuffer в auto-change режиме.
    return (uint8_t)(JO_VDP1_FBCR & 1u);
}

uint32_t rpe_saturn_vdp1_added_commands(void)
{
    return (s_vdp1_added_commands);
}


// ---------------------------------------------------------------------------
// VDP2: NBG1 8-bit bitmap background (S29–30)
// ---------------------------------------------------------------------------

static bool s_vdp2_background = false;

bool rpe_saturn_vdp2_init(uint8_t palette_id)
{
    if (!s_core_inited) return (false);
    if (palette_id < 1u || palette_id > 7u) return (false);
    if (!jo_vdp2_set_nbg1_8bits_bitmap((int)palette_id)) return (false);
    // VDP2 text (NBG0) выше VDP1, чтобы диагностическая строка читалась;
    // VDP1 выше NBG1; остальные слои — ниже.
    jo_core_set_screens_order(JO_NBG0_SCREEN, JO_SPRITE_SCREEN, JO_NBG1_SCREEN,
                              JO_RBG0_SCREEN, JO_NBG2_SCREEN, JO_NBG3_SCREEN);
    s_vdp2_background = true;
    return (true);
}

void rpe_saturn_vdp2_shutdown(void)
{
    if (!s_core_inited) return;
    jo_set_displayed_screens(JO_NBG0_SCREEN | JO_SPRITE_SCREEN);
    s_vdp2_background = false;
}

void rpe_saturn_vdp2_set_palette(uint8_t palette_id, const uint16_t* colors)
{
    if (!s_core_inited || colors == JO_NULL) return;
    if (palette_id < 1u || palette_id > 7u) return;

    volatile uint16_t* cram = (volatile uint16_t*)JO_VDP2_CRAM;
    const uint32_t base = (uint32_t)palette_id * 256u;
    uint32_t i;
    for (i = 0u; i < 256u; ++i)
        cram[base + i] = (uint16_t)(colors[i] | 0x8000u);
}

bool rpe_saturn_vdp2_upload_background(const uint8_t* pixels, uint16_t width, uint16_t height)
{
    if (!s_vdp2_background || pixels == JO_NULL) return (false);
    if (width == 0u || height == 0u || (width & 7u) != 0u || (height & 7u) != 0u)
        return (false);

    const int bitmap_width = jo_vdp2_get_nbg1_8bits_bitmap_width();
    const int bitmap_height = jo_vdp2_get_nbg1_8bits_bitmap_height();
    if (bitmap_width <= 0 || bitmap_height <= 0) return (false);

    jo_img_8bits img;
    img.width = width;
    img.height = height;
    img.data = (unsigned char*)pixels;

    int y;
    for (y = 0; y < bitmap_height; y += height)
    {
        int x;
        for (x = 0; x < bitmap_width; x += width)
            jo_vdp2_draw_bitmap_nbg1_8bits_image(&img, x, y);
    }
    return (true);
}

void rpe_saturn_vdp2_set_scroll(int16_t x, int16_t y)
{
    if (!s_vdp2_background) return;
    jo_vdp2_move_nbg1((int)x, (int)y);
}

bool rpe_saturn_vdp2_available(void)
{
    return (s_vdp2_background);
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
