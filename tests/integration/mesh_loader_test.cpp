// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/integration/mesh_loader_test.cpp
//
// Интеграционные тесты безопасного загрузчика .mesh v1.
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §8.1/§8.3
//     (формат + loader contract), §1.2.7 (обязательные malformed-кейсы);
//   - docs/plans/sprint_09_10.md, неделя 20 (кейсы: усечённый заголовок;
//     переполненные счётчики; индексы вне диапазона; «persistent arena
//     usage НЕ изменился при фейле»).
//
// Валидные .mesh-байты собираются в тесте побайтовым писателем (независимая
// эталонная модель §8.1); отдельные тесты читают РЕАЛЬНЫЙ артефакт пайплайна
// assets/build/cube.mesh (вход: RPE_TEST_SOURCE_DIR). Каждый отказ
// сопровождается проверкой, что использование арены НЕ изменилось (контракт
// §8.3/§1.2.7), а каждый успех — что арене выросла на развёртку результата.
#include <gtest/gtest.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "assets/assets.h"

// ---------------------------------------------------------------------------
// Тестовая инфраструктура: сборка .mesh-байтов побайтовым писателем
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

/** Арена фиксированного размера поверх static storage (§5.2: буфер не на стеке). */
struct ArenaFixture
{
    uint8_t buffer[64 * 1024];
    LinearAllocator alloc;

    ArenaFixture() : buffer{}, alloc(buffer, sizeof(buffer)) {}
};

/**
 * Сборщик .mesh v1: каркасный куб ±1 (8 вершин, 12 треугольников) с
 * независимой от конвертера побайтовой записью; методы-правки создают
 * malformed-варианты (плохой magic/version, дикие счётчики, усечение).
 */
class MeshFileBuilder {
public:
    MeshFileBuilder() { add_default_cube(); }

    /** Заголовок произвольными полями (перезаписывает первые 24 байта). */
    MeshFileBuilder& with_header(uint32_t magic, uint32_t version, uint32_t vertex_count,
                                 uint32_t index_count, uint32_t texture_id,
                                 int32_t bounding_sphere_radius)
    {
        uint8_t* h = m_bytes.data();
        assets_write_le_u32(h + 0u, magic);
        assets_write_le_u32(h + 4u, version);
        assets_write_le_u32(h + 8u, vertex_count);
        assets_write_le_u32(h + 12u, index_count);
        assets_write_le_u32(h + 16u, texture_id);
        assets_write_le_u32(h + 20u, (uint32_t)bounding_sphere_radius);
        return *this;
    }

    /** Усечение файла до size байт (для усечённых/переполненных payload'ов). */
    MeshFileBuilder& truncate_to(size_t size)
    {
        if (m_bytes.size() > size) m_bytes.resize(size);
        return *this;
    }

    const std::vector<uint8_t>& bytes() const { return m_bytes; }

    uint32_t vertex_count() const { return 8u; }
    uint32_t index_count() const { return 12u; }

    /** Радиус ограничивающей сферы куба ±1 в 16.16: floor(sqrt(3)·2^16) = 113512. */
    static int32_t default_radius() { return 113512; }

