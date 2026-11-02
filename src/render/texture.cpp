// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/texture.cpp
//
// Пул текстур ядра (контракты — src/render/texture.h). Статические
// области, никакой динамики: отказы — явные (хэндл 0), память не
// расширяется (Failure policy §1: allocation overflow => status + код).
//
// Стратегия занятости: first-fit слот; арена — bump-выделение без
// дефрагментации (freed-байты повторно доступны только после
// render_texture_pool_reset) — детерминизм и предсказуемость MVP.
// Палитры — отдельная статическая область 256×u16 на слот (512 B × 16).
#include "render/texture.h"

#include <string.h>

/** Запись пула текстур (хэндл = index + 1). */
typedef struct TexturePoolEntry
{
    bool occupied; /**< Слот занят. */
    uint32_t width;
    uint32_t height;
    uint32_t arena_offset; /**< Смещение пикселей в арене CI8. */
} TexturePoolEntry;

/** Пул записей текстур (index 0..RENDER_TEXTURE_MAX-1). */
static TexturePoolEntry s_textures[RENDER_TEXTURE_MAX];

/** Арена пикселей CI8 (все живые текстуры целиком). */
static uint8_t s_texture_arena[RENDER_TEXTURE_POOL_BYTES];

/** Bump-курсор арены: занято байт. */
static uint32_t s_texture_arena_used = 0u;

/** Палитры текстур: 256 × u16 RGB555+STP на слот (порядок — как слота). */
static uint16_t s_texture_palettes[RENDER_TEXTURE_MAX][256];

void render_texture_pool_reset(void)
{
    memset(s_textures, 0, sizeof(s_textures));
    memset(s_texture_arena, 0, sizeof(s_texture_arena));
    memset(s_texture_palettes, 0, sizeof(s_texture_palettes));
    s_texture_arena_used = 0u;
}

TextureHandle render_texture_acquire(uint32_t w, uint32_t h, const uint8_t* pixels,
                                     const uint16_t* palette)
{
    // Валидация параметров ДО любой записи (Failure policy: malformed
    // input reject before dereference); переполнение арифметики — в uint64.
    if (w == 0u || h == 0u || w > RENDER_TEXTURE_MAX_DIM || h > RENDER_TEXTURE_MAX_DIM ||
        pixels == NULL || palette == NULL) {
        return RENDER_TEXTURE_INVALID;
    }

    const uint64_t bytes_u64 = (uint64_t)w * (uint64_t)h;
    if (bytes_u64 > 0xFFFFFFFFull ||
        (uint64_t)s_texture_arena_used + bytes_u64 > RENDER_TEXTURE_POOL_BYTES) {
        return RENDER_TEXTURE_INVALID; // Арена исчерпана — явный отказ.
    }

    // First-fit свободный слот.
    uint32_t slot = RENDER_TEXTURE_MAX;
    for (uint32_t i = 0u; i < RENDER_TEXTURE_MAX; ++i) {
        if (!s_textures[i].occupied) {
            slot = i;
            break;
        }
    }
    if (slot == RENDER_TEXTURE_MAX) {
        return RENDER_TEXTURE_INVALID; // Пул полон — явный отказ.
    }

    TexturePoolEntry* entry = &s_textures[slot];
    entry->occupied = true;
    entry->width = w;
    entry->height = h;
    entry->arena_offset = s_texture_arena_used;
    memcpy(s_texture_arena + s_texture_arena_used, pixels, (size_t)bytes_u64);
    s_texture_arena_used += (uint32_t)bytes_u64;
    memcpy(s_texture_palettes[slot], palette, sizeof(s_texture_palettes[slot]));

    return (TextureHandle)(slot + 1u);
}

void render_texture_release(TextureHandle handle)
{
    const uint32_t index = (uint32_t)handle;
    if (index == 0u || index > RENDER_TEXTURE_MAX) {
        return; // Невалидный хэндл — безопасный no-op (platform.h контракт).
    }
    s_textures[index - 1u].occupied = false; // Арена — bump (см. шапку файла).
}

uint32_t render_texture_width(TextureHandle handle)
{
    const uint32_t index = (uint32_t)handle;
    if (index == 0u || index > RENDER_TEXTURE_MAX || !s_textures[index - 1u].occupied) {
        return 0u;
    }
    return s_textures[index - 1u].width;
}

uint32_t render_texture_height(TextureHandle handle)
{
    const uint32_t index = (uint32_t)handle;
    if (index == 0u || index > RENDER_TEXTURE_MAX || !s_textures[index - 1u].occupied) {
        return 0u;
    }
    return s_textures[index - 1u].height;
}

const uint8_t* render_texture_pixels(TextureHandle handle)
{
    const uint32_t index = (uint32_t)handle;
    if (index == 0u || index > RENDER_TEXTURE_MAX || !s_textures[index - 1u].occupied) {
        return NULL;
    }
    return s_texture_arena + s_textures[index - 1u].arena_offset;
}

const uint16_t* render_texture_palette(TextureHandle handle)
{
    const uint32_t index = (uint32_t)handle;
    if (index == 0u || index > RENDER_TEXTURE_MAX || !s_textures[index - 1u].occupied) {
        return NULL;
    }
    return s_texture_palettes[index - 1u];
}
