// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_bridge.c
//
// Единственный translation unit N64 HAL, включающий libdragon. Остальной
// HAL (platform/input/FS/memory/render), ядро и логгер libdragon не видят.
//
// Debug-канал (ADR-014): ISViewer (debug_init_emulog) + USB logging.
// ISViewer используется pinned эмулятором gopher64 как stdout; на железе
// доступен USB-лог (если подключён). Release-профиль N64 собирается без
// -DNDEBUG, поэтому debugf активен в boot-demo (docs/N64.md §5).
//
// S41–42: VI 320×240 16-bit, joypad (один poll/кадр), read-only DragonFS,
// RDRAM size и frame begin/end/clear через RDPQ. RSP/RDP-геометрия — S43–44.
#include "hal/n64/n64_bridge.h"

#include <libdragon.h>

#include <stdio.h>
#include <string.h>

#include "hal/n64/n64_log_rate.h"

#define RPE_N64_LOG_BUFFER 256

static bool s_core_ready = false;
static bool s_fs_ready = false;
static bool s_frame_active = false;
static surface_t* s_frame_surface = NULL;
static rdpq_font_t* s_debug_font = NULL;
static uint32_t s_log_dropped_reported = 0u;

static const char* n64_level_tag(int level)
{
    switch (level) {
        case 0:
            return "INFO";
        case 1:
            return "WARNING";
        case 2:
        default:
            return "ERROR";
    }
}

bool rpe_n64_core_init(void)
{
    if (s_core_ready) {
        return true;
    }

    // Важно: debug-канал до первого Log_Message. Оба вызова безопасны,
    // даже если USB не подключён (возвращают false).
    (void)debug_init_usblog();
    (void)debug_init_emulog();
    timer_init();

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    joypad_init();

    // Встроенный debug-шрифт 8×8 — только для boot-demo/overlay, не engine
    // font pipeline. Регистрация идемпотентна по жизненному циклу Init/Shutdown.
    s_debug_font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
    if (s_debug_font != NULL) {
        rdpq_text_register_font(1, s_debug_font);
    }

    rpe_n64_log_rate_reset();
    s_log_dropped_reported = 0u;
    s_frame_active = false;
    s_frame_surface = NULL;
    s_core_ready = true;
    return true;
}

void rpe_n64_core_shutdown(void)
{
    if (!s_core_ready) {
        return;
    }
    s_core_ready = false;

    if (s_frame_active) {
        rspq_wait();
        rdpq_detach_show();
        s_frame_active = false;
        s_frame_surface = NULL;
    }

    if (s_debug_font != NULL) {
        rdpq_text_unregister_font(1);
        rdpq_font_free(s_debug_font);
        s_debug_font = NULL;
    }

    joypad_close();
    // Дождаться RSP-очереди перед снятием overlay: повторный rdpq_init
    // после rdpq_close иначе видит pending DMA и уходит в RSP-crash.
    rspq_wait();
    rdpq_close();
    display_close();
}

bool rpe_n64_core_ready(void)
{
    return s_core_ready;
}

uint32_t rpe_n64_ticks_ms(void)
{
    // uint32-срез 64-битного счётчика: wrap-safe беззнаковая разность.
    return (uint32_t)get_ticks_ms();
}

void rpe_n64_sleep_ms(uint32_t ms)
{
    // wait_ms ограничен 91625 мс; режем длинные паузы на части.
    while (ms > 0u) {
        const uint32_t chunk = (ms > 90000u) ? 90000u : ms;
        wait_ms(chunk);
        ms -= chunk;
    }
}

uint32_t rpe_n64_rdram_size(void)
{
    if (!s_core_ready) {
        return 0u;
    }
    const int size = get_memory_size();
    return (size > 0) ? (uint32_t)size : 0u;
}

void rpe_n64_log_write_va(int level, const char* domain, const char* format, va_list* args)
{
    char buffer[RPE_N64_LOG_BUFFER];
    if (format == NULL || args == NULL) {
        return;
    }

    va_list copy;
    va_copy(copy, *args);
    const int written = vsnprintf(buffer, sizeof(buffer), format, copy);
    va_end(copy);
    if (written < 0) {
        return;
    }
    if (written >= (int)sizeof(buffer)) {
        buffer[sizeof(buffer) - 4] = '.';
        buffer[sizeof(buffer) - 3] = '.';
        buffer[sizeof(buffer) - 2] = '.';
        buffer[sizeof(buffer) - 1] = '\0';
    }

    if (!rpe_n64_log_rate_allow(rpe_n64_ticks_ms())) {
        return;
    }

    // При возобновлении окна сообщаем, сколько строк потеряно, — канал не
    // «молча» теряет диагностику.
    const uint32_t dropped = rpe_n64_log_rate_dropped();
    if (dropped != s_log_dropped_reported) {
        const uint32_t delta = dropped - s_log_dropped_reported;
        s_log_dropped_reported = dropped;
        debugf("[WARNING][log] rate limit: %lu messages dropped\n", (unsigned long)delta);
    }

    debugf("[%s][%s] %s\n", n64_level_tag(level),
           (domain != NULL && domain[0] != '\0') ? domain : "?", buffer);
}

// ---------------------------------------------------------------------------
// Input: ровно один joypad_poll на кадр; снимок портов.
// ---------------------------------------------------------------------------