    /** Сериализованный объём каркасного файла (байт). */
    static size_t default_encoded_size()
    {
        return ASSETS_MESH_HEADER_BYTES + 8u * ASSETS_MESH_VERTEX_BYTES +
               12u * ASSETS_MESH_INDEX_BYTES;
    }

private:
    /**
     * Каркас: 24-байтовый заголовок + 8 вершин куба ±1 + 12 треугольников
     * (12 записей по 6 байт). Нормали граней ±127 (контракт i8-сатурации),
     * UV 0, цвет вершины (255,200,100,255).
     */
    void add_default_cube()
    {
        const int32_t one = 65536; // fixed 16.16: 1.0
        struct CubeCorner
        {
            int32_t x, y, z;
            int8_t nx, ny, nz;
        };
        const CubeCorner corners[8] = {
            {-one, -one, -one, -127, -127, -127}, {one, -one, -one, +127, -127, -127},
            {-one, one, -one, -127, +127, -127},  {one, one, -one, +127, +127, -127},
            {-one, -one, one, -127, -127, +127},  {one, -one, one, +127, -127, +127},
            {-one, one, one, -127, +127, +127},   {one, one, one, +127, +127, +127},
        };

        append_le_u32(&m_bytes, ASSETS_MESH_MAGIC);
        append_le_u32(&m_bytes, ASSETS_MESH_VERSION);
        append_le_u32(&m_bytes, 8u);  // vertex_count
        append_le_u32(&m_bytes, 12u); // index_count (12 треугольников, кратно 3)
        append_le_u32(&m_bytes, 5u);  // texture_id
        append_le_u32(&m_bytes, (uint32_t)default_radius());

        for (uint32_t i = 0; i < 8u; ++i) {
            append_le_u32(&m_bytes, (uint32_t)corners[i].x);
            append_le_u32(&m_bytes, (uint32_t)corners[i].y);
            append_le_u32(&m_bytes, (uint32_t)corners[i].z);
            m_bytes.push_back((uint8_t)corners[i].nx);
            m_bytes.push_back((uint8_t)corners[i].ny);
            m_bytes.push_back((uint8_t)corners[i].nz);
            m_bytes.push_back(0u);   // u
            m_bytes.push_back(0u);   // v
            m_bytes.push_back(255u); // r
            m_bytes.push_back(200u); // g
            m_bytes.push_back(100u); // b
            m_bytes.push_back(255u); // a
        }

        // 12 треугольников куба: CCW наружу (right-handed, §Cull «CCW фронт»);
        // cross(b−a, c−a)·n > 0 для каждой грани — проверено отручно.
        static const uint16_t triangles[12][3] = {
            {0, 4, 6}, {0, 6, 2}, // −X и его верхняя половина
            {1, 3, 7}, {1, 7, 5}, // +X
            {0, 1, 5}, {0, 5, 4}, // −Y
            {2, 7, 3}, {2, 6, 7}, // +Y
            {0, 2, 3}, {0, 3, 1}, // −Z
            {4, 5, 7}, {4, 7, 6}, // +Z
        };
        for (uint32_t t = 0; t < 12u; ++t) {
            append_le_u16(&m_bytes, triangles[t][0]);
            append_le_u16(&m_bytes, triangles[t][1]);
            append_le_u16(&m_bytes, triangles[t][2]);
        }
    }

    std::vector<uint8_t> m_bytes;
};

/** Успешная загрузка + проверка, что арене выросла от результата. */
void expect_load_ok(const std::vector<uint8_t>& bytes, ArenaFixture& arena, MeshAsset** out)
{
    const size_t used_before = arena.alloc.used();
    uint32_t err = UINT32_MAX;
    ASSERT_TRUE(mesh_validate_and_load(bytes.data(), bytes.size(), &arena.alloc, out, &err))
        << "diag=" << err;
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_OK);
    EXPECT_GT(arena.alloc.used(), used_before);
}

/** Вариант с явным размером среза: нужен, когда data() != nullptr, а size == 0. */
void expect_load_fail_arena_unchanged(const uint8_t* data, size_t size, ArenaFixture& arena,
                                      uint32_t expected_diag)
{
    const size_t used_before = arena.alloc.used();
    MeshAsset* mesh = (MeshAsset*)0x1; // маркер «не переписан»
    uint32_t err = UINT32_MAX;
    EXPECT_FALSE(mesh_validate_and_load(data, size, &arena.alloc, &mesh, &err));
    EXPECT_EQ(err, expected_diag);
    EXPECT_EQ(arena.alloc.used(), used_before) << "failed load расходовал арену!";
    EXPECT_EQ(mesh, (MeshAsset*)0x1); // out не перезаписан при фейле
}

/** Отказ + диагностический код + проверка, что арена НЕ изменилась (§8.3). */
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
// Успешные кейсы: обратная самопроверка читателем (§8.1 поля)
// ---------------------------------------------------------------------------

