// SPDX-License-Identifier: MIT
// RetroPolyEngine SDK consumer probe (S33–34).
//
// Проверяет, что публичный umbrella компилируется, линкуется и что версия
// доступна без дерева исходников движка.
#include <retro_engine/retro_engine.h>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--version") == 0) {
        std::printf("RetroPolyEngine %s (%s) sdk_probe\n", RETRO_ENGINE_VERSION_STRING,
                    RETRO_ENGINE_PLATFORM_NAME);
        return 0;
    }

    // Дымовая проверка fixed-point ядра (линковка/инлайн-контракты).
    const fixed_t two = fixed_from_int(2);
    const fixed_t three = fixed_from_int(3);
    if (fixed_to_int(fixed_mul(two, three)) != 6) {
        std::fprintf(stderr, "sdk_probe: fixed-point sanity failed\n");
        return 1;
    }

    std::printf("RetroPolyEngine SDK %s (%s) OK\n", RETRO_ENGINE_VERSION_STRING,
                RETRO_ENGINE_PLATFORM_NAME);
    return 0;
}
