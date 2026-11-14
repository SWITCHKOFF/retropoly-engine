// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_log_rate.h
//
// Детерминированный rate limit debug-канала N64 (ISViewer/USB).
//
// ISViewer/USB — медленный канал: boot-demo и stress-прогон не должны
// заливать его повторами. Окно фиксированное (1 с), лимит — N сообщений;
// «лишние» сообщения отбрасываются счётчиком. Модуль не зависит от
// libdragon и тестируется на хосте.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Лимит сообщений на окно (1 с). */
#define RPE_N64_LOG_RATE_LIMIT 120u

/** Длина окна в миллисекундах. */
#define RPE_N64_LOG_RATE_WINDOW_MS 1000u

/**
 * Разрешить сообщение в момент @p now_ms.
 *
 * Wrap-safe: окно считается беззнаковой разностью; первый вызов после
 * reset() всегда разрешён.
 *
 * @return true — сообщение можно писать; false — лимит окна исчерпан.
 */
bool rpe_n64_log_rate_allow(uint32_t now_ms);

/** Число отброшенных сообщений с последнего reset(). */
uint32_t rpe_n64_log_rate_dropped(void);

/** Сброс окна/счётчиков (Platform_Init/тесты). */
void rpe_n64_log_rate_reset(void);

#ifdef __cplusplus
}
#endif