TEST(MeshLoader, ValidCubeRoundTripFields)
{
    ArenaFixture arena;
    MeshAsset* mesh = nullptr;
    expect_load_ok(MeshFileBuilder().bytes(), arena, &mesh);

    EXPECT_EQ(mesh->vertex_count, 8u);
    EXPECT_EQ(mesh->index_count, 12u);
    EXPECT_EQ(mesh->texture_id, 5u);
    EXPECT_EQ(mesh->bounding_sphere_radius, MeshFileBuilder::default_radius());

    // Вершина 0: (−1,−1,−1), нормаль (−127,−127,−127), цвет (255,200,100,255).
    EXPECT_EQ(mesh->vertices[0].x, -65536);
    EXPECT_EQ(mesh->vertices[0].y, -65536);
    EXPECT_EQ(mesh->vertices[0].z, -65536);
    EXPECT_EQ(mesh->vertices[0].nx, -127);
    EXPECT_EQ(mesh->vertices[0].ny, -127);
    EXPECT_EQ(mesh->vertices[0].nz, -127);
    EXPECT_EQ(mesh->vertices[0].u, 0u);
    EXPECT_EQ(mesh->vertices[0].v, 0u);
    EXPECT_EQ(mesh->vertices[0].r, 255u);
    EXPECT_EQ(mesh->vertices[0].g, 200u);
    EXPECT_EQ(mesh->vertices[0].b, 100u);
    EXPECT_EQ(mesh->vertices[0].a, 255u);

    // Вершина 7: (+1,+1,+1), нормаль (+127,+127,+127).
    EXPECT_EQ(mesh->vertices[7].x, 65536);
    EXPECT_EQ(mesh->vertices[7].y, 65536);
    EXPECT_EQ(mesh->vertices[7].z, 65536);
    EXPECT_EQ(mesh->vertices[7].nx, 127);
    EXPECT_EQ(mesh->vertices[7].ny, 127);
    EXPECT_EQ(mesh->vertices[7].nz, 127);

    // Индексные записи: треугольник 0 = (0,4,6); валидность всех индексов.
    EXPECT_EQ(mesh->indices[0].v0, 0u);
    EXPECT_EQ(mesh->indices[0].v1, 4u);
    EXPECT_EQ(mesh->indices[0].v2, 6u);
    for (uint32_t i = 0; i < mesh->index_count; ++i) {
        EXPECT_LT(mesh->indices[i].v0, mesh->vertex_count);
        EXPECT_LT(mesh->indices[i].v1, mesh->vertex_count);
        EXPECT_LT(mesh->indices[i].v2, mesh->vertex_count);
    }
}

TEST(MeshLoader, ArenaUsageBoundedByRuntimeStructs)
{
    ArenaFixture arena;
    const size_t used_before = arena.alloc.used();
    MeshAsset* mesh = nullptr;
    expect_load_ok(MeshFileBuilder().bytes(), arena, &mesh);

    const size_t encoded = MeshFileBuilder::default_encoded_size();
    // Развёртка (host-структуры) ≥ сериализованного объёма…
    EXPECT_GT(arena.alloc.used() - used_before, (size_t)encoded - ASSETS_MESH_HEADER_BYTES);
    // …и уложилась в ожидание арены (никаких скрытых файлов/буферов нет).
    EXPECT_LE(arena.alloc.used() - used_before,
              8u * sizeof(Vertex) + 12u * sizeof(Triangle) + sizeof(MeshAsset) + 64u);
}

// ---------------------------------------------------------------------------
// Malformed-кейсы (§1.2.7 / sprint_09_10.md) — каждый с проверкой арены
// ---------------------------------------------------------------------------

TEST(MeshLoader, RejectsNullArguments)
{
    ArenaFixture arena;
    MeshFileBuilder builder;
    const std::vector<uint8_t>& bytes = builder.bytes();
    MeshAsset* mesh = nullptr;
    uint32_t err = UINT32_MAX;

    EXPECT_FALSE(mesh_validate_and_load(nullptr, bytes.size(), &arena.alloc, &mesh, &err));
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_NULL_ARGUMENT);
    EXPECT_FALSE(mesh_validate_and_load(bytes.data(), bytes.size(), nullptr, &mesh, &err));
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_NULL_ARGUMENT);
    EXPECT_FALSE(mesh_validate_and_load(bytes.data(), bytes.size(), &arena.alloc, nullptr, &err));
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_NULL_ARGUMENT);
    EXPECT_EQ(arena.alloc.used(), 0u);
}

