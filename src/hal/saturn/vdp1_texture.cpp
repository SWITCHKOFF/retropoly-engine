// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp1_texture.cpp
//
// Реализация реестра VDP1-текстур (контракты — заголовок).
#include "hal/saturn/vdp1_texture.h"

#include "core/log/log.h"
#include "hal/saturn/jo_saturn_bridge.h"
#include "hal/saturn/triangle_to_quad.h"

namespace saturn {

namespace {

Vdp1TextureRegistry s_registry;

}  // namespace

void Vdp1TextureRegistry::init()
{
    m_allocator.init();
    for (uint32_t i = 0u; i < VDP1_TEXTURE_MAX; ++i) {
        m_slots[i] = Vdp1Texture{};
    }
    for (uint32_t i = 0u; i < 8u; ++i) {
        m_clut_used[i] = false;
    }
    m_inited = true;
    m_height_clamped = 0u;
    m_diag = VDP1_TEX_OK;
}

void Vdp1TextureRegistry::shutdown()
{
    m_allocator.shutdown();
    for (uint32_t i = 0u; i < VDP1_TEXTURE_MAX; ++i) {
        m_slots[i] = Vdp1Texture{};
    }
    for (uint32_t i = 0u; i < 8u; ++i) {
        m_clut_used[i] = false;
    }
    m_inited = false;
    m_height_clamped = 0u;
    m_diag = VDP1_TEX_OK;
}

Vdp1Texture* Vdp1TextureRegistry::find_mutable(TextureHandle handle)
{
    for (uint32_t i = 0u; i < VDP1_TEXTURE_MAX; ++i) {
        if (m_slots[i].used && m_slots[i].handle == handle) {
            return &m_slots[i];
        }
    }
    return nullptr;
}

uint8_t Vdp1TextureRegistry::allocate_clut()
{
    for (uint8_t c = VDP1_CLUT_FIRST; c <= VDP1_CLUT_LAST; ++c) {
        if (!m_clut_used[c]) {
            m_clut_used[c] = true;
            return c;
        }
    }
    return 0u;
}

void Vdp1TextureRegistry::release_clut(uint8_t clut)
{
    if (clut >= VDP1_CLUT_FIRST && clut <= VDP1_CLUT_LAST) {
        m_clut_used[clut] = false;
    }
}

bool Vdp1TextureRegistry::create(TextureHandle handle, uint32_t width, uint32_t height,
                                 const uint8_t* pixels, const uint16_t* palette)
{
    if (!m_inited) {
        m_diag = VDP1_TEX_NOT_INITIALIZED;
        return false;
    }
    if (handle == RENDER_TEXTURE_INVALID || pixels == nullptr || palette == nullptr) {
        m_diag = VDP1_TEX_NULL_ARGUMENT;
        return false;
    }
    if (width < 8u || width > VDP1_TEXTURE_MAX_DIM || (width & 7u) != 0u || height < 1u ||
        height > VDP1_TEXTURE_MAX_DIM) {
        m_diag = VDP1_TEX_BAD_SIZE;
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal",
                    "vdp1 texture: недопустимый размер %ux%u (width кратна 8, max 256)",
                    static_cast<unsigned>(width), static_cast<unsigned>(height));
        return false;
    }
    if (find_mutable(handle) != nullptr) {
        m_diag = VDP1_TEX_DUPLICATE_HANDLE;
        return false;
    }

    Vdp1Texture* slot = nullptr;
    for (uint32_t i = 0u; i < VDP1_TEXTURE_MAX; ++i) {
        if (!m_slots[i].used) {
            slot = &m_slots[i];
            break;
        }
    }
    if (slot == nullptr) {
        m_diag = VDP1_TEX_POOL_FULL;
        return false;
    }

    const uint32_t source_height = height;
    const uint32_t pattern_height =
        (height > VDP1_PATTERN_MAX_HEIGHT) ? VDP1_PATTERN_MAX_HEIGHT : height;
    const uint32_t bytes = width * pattern_height;

