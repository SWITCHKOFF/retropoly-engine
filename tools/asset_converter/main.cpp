// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/asset_converter/main.cpp
//
// asset_converter — host-only конвертер ассетов (Спринты 9–10, N64 S45–46).
// CLI: `asset_converter [--platform pc|ps1|saturn|n64] <input> <output>`;
// формат определяется по расширению входа (.gltf/.glb → .mesh v1;
// .png/.jpg/.jpeg/.tga/.bmp → .tex v1).
// Никогда не компилируется MIPS-тулчейном (root CMakeLists: PLATFORM_PC).
//
// `--platform n64` — N64 runtime layout: текстура nearest-downsample 64×64
// до квантования, CI8 + 256-entry CLUT (RGBA5551 на runtime), тайлы/TMEM-
// полосы формируются загрузчиком. `.mesh`/`.tex` v1 не меняются.
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

typedef enum PlatformTarget
{
    TARGET_PC = 0,
    TARGET_PS1,
    TARGET_SATURN,
    TARGET_N64
} PlatformTarget;

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

bool parse_platform(const char* value, PlatformTarget* out)
{
    if (value == nullptr || out == nullptr) return false;
    if (strcmp(value, "pc") == 0) {
        *out = TARGET_PC;
        return true;
    }
    if (strcmp(value, "ps1") == 0) {
        *out = TARGET_PS1;
        return true;
    }
    if (strcmp(value, "saturn") == 0) {
        *out = TARGET_SATURN;
        return true;
    }
    if (strcmp(value, "n64") == 0) {
        *out = TARGET_N64;
        return true;
    }
    return false;
}

const char* platform_name(PlatformTarget platform)
{
    switch (platform) {
        case TARGET_PS1:
            return "ps1";
        case TARGET_SATURN:
            return "saturn";
        case TARGET_N64:
            return "n64";
        default:
            return "pc";
    }
}

}  // namespace

int main(int argc, char** argv)
{
    PlatformTarget platform = TARGET_PC;
    const char* positional[3] = {nullptr, nullptr, nullptr};
    uint32_t positional_count = 0u;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--platform") == 0) {
            if (i + 1 >= argc || !parse_platform(argv[++i], &platform)) {
                std::printf("asset_converter: --platform требует pc|ps1|saturn|n64\n");
                return 1;
            }
        } else if (strncmp(arg, "--platform=", 11) == 0) {
            if (!parse_platform(arg + 11, &platform)) {
                std::printf("asset_converter: --platform требует pc|ps1|saturn|n64\n");
                return 1;
            }
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            positional_count = 0u;
            break;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            std::printf("asset_converter: неизвестный флаг «%s»\n", arg);
            return 1;
        } else if (positional_count < 3u) {
            positional[positional_count++] = arg;
        } else {
            positional_count = 3u;
            break;
        }
    }

    if (positional_count != 2u) {
        std::printf("usage: asset_converter [--platform pc|ps1|saturn|n64] <input> <output>\n");
        std::printf("  .gltf/.glb          → .mesh v1 (glTF-треугольный меш)\n");
        std::printf("  .png/.jpg/.tga/.bmp → .tex v1 (CI8 + палитра RGB555+STP)\n");
        std::printf("  --platform n64      → .tex 64×64 CI8 + 256-entry CLUT (TMEM)\n");
        return 1;
    }

    const char* input = positional[0];
    const char* output = positional[1];

    uint32_t diag = CONVERT_OK;
    bool handled = false;

    if (has_extension(input, ".gltf") || has_extension(input, ".glb")) {
        handled = convert_mesh(input, output, &diag);
    } else if (has_extension(input, ".png") || has_extension(input, ".jpg") ||
               has_extension(input, ".jpeg") || has_extension(input, ".tga") ||
               has_extension(input, ".bmp")) {
        handled = (platform == TARGET_N64) ? convert_texture_n64(input, output, &diag)
                                           : convert_texture(input, output, &diag);
    } else {
        std::printf("asset_converter: неизвестное расширение входа «%s»\n", input);
        return 2;
    }

    if (!handled) {
        std::printf("asset_converter: ошибка %u (input=%s output=%s target=%s)\n", (unsigned)diag,
                    input, output, platform_name(platform));
        return exit_code_for(diag);
    }

    std::printf("asset_converter: %s → %s (ok, target=%s)\n", input, output,
                platform_name(platform));
    return 0;
}
