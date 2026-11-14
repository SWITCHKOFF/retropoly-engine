// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/gui/screenshot.h
#pragma once

#include <string>

namespace rpe {
namespace ui {

/**
 * Сохранить текущий OpenGL-фреймбуфер в PNG (для docs/скриншотов).
 *
 * @param path Выходной файл.
 * @param width Ширина фреймбуфера.
 * @param height Высота фреймбуфера.
 * @param error Текст ошибки.
 */
bool captureFramebufferPng(const std::string& path, int width, int height, std::string* error);

} // namespace ui
} // namespace rpe
