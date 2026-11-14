// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/gui/screenshot.cpp
#include "screenshot.h"

#include <vector>

#include <glad/gl.h>

#include "fs_util.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace rpe {
namespace ui {

bool captureFramebufferPng(const std::string& path, int width, int height, std::string* error)
{
    if (width <= 0 || height <= 0) {
        if (error != nullptr) *error = "invalid framebuffer size";
        return false;
    }
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) *
                                      4u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) {
        if (error != nullptr) *error = "glReadPixels failed";
        return false;
    }
    const std::string parent = parentPath(path);
    if (!parent.empty() && !createDirectories(parent, error)) return false;
    stbi_flip_vertically_on_write(1);
    const int written = stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4);
    if (written == 0) {
        if (error != nullptr) *error = "stbi_write_png failed: " + path;
        return false;
    }
    return true;
}

} // namespace ui
} // namespace rpe
