// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_hal_internal.h
//
// ВНУТРЕННИЕ связки HAL Saturn (не публичный API): состояние платформы для
// render-backend'а и инициализация CD-подсистемы. Заголовок host-совместим.
#pragma once

namespace saturn {

/** @return true между успешным Platform_Init() и Platform_Shutdown(). */
bool platform_ready(void);

/**
 * Подключение CD-источника (реализация — filesystem_saturn.cpp).
 *
 * @return true, если ISO9660 доступен (0.BIN найден).
 */
bool filesystem_init(void);

/** Отключение CD-источника (идемпотентно). */
void filesystem_shutdown(void);

}  // namespace saturn
