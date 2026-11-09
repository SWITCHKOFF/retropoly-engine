// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/asset_converter/main.cpp
//
// asset_converter — host-only конвертер ассетов (Спринты 9–10).
// CLI: `asset_converter <input> <output>`; формат определяется по расширению
// входа (.gltf → .mesh v1; .png/.jpg/.jpeg/.tga/.bmp → .tex v1).
// Никогда не компилируется MIPS-тулчейном (root CMakeLists: PLATFORM_PC).
//
// Коды возврата (контракт CLI, docs/ASSETS.md):
//   0 — успех;
//   1 — usage (аргументов != 2);
//   2 — неизвестное расширение входа;
//   3 — вход не открыт/не декодирован;
//   4 — структура входа повреждена (validate/атрибуты/индексы/типы);
//   5 — вход не поддержан MVP (dims/limits §8.2, непонятный bounds);
//   6 — не удалось записать выход;
//   7 — нехватка памяти хоста.
#include <stdint.h>
#include <string.h>

#include <stdio.h>
#include <string>

#include "converter_common.h"
#include "mesh_converter.h"
#include "texture_converter.h"

namespace {

/**
 * Регистр-независимое сравнение расширения входа (последней точки).
 *
 * @param path Путь файла.
 * @param extension Расширение в нижнем регистре с точкой (например, «.png»).
 * @return true, если расширение совпадает.
 */
bool has_extension(const char* path, const char* extension)
{
    if (path == nullptr || extension == nullptr) return false;
    const std::string full(path);
    const size_t dot = full.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string tail = full.substr(dot);
    for (size_t i = 0; i < tail.size(); ++i) {
        const char c = tail[i];
        if (c >= 'A' && c <= 'Z') tail[i] = (char)(c - 'A' + 'a');
    }
    return tail == extension;
}

/** Диагностика конвертера → код возврата CLI (контракт выше). */
int exit_code_for(uint32_t diag)
{
    switch (diag) {
        case CONVERT_OK:
            return 0;
        case CONVERT_ERR_OPEN_INPUT:
            return 3;
        case CONVERT_ERR_PARSE:
            return 4;
        case CONVERT_ERR_UNSUPPORTED:
            return 5;
        case CONVERT_ERR_WRITE_OUTPUT:
            return 6;
        case CONVERT_ERR_ALLOCATION:
            return 7;
        default:
            return 7;
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::printf("usage: asset_converter <input> <output>\n");
        std::printf("  .gltf              → .mesh v1 (glTF-треугольный меш)\n");
        std::printf("  .png/.jpg/.tga/.bmp → .tex v1 (CI8 + палитра RGB555+STP)\n");
        return 1;
    }

    const char* input = argv[1];
    const char* output = argv[2];

    uint32_t diag = CONVERT_OK;
    bool handled = false;

    if (has_extension(input, ".gltf") || has_extension(input, ".glb")) {
        handled = convert_mesh(input, output, &diag);
    } else if (has_extension(input, ".png") || has_extension(input, ".jpg") ||
               has_extension(input, ".jpeg") || has_extension(input, ".tga") ||
               has_extension(input, ".bmp")) {
        handled = convert_texture(input, output, &diag);
    } else {
        std::printf("asset_converter: неизвестное расширение входа «%s»\n", input);
        return 2;
    }

    if (!handled) {
        std::printf("asset_converter: ошибка %u (input=%s output=%s)\n", (unsigned)diag, input,
                    output);
        return exit_code_for(diag);
    }

    std::printf("asset_converter: %s → %s (ok)\n", input, output);
    return 0;
}
