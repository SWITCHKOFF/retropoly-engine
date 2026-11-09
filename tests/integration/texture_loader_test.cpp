// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/integration/texture_loader_test.cpp
//
// Интеграционные тесты безопасного загрузчика .tex v1 (CI8 + RGB555+STP).
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §8.2 (формат),
//     §1.2.7 (malformed-кейсы), §1.2.5 (бит-layout палитры);
//   - docs/plans/sprint_09_10.md (недели 19–20: перекрытие payloads, невалидные
//     width/height, усечённый CI8, «арена не расходуется при фейле»).
//
// Валидные .tex-байты собираются побайтовым писателем с эталонным
// pack_rgb555_stp (§1.2.5); отдельные тесты читают РЕАЛЬНЫЙ артефакт
// assets/build/texture.tex и валидируют политику слота 0 конвертера. Каждый
// отказ — с проверкой, что использование арены не изменилось.
#include <gtest/gtest.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <stdio.h>
#include <string>
#include <vector>

#include "assets/assets.h"
#include "render/texture.h" /* pack_rgb555_stp — кросс-проверка §1.2.5 */

// ---------------------------------------------------------------------------
// Тестовая инфраструктура: побайтовый писатель .tex + эталонный pack
// ---------------------------------------------------------------------------

namespace {

/** Запись LE u32 в байтовый вектор (push_back: без uninitialized-copy-инлайна). */
void append_le_u32(std::vector<uint8_t>* out, uint32_t v)
{
    uint8_t tmp[4];
    assets_write_le_u32(tmp, v);
    for (const uint8_t b : tmp) {
        out->push_back(b);
    }
}

/** Запись LE u16 в байтовый вектор (push_back: без uninitialized-copy-инлайна). */
void append_le_u16(std::vector<uint8_t>* out, uint16_t v)
{
    uint8_t tmp[2];
    assets_write_le_u16(tmp, v);
    for (const uint8_t b : tmp) {
        out->push_back(b);
    }
}

/** Независимый эталон pack_rgb555_stp (§1.2.5; тест-векторы — внизу). */
uint16_t ref_pack_rgb555_stp(uint8_t r5, uint8_t g5, uint8_t b5, bool stp)
{
    return (uint16_t)((r5 & 0x1Fu) | ((g5 & 0x1Fu) << 5) | ((b5 & 0x1Fu) << 10) |
                      (stp ? 0x8000u : 0u));
}

/** Арена фиксированного размера поверх static storage (§5.2: не на стеке). */
struct ArenaFixture
{
    uint8_t buffer[128 * 1024];
    LinearAllocator alloc;

    ArenaFixture() : buffer{}, alloc(buffer, sizeof(buffer)) {}
};

/**
 * Сборщик .tex v1: каркас 256×256 (палитра-эталон + CI8-паттерн) записывается
 * побайтово, независимо от конвертера. Правки-методы строят malformed-варианты;
 * header_only() даёт файл из одного заголовка (дешёвые счётные кейсы).
 */
class TexFileBuilder {
public:
    TexFileBuilder() { add_default(256u, 256u, false); }

    /** Только заголовок (20 байт; без payload'ов). */
    TexFileBuilder& header_only()
    {
        m_bytes.clear();
        add_default(256u, 256u, true);
        return *this;
    }

    /** Пересборка с payload'ом под размеры (валид — сам выбор в тесте). */
    TexFileBuilder& rebuild(uint32_t w, uint32_t h)
    {
        m_bytes.clear();
        add_default(w, h, false);
        return *this;
    }

    /** Заголовок произвольными полями (перезаписывает первые 20 байт). */
    TexFileBuilder& with_header(uint32_t magic, uint32_t version, uint32_t width, uint32_t height,
                                uint32_t palette_offset, uint32_t pixel_offset)
    {
        uint8_t* hdr = m_bytes.data();
        assets_write_le_u32(hdr + 0u, magic);
        assets_write_le_u32(hdr + 4u, version);
        assets_write_le_u16(hdr + 8u, (uint16_t)width);
        assets_write_le_u16(hdr + 10u, (uint16_t)height);
        assets_write_le_u32(hdr + 12u, palette_offset);
        assets_write_le_u32(hdr + 16u, pixel_offset);
        return *this;
    }