TEST(MeshLoader, RejectsTruncatedHeader)
{
    ArenaFixture arena;
    // Все срезы длины 0..23 → TRUNCATED_HEADER. Probe-буфер держит data()
    // ненулевым даже при n = 0 (пустой vector дал бы NULL_ARGUMENT по контракту).
    const std::vector<uint8_t> probe(64u, 0u);
    for (size_t n = 0; n < ASSETS_MESH_HEADER_BYTES; ++n) {
        expect_load_fail_arena_unchanged(probe.data(), n, arena, ASSET_ERR_TRUNCATED_HEADER);
    }
}

TEST(MeshLoader, RejectsBadMagic)
{
    ArenaFixture arena;
    expect_load_fail_arena_unchanged(
        MeshFileBuilder().with_header(0x12345678u, ASSETS_MESH_VERSION, 8u, 12u, 5u, 1000).bytes(),
        arena, ASSET_ERR_BAD_MAGIC);
}

TEST(MeshLoader, RejectsBadVersion)
{
    ArenaFixture arena;
    expect_load_fail_arena_unchanged(
        MeshFileBuilder().with_header(ASSETS_MESH_MAGIC, 2u, 8u, 12u, 5u, 1000).bytes(), arena,
        ASSET_ERR_BAD_VERSION);
}

TEST(MeshLoader, RejectsZeroCounts)
{
    ArenaFixture arena;
    expect_load_fail_arena_unchanged(
        MeshFileBuilder()
            .with_header(ASSETS_MESH_MAGIC, ASSETS_MESH_VERSION, 0u, 12u, 5u, 1000)
            .bytes(),
        arena, ASSET_ERR_BAD_COUNTS);
    expect_load_fail_arena_unchanged(
        MeshFileBuilder()
            .with_header(ASSETS_MESH_MAGIC, ASSETS_MESH_VERSION, 8u, 0u, 5u, 1000)
            .bytes(),
        arena, ASSET_ERR_BAD_COUNTS);
}

TEST(MeshLoader, RejectsIndexCountNotMultipleOfThree)
{
    ArenaFixture arena;
    // index_count = 11 (не кратен 3; normativity «(кратно 3)» §8.1/§8.3).
    // Файл обрезается до 11 записей, чтобы payload-граница тоже была нелегитимной.
    MeshFileBuilder bad;
    bad.with_header(ASSETS_MESH_MAGIC, ASSETS_MESH_VERSION, 8u, 11u, 5u, 1000);
    bad.truncate_to(ASSETS_MESH_HEADER_BYTES + 8u * ASSETS_MESH_VERTEX_BYTES +
                    11u * ASSETS_MESH_INDEX_BYTES);
    expect_load_fail_arena_unchanged(bad.bytes(), arena, ASSET_ERR_BAD_COUNTS);
}

TEST(MeshLoader, RejectsOverflowSizedCounts)
{
    ArenaFixture arena;
    // Переполненные счётчики (UINT32_MAX): арифметика u64 выживает, payload
    // почти ~350 ГБ >> file_size → отказ ДО аллокации и ДО чтения payload'а.
    expect_load_fail_arena_unchanged(
        MeshFileBuilder()
            .with_header(ASSETS_MESH_MAGIC, ASSETS_MESH_VERSION, UINT32_MAX, UINT32_MAX, 5u, 1000)
            .bytes(),
        arena, ASSET_ERR_PAYLOAD_RANGE);
}

TEST(MeshLoader, RejectsPayloadOutsideFile)
{
    ArenaFixture arena;
    // Заголовок заявляет 8 вершин/12 треугольников; файл короче payload'а.
    MeshFileBuilder cut;
    cut.truncate_to(MeshFileBuilder().default_encoded_size() - 1u);
    EXPECT_EQ(cut.bytes().size(), MeshFileBuilder().default_encoded_size() - 1u);
    expect_load_fail_arena_unchanged(cut.bytes(), arena, ASSET_ERR_PAYLOAD_RANGE);
}

