// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/n64/fake_n64_bridge.h
//
// Host-test doubles для N64 C-моста (S39–42): lifecycle/тики, joypad-снимки,
// in-memory DFS, render frame и RDRAM size. Позволяют проверять HAL без
// libdragon/toolchain.
#pragma once

#include <stdint.h>

#include "hal/n64/n64_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- lifecycle -------------------------------------------------------------
extern int rpe_n64_fake_init_calls;
extern int rpe_n64_fake_shutdown_calls;
extern int rpe_n64_fake_sleep_calls;
extern uint32_t rpe_n64_fake_ticks;
extern int rpe_n64_fake_init_result;
extern int rpe_n64_fake_ready;
extern uint32_t rpe_n64_fake_rdram_bytes;

// --- input -----------------------------------------------------------------
extern int rpe_n64_fake_input_poll_calls;
extern RpeN64PadState rpe_n64_fake_pads[4];

// --- FS --------------------------------------------------------------------
extern int rpe_n64_fake_fs_ready;
extern int rpe_n64_fake_fs_init_result;

// --- render ----------------------------------------------------------------
extern int rpe_n64_fake_render_available;
extern int rpe_n64_fake_frame_begin_calls;
extern int rpe_n64_fake_frame_end_calls;
extern int rpe_n64_fake_frame_clear_calls;
extern int rpe_n64_fake_text_calls;
extern int rpe_n64_fake_frame_begin_result;
extern int rpe_n64_fake_frame_active;

/** Полный сброс fake-состояния (включая in-memory DFS). */
void rpe_n64_fake_reset(void);

/**
 * Добавить файл в fake DFS (данные копируются).
 *
 * @param reported_size < 0 — реальный размер; ≥ 0 — «лживый» размер
 *                      (truncated/oversize тесты).
 */
void rpe_n64_fake_add_file(const char* name, const uint8_t* data, uint32_t size,
                           int32_t reported_size);

/** Удалить файл из fake DFS. */
void rpe_n64_fake_remove_file(const char* name);

/** Заставить rpe_n64_fs_read() вернуть ошибку для файла. */
void rpe_n64_fake_fail_read(const char* name, int fail);

/** @return Число зарегистрированных fake-файлов. */
uint32_t rpe_n64_fake_file_count(void);

#ifdef __cplusplus
}
#endif