    const uint32_t vram_offset = m_allocator.allocate(bytes);
    if (vram_offset == VDP1_TEXTURE_INVALID_OFFSET) {
        m_diag = VDP1_TEX_VRAM_OUT_OF_MEMORY;
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1 texture: VRAM overflow (%u bytes)",
                    static_cast<unsigned>(bytes));
        return false;
    }

    const uint8_t clut = allocate_clut();
    if (clut == 0u) {
        (void)m_allocator.free(vram_offset);
        m_diag = VDP1_TEX_CLUT_OUT_OF_MEMORY;
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1 texture: CLUT slots exhausted");
        return false;
    }

    // Аллокатор выдаёт смещение относительно VDP1_TEXTURE_BASE; мост
    // принимает абсолютное смещение от базы VDP1 VRAM (ADR-010).
    const uint32_t vram_address = VDP1_TEXTURE_BASE + vram_offset;
    const bool uploaded = rpe_saturn_vdp1_upload_ci8(
        vram_address, pixels, static_cast<uint16_t>(width),
        static_cast<uint16_t>(pattern_height), static_cast<uint16_t>(width));
    if (!uploaded) {
        release_clut(clut);
        (void)m_allocator.free(vram_offset);
        m_diag = VDP1_TEX_UPLOAD_FAILED;
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "vdp1 texture: VRAM upload failed");
        return false;
    }

    rpe_saturn_vdp1_set_palette(clut, palette);

    *slot = Vdp1Texture{};
    slot->used = true;
    slot->handle = handle;
    slot->width = static_cast<uint16_t>(width);
    slot->height = static_cast<uint16_t>(pattern_height);
    slot->source_height = static_cast<uint16_t>(source_height);
    slot->stride = static_cast<uint16_t>(width);
    slot->vram_offset = vram_offset;
    slot->src_addr8 = static_cast<uint16_t>((VDP1_TEXTURE_BASE + vram_offset) / 8u);
    slot->clut = clut;

    if (source_height > VDP1_PATTERN_MAX_HEIGHT) {
        ++m_height_clamped;
        Log_Message(LOG_LEVEL_WARNING, "saturn_hal",
                    "vdp1 texture: height %u клампится до 255 (VDP1 limit)",
                    static_cast<unsigned>(source_height));
    }

    m_diag = VDP1_TEX_OK;
    return true;
}

bool Vdp1TextureRegistry::destroy(TextureHandle handle)
{
    if (!m_inited) {
        m_diag = VDP1_TEX_NOT_INITIALIZED;
        return false;
    }
    Vdp1Texture* slot = find_mutable(handle);
    if (slot == nullptr) {
        m_diag = VDP1_TEX_BAD_HANDLE;
        return false;
    }

    release_clut(slot->clut);
    (void)m_allocator.free(slot->vram_offset);
    *slot = Vdp1Texture{};
    m_diag = VDP1_TEX_OK;
    return true;
}

const Vdp1Texture* Vdp1TextureRegistry::find(TextureHandle handle) const
{
    for (uint32_t i = 0u; i < VDP1_TEXTURE_MAX; ++i) {
        if (m_slots[i].used && m_slots[i].handle == handle) {
            return &m_slots[i];
        }
    }
    return nullptr;
}

uint32_t Vdp1TextureRegistry::texture_count() const
{
    uint32_t count = 0u;
    for (uint32_t i = 0u; i < VDP1_TEXTURE_MAX; ++i) {
        if (m_slots[i].used) {
            ++count;
        }
    }
    return count;
}

uint32_t Vdp1TextureRegistry::clut_count() const
{
    uint32_t count = 0u;
    for (uint8_t c = VDP1_CLUT_FIRST; c <= VDP1_CLUT_LAST; ++c) {
        if (m_clut_used[c]) {
            ++count;
        }
    }
    return count;
}

const char* Vdp1TextureRegistry::diag_string(Vdp1TextureDiag diag)
{
    switch (diag) {
        case VDP1_TEX_OK:
            return "ok";
        case VDP1_TEX_NOT_INITIALIZED:
            return "not initialized";
        case VDP1_TEX_NULL_ARGUMENT:
            return "null argument";
        case VDP1_TEX_BAD_SIZE:
            return "bad size";
        case VDP1_TEX_BAD_HANDLE:
            return "bad handle";
        case VDP1_TEX_DUPLICATE_HANDLE:
            return "duplicate handle";
        case VDP1_TEX_POOL_FULL:
            return "texture pool full";
        case VDP1_TEX_VRAM_OUT_OF_MEMORY:
            return "vram out of memory";
        case VDP1_TEX_CLUT_OUT_OF_MEMORY:
            return "clut out of memory";
        case VDP1_TEX_UPLOAD_FAILED:
            return "upload failed";
        case VDP1_TEX_COUNT:
        default:
            return "unknown";
    }
}

Vdp1TextureRegistry& vdp1_textures(void)
{
    return s_registry;
}

}  // namespace saturn