TEST(MeshLoader, RejectsOutOfRangeIndices)
{
    ArenaFixture arena;
    const size_t record_offset =
        ASSETS_MESH_HEADER_BYTES + 8u * ASSETS_MESH_VERTEX_BYTES + 7u * ASSETS_MESH_INDEX_BYTES;

    std::vector<uint8_t> bytes = MeshFileBuilder().bytes();
    assets_write_le_u16(bytes.data() + record_offset, 8u); // v0 == vertex_count → reject
    expect_load_fail_arena_unchanged(bytes, arena, ASSET_ERR_INDEX_RANGE);

    bytes = MeshFileBuilder().bytes();
    assets_write_le_u16(bytes.data() + record_offset + 2u, 100u); // v1 вне диапазона
    expect_load_fail_arena_unchanged(bytes, arena, ASSET_ERR_INDEX_RANGE);

    bytes = MeshFileBuilder().bytes();
    assets_write_le_u16(bytes.data() + record_offset + 4u, 65535u); // v2 вне диапазона
    expect_load_fail_arena_unchanged(bytes, arena, ASSET_ERR_INDEX_RANGE);
}

TEST(MeshLoader, RejectsWhenArenaTooSmallAfterValidation)
{
    // Арена 64 байта: валидация проходит (читает из data), аллокация откажется
    // → rollback → used() == 0 (проверка шага 7 §1.2.7).
    alignas(8) uint8_t buffer[64];
    LinearAllocator alloc(buffer, sizeof(buffer));
    MeshAsset* mesh = nullptr;
    uint32_t err = UINT32_MAX;
    EXPECT_FALSE(mesh_validate_and_load(MeshFileBuilder().bytes().data(),
                                        MeshFileBuilder().bytes().size(), &alloc, &mesh, &err));
    EXPECT_EQ(err, (uint32_t)ASSET_ERR_ALLOC_FAIL);
    EXPECT_EQ(alloc.used(), 0u);
}

// ---------------------------------------------------------------------------
// вход реального pipeline-артефакта (контракт недели 18: «куб → cube.mesh →
// обратная проверка полей (самопроверка читателем из теста)»)
// ---------------------------------------------------------------------------

TEST(MeshLoader, RealBlenderCubeAssetLoadsAndValidates)
{
    // Артефакт CMake-пайплайна: asset_converter assets/source/cube.gltf →
    // cube.mesh (Спринт 9–10, недели 18/20). Тест читает его на disk'е.
    const std::string path = std::string(RPE_TEST_SOURCE_DIR) + "/assets/build/cube.mesh";
    std::vector<uint8_t> bytes;
    ASSERT_TRUE(read_file_bytes(path, &bytes)) << "нет артефакта " << path;

    ArenaFixture arena;
    MeshAsset* mesh = nullptr;
    expect_load_ok(bytes, arena, &mesh);
    EXPECT_GT(mesh->vertex_count, 0u);
    EXPECT_EQ(mesh->index_count % 3u, 0u);
    // Куб Blender: 24 вершины (4 на грань из-за нормалей/UV), 12 треугольников.
    EXPECT_EQ(mesh->vertex_count, 24u);
    EXPECT_EQ(mesh->index_count, 12u);
    // Все индексы — вершины куба, радиус > 0 (сфера покрывает ±1 куб).
    for (uint32_t i = 0; i < mesh->index_count; ++i) {
        EXPECT_LT(mesh->indices[i].v0, mesh->vertex_count);
        EXPECT_LT(mesh->indices[i].v1, mesh->vertex_count);
        EXPECT_LT(mesh->indices[i].v2, mesh->vertex_count);
    }
    EXPECT_GT(mesh->bounding_sphere_radius, 0);
    // fixed 16.16: радиус куба ±1 = √3 → 1.732…; fixed_from_float усекает:
    // floor(1.7320508075688772 × 65536) = 113511 (в границах int32: без sat).
    EXPECT_EQ(mesh->bounding_sphere_radius, 113511);
    EXPECT_LT(mesh->bounding_sphere_radius, INT32_MAX);
}