    /** Слияние payload'ов: pixel_offset внутрь области палитры. */
    TexFileBuilder& overlap_payloads()
    {
        with_header(ASSETS_TEX_MAGIC, ASSETS_TEX_VERSION, 256u, 256u, ASSETS_TEX_HEADER_BYTES,
                    ASSETS_TEX_HEADER_BYTES + 200u);
        return *this;
    }

    /** Усечение файла до size байт. */
    TexFileBuilder& truncate_to(size_t size)
    {
        if (m_bytes.size() > size) m_bytes.resize(size);
        return *this;
    }

    const std::vector<uint8_t>& bytes() const { return m_bytes; }

    /** Сериализованный объём каркасного файла 256×256. */
    static size_t default_encoded_size()
    {
        return ASSETS_TEX_HEADER_BYTES + ASSETS_TEX_PALETTE_BYTES + (size_t)256u * 256u;
    }

private:
    /**
     * Каркас: 20-байтовый заголовок + палитра (256 × LE u16 — эталонный
     * RGB555+STP-пак) + CI8 (i%256). Слоты 0..3 — ручные значения, чтобы
     * проверить бит-layout по факту чтения (R/G/STP в не-stub видах, без
     * запрещённого «(i<<8)|i»).
     */
    void add_default(uint32_t w, uint32_t h, bool header_only)
    {
        const uint32_t palette_offset = ASSETS_TEX_HEADER_BYTES;
        const uint32_t pixel_offset = ASSETS_TEX_HEADER_BYTES + ASSETS_TEX_PALETTE_BYTES;

        append_le_u32(&m_bytes, ASSETS_TEX_MAGIC);
        append_le_u32(&m_bytes, ASSETS_TEX_VERSION);
        append_le_u16(&m_bytes, (uint16_t)w);
        append_le_u16(&m_bytes, (uint16_t)h);
        append_le_u32(&m_bytes, palette_offset);
        append_le_u32(&m_bytes, pixel_offset);
        if (header_only) return;

        append_le_u16(&m_bytes, ref_pack_rgb555_stp(0u, 0u, 0u, false));
        append_le_u16(&m_bytes, ref_pack_rgb555_stp(1u, 0u, 0u, false));
        append_le_u16(&m_bytes, ref_pack_rgb555_stp(0u, 31u, 0u, false));
        append_le_u16(&m_bytes, ref_pack_rgb555_stp(0u, 0u, 31u, true));
        for (uint32_t i = 4u; i < ASSETS_TEX_PALETTE_ENTRIES; ++i) {
            append_le_u16(&m_bytes, ref_pack_rgb555_stp((uint8_t)i, (uint8_t)(255u - i),
                                                        (uint8_t)((i * 7u) & 0xFFu), false));
        }

        for (uint32_t i = 0; i < w * h; ++i)
            m_bytes.push_back((uint8_t)(i % 256u));
    }

    std::vector<uint8_t> m_bytes;
};

/** Успешная загрузка + проверка, что арене выросла от результата. */
void expect_load_ok(const std::vector<uint8_t>& bytes, ArenaFixture& arena, TextureAsset** out)
{
    const size_t used_before = arena.alloc.used();
    uint32_t err = UINT32_MAX;
    ASSERT_TRUE(texture_validate_and_load(bytes.data(), bytes.size(), &arena.alloc, out, &err))
        << "diag=" << err;
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_OK);
    EXPECT_GT(arena.alloc.used(), used_before);
}

/** Вариант с явным размером среза: нужен, когда data() != nullptr, а size == 0. */
void expect_load_fail_arena_unchanged(const uint8_t* data, size_t size, ArenaFixture& arena,
                                      uint32_t expected_diag)
{
    const size_t used_before = arena.alloc.used();
    TextureAsset* tex = (TextureAsset*)0x1;
    uint32_t err = UINT32_MAX;
    EXPECT_FALSE(texture_validate_and_load(data, size, &arena.alloc, &tex, &err));
    EXPECT_EQ(err, expected_diag);
    EXPECT_EQ(arena.alloc.used(), used_before) << "failed load расходовал арену!";
    EXPECT_EQ(tex, (TextureAsset*)0x1) << "out перезаписан при фейле";
}

/** Отказ + код + проверка, что арена НЕ изменилась. */
void expect_load_fail_arena_unchanged(const std::vector<uint8_t>& bytes, ArenaFixture& arena,
                                      uint32_t expected_diag)
{
    expect_load_fail_arena_unchanged(bytes.data(), bytes.size(), arena, expected_diag);
}