void rpe_n64_input_poll(void)
{
    if (!s_core_ready) {
        return;
    }
    joypad_poll();
}

bool rpe_n64_input_read(uint32_t port, RpeN64PadState* out)
{
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!s_core_ready || port >= JOYPAD_PORT_COUNT) {
        return false;
    }

    const joypad_port_t joypad_port = (joypad_port_t)port;
    if (!joypad_is_connected(joypad_port) || joypad_get_style(joypad_port) == JOYPAD_STYLE_NONE) {
        return false;
    }

    const joypad_buttons_t buttons = joypad_get_buttons(joypad_port);
    const joypad_inputs_t inputs = joypad_get_inputs(joypad_port);

    uint32_t mask = 0u;
    if (buttons.a) {
        mask |= RPE_N64_BTN_A;
    }
    if (buttons.b) {
        mask |= RPE_N64_BTN_B;
    }
    if (buttons.z) {
        mask |= RPE_N64_BTN_Z;
    }
    if (buttons.start) {
        mask |= RPE_N64_BTN_START;
    }
    if (buttons.d_up) {
        mask |= RPE_N64_BTN_D_UP;
    }
    if (buttons.d_down) {
        mask |= RPE_N64_BTN_D_DOWN;
    }
    if (buttons.d_left) {
        mask |= RPE_N64_BTN_D_LEFT;
    }
    if (buttons.d_right) {
        mask |= RPE_N64_BTN_D_RIGHT;
    }
    if (buttons.l) {
        mask |= RPE_N64_BTN_L;
    }
    if (buttons.r) {
        mask |= RPE_N64_BTN_R;
    }
    if (buttons.c_up) {
        mask |= RPE_N64_BTN_C_UP;
    }
    if (buttons.c_down) {
        mask |= RPE_N64_BTN_C_DOWN;
    }
    if (buttons.c_left) {
        mask |= RPE_N64_BTN_C_LEFT;
    }
    if (buttons.c_right) {
        mask |= RPE_N64_BTN_C_RIGHT;
    }

    out->buttons = mask;
    out->stick_x = inputs.stick_x;
    out->stick_y = inputs.stick_y;
    out->cstick_x = inputs.cstick_x;
    out->cstick_y = inputs.cstick_y;
    out->analog_l = inputs.analog_l;
    out->analog_r = inputs.analog_r;
    out->connected = true;
    return true;
}

// ---------------------------------------------------------------------------
// Read-only DFS (DragonFS image, mkdfs + n64tool --align 16)
// ---------------------------------------------------------------------------

bool rpe_n64_fs_init(void)
{
    if (!s_core_ready) {
        return false;
    }
    if (s_fs_ready) {
        return true;
    }
    const int result = dfs_init(DFS_DEFAULT_LOCATION);
    s_fs_ready = (result == DFS_ESUCCESS);
    return s_fs_ready;
}

void rpe_n64_fs_shutdown(void)
{
    s_fs_ready = false;
}

bool rpe_n64_fs_ready(void)
{
    return s_core_ready && s_fs_ready;
}

int32_t rpe_n64_fs_size(const char* name)
{
    if (!rpe_n64_fs_ready() || name == NULL || name[0] == '\0') {
        return -1;
    }
    const int size = dfs_rom_size(name);
    return (size < 0) ? -1 : (int32_t)size;
}

int32_t rpe_n64_fs_read(const char* name, uint8_t* dst, uint32_t size)
{
    if (!rpe_n64_fs_ready() || name == NULL || dst == NULL || size == 0u) {
        return -1;
    }

    const int handle = dfs_open(name);
    if (handle < 0) {
        return -1;
    }

    uint32_t total = 0u;
    while (total < size) {
        const int read = dfs_read(dst + total, 1, (int)(size - total), (uint32_t)handle);
        if (read <= 0) {
            break;
        }
        total += (uint32_t)read;
    }
    (void)dfs_close((uint32_t)handle);

    return (total == size) ? (int32_t)total : -1;
}

// ---------------------------------------------------------------------------
// Render frame skeleton: display surface + RDPQ clear + debug text
// ---------------------------------------------------------------------------

bool rpe_n64_render_available(void)
{
    return s_core_ready;
}

bool rpe_n64_frame_begin(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_core_ready || s_frame_active) {
        return false;
    }
    s_frame_surface = display_get();
    if (s_frame_surface == NULL) {
        return false;
    }
    rdpq_attach(s_frame_surface, NULL);
    const color_t clear = {(uint8_t)r, (uint8_t)g, (uint8_t)b, 0u};
    rdpq_clear(clear);
    s_frame_active = true;
    return true;
}

void rpe_n64_frame_clear(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_frame_active) {
        return;
    }
    const color_t clear = {(uint8_t)r, (uint8_t)g, (uint8_t)b, 0u};
    rdpq_clear(clear);
}

void rpe_n64_frame_end(void)
{
    if (!s_frame_active) {
        return;
    }
    rdpq_detach_show();
    s_frame_active = false;
    s_frame_surface = NULL;
}

bool rpe_n64_frame_active(void)
{
    return s_frame_active;
}

bool rpe_n64_text_print(int x, int y, const char* text)
{
    if (!s_frame_active || text == NULL || s_debug_font == NULL) {
        return false;
    }
    (void)rdpq_text_print(NULL, 1, (float)x, (float)y, text);
    return true;
}
