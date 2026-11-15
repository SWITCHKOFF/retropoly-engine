// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/n64_render_control.h
//
// Внутренние тестовые/диагностические ручки N64 render backend (S43–44):
// переключение CPU T&L fallback для ручного gate (сравнение кадра/hash).
#pragma once

#include <stdint.h>

/** Принудительный CPU-fallback T&L (true) или RSP-путь (false). */
void hal_render_backend_set_cpu_fallback(bool enabled);

/** true — текущий кадр считает T&L на CPU (fallback или нет RSP). */
bool hal_render_backend_cpu_fallback(void);

/** true — RSP T&L доступен и не отключён принудительно. */
bool hal_render_backend_rsp_active(void);

/** wall-время последнего rpe_n64_tnl_wait в end_frame (µs, benchmark). */
uint32_t hal_render_backend_last_tnl_us(void);

/** wall-время последней RDP-подачи группы в end_frame (µs, benchmark). */
uint32_t hal_render_backend_last_rdp_us(void);

/** Пиковый TMEM под текстурную полосу+TLUT (байты; S45–46 overlay). */
uint32_t hal_render_backend_tmem_bytes_used(void);

/** Текущее число pending-треугольников backend'а (диагностика турникета). */
uint32_t hal_render_backend_pending_triangles(void);

/** Число RDP-треугольников, нарисованных в предыдущем кадре (overlay). */
uint32_t hal_render_backend_last_draw_calls(void);

/** Текст overlay, рисуемый встроенным шрифтом в конце кадра (NULL/"" — нет). */
void hal_render_backend_set_text_overlay(const char* text);

/** Позиция overlay-текста в пикселях reference 320×240 (по умолчанию 8,8). */
void hal_render_backend_set_text_overlay_position(int32_t x, int32_t y);