/** Чтение файла целиком (артефакт пайплайна assets/build/). */
bool read_file_bytes(const std::string& path, std::vector<uint8_t>* out)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) return false;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    bool ok = size > 0;
    if (ok) {
        out->resize((size_t)size);
        ok = fread(out->data(), 1, out->size(), file) == out->size();
    }
    fclose(file);
    return ok;
}

} // namespace

// ---------------------------------------------------------------------------
// §1.2.5: бит-layout палитры — нормативные векторы + кросс-проверка движка
// ---------------------------------------------------------------------------

TEST(TexContract, RGB555BitLayoutNormativeVectors)
{
    EXPECT_EQ(ref_pack_rgb555_stp(31u, 0u, 0u, true), 0x801Fu);
    EXPECT_EQ(ref_pack_rgb555_stp(0u, 31u, 0u, true), 0x83E0u);
    EXPECT_EQ(ref_pack_rgb555_stp(0u, 0u, 31u, true), 0xFC00u);
}

TEST(TexContract, EngineAndRefPackingAgree)
{
    // Движковый pack (src/render/texture.h) и ref конвертера — один бит-layout.
    for (uint32_t r = 0; r < 32u; ++r) {
        for (uint32_t g = 0; g < 32u; ++g) {
            for (uint32_t b = 0; b < 32u; b += 31u) { // 2 значения b — достаточная выборка
                uint16_t a = ref_pack_rgb555_stp((uint8_t)r, (uint8_t)g, (uint8_t)b, false);
                uint16_t c = pack_rgb555_stp((uint8_t)r, (uint8_t)g, (uint8_t)b, false);
                ASSERT_EQ(a, c) << r << "," << g << "," << b;
                a = ref_pack_rgb555_stp((uint8_t)r, (uint8_t)g, (uint8_t)b, true);
                c = pack_rgb555_stp((uint8_t)r, (uint8_t)g, (uint8_t)b, true);
                ASSERT_EQ(a, c) << r << "," << g << "," << b;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Успешные кейсы: обратная проверка полей
// ---------------------------------------------------------------------------

TEST(TextureLoader, ValidTexRoundTripFields)
{
    ArenaFixture arena;
    TextureAsset* tex = nullptr;
    expect_load_ok(TexFileBuilder().bytes(), arena, &tex);

    EXPECT_EQ(tex->width, 256u);
    EXPECT_EQ(tex->height, 256u);

    // Палитра скопирована честно по слотам (0..3 — ручные векторы).
    EXPECT_EQ(tex->palette[0], ref_pack_rgb555_stp(0u, 0u, 0u, false));
    EXPECT_EQ(tex->palette[1], ref_pack_rgb555_stp(1u, 0u, 0u, false));
    EXPECT_EQ(tex->palette[2], ref_pack_rgb555_stp(0u, 31u, 0u, false));
    EXPECT_EQ(tex->palette[3], ref_pack_rgb555_stp(0u, 0u, 31u, true));
    EXPECT_NE(tex->palette[255], 0u);

    // CI8-паттерн i%256 в разных местах payload'а.
    for (uint32_t idx : {0u, 1u, 100u, 1000u, 65535u}) {
        EXPECT_EQ(tex->pixels[idx], (uint8_t)(idx % 256u));
    }
}

TEST(TextureLoader, ValidSmallestTex64x64)
{
    ArenaFixture arena;
    TextureAsset* tex = nullptr;
    expect_load_ok(TexFileBuilder().rebuild(64u, 64u).bytes(), arena, &tex);
    EXPECT_EQ(tex->width, 64u);
    EXPECT_EQ(tex->height, 64u);
}

// ---------------------------------------------------------------------------
// Malformed-кейсы (§1.2.7 / sprint_09_10.md) — каждый с проверкой арены
// ---------------------------------------------------------------------------

TEST(TextureLoader, RejectsNullArguments)
{
    ArenaFixture arena;
    TexFileBuilder builder;
    const std::vector<uint8_t>& bytes = builder.bytes();
    TextureAsset* tex = nullptr;
    uint32_t err = UINT32_MAX;

    EXPECT_FALSE(texture_validate_and_load(nullptr, bytes.size(), &arena.alloc, &tex, &err));
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_NULL_ARGUMENT);
    EXPECT_FALSE(texture_validate_and_load(bytes.data(), bytes.size(), nullptr, &tex, &err));
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_NULL_ARGUMENT);
    EXPECT_FALSE(
        texture_validate_and_load(bytes.data(), bytes.size(), &arena.alloc, nullptr, &err));
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_NULL_ARGUMENT);
    EXPECT_EQ(arena.alloc.used(), 0u);
}

TEST(TextureLoader, RejectsTruncatedHeader)
{
    ArenaFixture arena;
    // Срезы 0..23 → TRUNCATED_HEADER; probe держит data() ненулевым при n = 0.
    const std::vector<uint8_t> probe(64u, 0u);
    for (size_t n = 0; n < ASSETS_TEX_HEADER_BYTES; ++n) {
        expect_load_fail_arena_unchanged(probe.data(), n, arena, ASSET_ERR_TRUNCATED_HEADER);
    }
}

TEST(TextureLoader, RejectsBadMagic)
{
    ArenaFixture arena;
    expect_load_fail_arena_unchanged(
        TexFileBuilder()
            .with_header(0xDEADBEEFu, ASSETS_TEX_VERSION, 256u, 256u, 20u, 532u)
            .bytes(),
        arena, ASSET_ERR_BAD_MAGIC);
}

TEST(TextureLoader, RejectsBadVersion)
{
    ArenaFixture arena;
    expect_load_fail_arena_unchanged(
        TexFileBuilder().with_header(ASSETS_TEX_MAGIC, 2u, 256u, 256u, 20u, 532u).bytes(), arena,
        ASSET_ERR_BAD_VERSION);
}

TEST(TextureLoader, RejectsInvalidDimensions)
{
    ArenaFixture arena;
    // Норматив §8.2: {64,128,256}; нулевые/соседние/дикие значения — отказ.
    const uint32_t bad_dims[] = {0u, 1u, 32u, 63u, 65u, 100u, 127u, 257u, 65535u};
    for (uint32_t bad_w : bad_dims) {
        expect_load_fail_arena_unchanged(
            TexFileBuilder()
                .header_only()
                .with_header(ASSETS_TEX_MAGIC, ASSETS_TEX_VERSION, bad_w, 256u, 20u, 532u)
                .bytes(),
            arena, ASSET_ERR_BAD_COUNTS);
    }
    for (uint32_t bad_h : bad_dims) {
        expect_load_fail_arena_unchanged(
            TexFileBuilder()
                .header_only()
                .with_header(ASSETS_TEX_MAGIC, ASSETS_TEX_VERSION, 256u, bad_h, 20u, 532u)
                .bytes(),
            arena, ASSET_ERR_BAD_COUNTS);
    }
}

TEST(TextureLoader, RejectsOffsetsInsideHeader)
{
    ArenaFixture arena;
    // Payload внутри заголовка: palette_offset = 10 (< 20); pixel_offset = 0.
    expect_load_fail_arena_unchanged(
        TexFileBuilder()
            .header_only()
            .with_header(ASSETS_TEX_MAGIC, ASSETS_TEX_VERSION, 256u, 256u, 10u, 532u)
            .bytes(),
        arena, ASSET_ERR_PAYLOAD_RANGE);
    expect_load_fail_arena_unchanged(
        TexFileBuilder()
            .header_only()
            .with_header(ASSETS_TEX_MAGIC, ASSETS_TEX_VERSION, 256u, 256u, 20u, 0u)
            .bytes(),
        arena, ASSET_ERR_PAYLOAD_RANGE);
}

TEST(TextureLoader, RejectsOverlappingPayloads)
{
    ArenaFixture arena;
    // Полный каркас 256×256; pixel_offset (220) внутри палитры (20..531).
    expect_load_fail_arena_unchanged(TexFileBuilder().overlap_payloads().bytes(), arena,
                                     ASSET_ERR_PAYLOAD_RANGE);
}

TEST(TextureLoader, RejectsPayloadBeyondFile)
{
    ArenaFixture arena;
    // Header-only файл (20 байт): увод палитры в вечность — payload вне файла.
    expect_load_fail_arena_unchanged(
        TexFileBuilder()
            .header_only()
            .with_header(ASSETS_TEX_MAGIC, ASSETS_TEX_VERSION, 256u, 256u, 1000000u, 532u)
            .bytes(),
        arena, ASSET_ERR_PAYLOAD_RANGE);
    expect_load_fail_arena_unchanged(
        TexFileBuilder()
            .header_only()
            .with_header(ASSETS_TEX_MAGIC, ASSETS_TEX_VERSION, 256u, 256u, 20u, 1000000u)
            .bytes(),
        arena, ASSET_ERR_PAYLOAD_RANGE);

    // Полный каркас: pixel_offset уводит пиксели за конец файла.
    expect_load_fail_arena_unchanged(
        TexFileBuilder()
            .with_header(ASSETS_TEX_MAGIC, ASSETS_TEX_VERSION, 256u, 256u, 20u,
                         ASSETS_TEX_PALETTE_BYTES + 256u * 256u)
            .bytes(),
        arena, ASSET_ERR_PAYLOAD_RANGE);
}

TEST(TextureLoader, RejectsTruncatedCIPayload)
{
    ArenaFixture arena;
    // Отрезан последний пиксель (w*h == 65536 байт payload'а).
    TexFileBuilder cut;
    cut.truncate_to(TexFileBuilder::default_encoded_size() - 1u);
    expect_load_fail_arena_unchanged(cut.bytes(), arena, ASSET_ERR_PAYLOAD_RANGE);

    // Нет пикселей вовсе (только заголовок + палитра).
    TexFileBuilder no_pixels;
    no_pixels.truncate_to(ASSETS_TEX_HEADER_BYTES + ASSETS_TEX_PALETTE_BYTES);
    expect_load_fail_arena_unchanged(no_pixels.bytes(), arena, ASSET_ERR_PAYLOAD_RANGE);
}

TEST(TextureLoader, RejectsWhenArenaTooSmallAfterValidation)
{
    // Валидный 256×256: валидация руками из data пройдёт, аллокация на 64-байтной
    // арене откажется → rollback → used() == 0 (проверка шага 7 §1.2.7).
    alignas(8) uint8_t buffer[64];
    LinearAllocator alloc(buffer, sizeof(buffer));
    TextureAsset* tex = nullptr;
    uint32_t err = UINT32_MAX;
    EXPECT_FALSE(texture_validate_and_load(TexFileBuilder().bytes().data(),
                                           TexFileBuilder().bytes().size(), &alloc, &tex, &err));
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_ALLOC_FAIL);
    EXPECT_EQ(alloc.used(), 0u);
}

