// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/fake_n64_render_bridge.h
//
// Счётчики host-двойника render-моста N64 (S43–44).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t rpe_n64_fake_render_transform_calls(void);
uint32_t rpe_n64_fake_render_wait_calls(void);
uint32_t rpe_n64_fake_render_upload_calls(void);
uint32_t rpe_n64_fake_render_triangle_calls(void);
uint32_t rpe_n64_fake_render_overlay_calls(void);
uint32_t rpe_n64_fake_render_last_strip_sx(void);
uint32_t rpe_n64_fake_render_last_strip_sw(void);
uint32_t rpe_n64_fake_render_last_upload_bytes(void);
bool rpe_n64_fake_render_last_textured(void);
void rpe_n64_fake_render_set_tnl_available(bool available);
void rpe_n64_fake_render_reset(void);

#ifdef __cplusplus
}
#endif