// ---------------------------------------------------------------------------
// Реальный pipeline-артефакт (неделя 19: PNG 256×256 → texture.tex →
// palette-валидация бит-layout) + политика слота 0 конвертера
// ---------------------------------------------------------------------------

TEST(TextureLoader, RealPipelineTextureLoadsAndValidates)
{
    const std::string path = std::string(RPE_TEST_SOURCE_DIR) + "/assets/build/texture.tex";
    std::vector<uint8_t> bytes;
    ASSERT_TRUE(read_file_bytes(path, &bytes)) << "нет артефакта " << path;

    ArenaFixture arena;
    TextureAsset* tex = nullptr;
    expect_load_ok(bytes, arena, &tex);

    EXPECT_EQ(tex->width, 256u);
    EXPECT_EQ(tex->height, 256u);

    // Политика слота 0 конвертера (фиксируется в ASSETS.md): reserved
    // «transparent black» — 0x0000 (RGB=0, STP=0), и CI8-индекс 0 не входит
    // ни в пиксели payload'а (квантование съезжает в слоты 1..255).
    EXPECT_EQ(tex->palette[0], 0x0000u);
    bool index_zero_present = false;
    for (uint32_t i = 0; i < tex->width * tex->height; ++i) {
        if (tex->pixels[i] == 0u) index_zero_present = true;
    }
    EXPECT_FALSE(index_zero_present);

    // Слоты 1..255: STP-бит зарезервированной политики — 0 (MVP прозрачность
    // выключена, §8.2/ASSETS.md), палитра не выродилась в 0 цветов.
    for (uint32_t i = 1u; i < ASSETS_TEX_PALETTE_ENTRIES; ++i) {
        EXPECT_EQ(tex->palette[i] & 0x8000u, 0u) << "STP включён в слоте " << i;
    }
    uint32_t nonzero_slots = 0u;
    for (uint32_t i = 1u; i < ASSETS_TEX_PALETTE_ENTRIES; ++i) {
        if (tex->palette[i] != 0u) ++nonzero_slots;
    }
    EXPECT_GE(nonzero_slots, 2u);   // квантование не обнулилось
    EXPECT_LT(nonzero_slots, 256u); // ≤ 255 полезных (слот 0 reserved)
}
