// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/visual/golden_protocol.cpp
//
// Реализация единого golden-протокола (см. golden_protocol.h — метод).
// История метода: docs/plans/sprint_07_08.md (недели 16), протокол v2 —
// fix(golden) 2f21e1d (S7–8), перенос в общий T.U. — Спринты 11–12.
#include "tests/visual/golden_protocol.h"

#include <gtest/gtest.h> // имя текущего теста (dump-имена)

#include "render/renderer.h" // Render_TestReadbackHash/Pixels

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>

// LSan-суппрессии системного GL-драйвера (см. hal_test.cpp — один T.U.
// на исполняемый файл переносит список; здесь — рендер-тестовый exe).
extern "C" const char* __lsan_default_suppressions(void)
{
    return "leak:libgallium\n"
           "leak:libGLX\n"
           "leak:libGL\n";
}

namespace golden {

bool display_available()
{
#if defined(__linux__)
    const char* x11 = getenv("DISPLAY");
    const char* wayland = getenv("WAYLAND_DISPLAY");
    return (x11 != nullptr && x11[0] != '\0') || (wayland != nullptr && wayland[0] != '\0');
#else
    return true;
#endif
}

uint16_t px_pack15(const uint8_t* px)
{
    const uint16_t r5 = px[0] >> 3;
    const uint16_t g5 = px[1] >> 3;
    const uint16_t b5 = px[2] >> 3;
    return (uint16_t)(r5 | (g5 << 5) | (b5 << 10));
}

namespace {

/// Канальный порог фолбэка golden v2: дельта любого канала не больше
/// 2 LSB (норматив 15-битной хэш-компрессии — теперь явный кап).
const uint32_t GOLDEN_MAX_LSB = 2u;

/// Путь эталона @p name: <root>/tests/visual/reference/<name>.bin
///
/// Корень — compile-time (канонический; CMake: RPE_TEST_SOURCE_DIR =
/// ${CMAKE_SOURCE_DIR}). getenv-fallback — только для ad-hoc прогонов вне
/// билд-кабинета (норматив комментария tests/CMakeLists.txt: **ошибка
/// инверсии** — эталоны не зависят от CWD запустившего процесса).
#ifndef RPE_TEST_SOURCE_DIR
#define RPE_TEST_SOURCE_DIR "."
#endif

const char* golden_path(const char* name)
{
    static char path[640];
    const char* root = RPE_TEST_SOURCE_DIR;
    snprintf(path, sizeof(path), "%s/tests/visual/reference/%s", root, name);
    return path;
}

/// Текущее имя теста (для имён dump-файлов): reference/<TestName>.dump.px.
const char* golden_test_name(void)
{
    const testing::TestInfo* info = testing::UnitTest::GetInstance()->current_test_info();
    return (info != nullptr) ? info->name() : "unknown-test";
}

/// BE-запись u32 (согласована с golden_check-контрактом big-endian хэша).
void rpe_fwrite_u32_be(FILE* f, uint32_t v)
{
    for (int shift = 24; shift >= 0; shift -= 8) {
        const uint8_t byte = (uint8_t)((v >> shift) & 0xFFu);
        fwrite(&byte, 1, 1, f);
    }
}

/// u16 BE-запись (px-партнёр эталона: 15-битные пиксели, тот же BE-контракт).
void rpe_fwrite_u16_be(FILE* f, uint16_t v)
{
    const uint8_t bytes[2] = {(uint8_t)(v >> 8), (uint8_t)(v & 0xFFu)};
    fwrite(bytes, 1, 2, f);
}

/// u32 BE-чтение (px-партнёр: заголовок w/h).
bool rpe_fread_u32_be(FILE* f, uint32_t* v)
{
    uint32_t acc = 0u;
    for (int i = 0; i < 4; ++i) {
        const int byte = fgetc(f);
        if (byte == EOF) {
            return false;
        }
        acc = (acc << 8) | (uint32_t)byte;
    }
    *v = acc;
    return true;
}

/// u16 BE-чтение (px-партнёр: пиксели).
bool rpe_fread_u16_be(FILE* f, uint16_t* v)
{
    uint16_t acc = 0u;
    for (int i = 0; i < 2; ++i) {
        const int byte = fgetc(f);
        if (byte == EOF) {
            return false;
        }
        acc = (uint16_t)((acc << 8) | (uint16_t)byte);
    }
    *v = acc;
    return true;
}

/// Readback ТЕКУЩЕГО снятого кадра + валидность (фолбэку нужен сырой
/// кадр ПОКА GL жив; после shutdown readback мёртв). Буфер кадра 320×240 RGBA8.
uint8_t s_capture_pixels[320u * 240u * 4u];
bool s_capture_pixels_valid = false;

} // namespace

uint64_t capture_readback()
{
    const uint64_t hash = Render_TestReadbackHash();
    if (hash != 0u) {
        s_capture_pixels_valid =
            Render_TestReadbackPixels(s_capture_pixels, sizeof(s_capture_pixels));
    }
    const char* dump = getenv("RENDER_GOLDEN_DUMP");
    if (hash != 0u && dump != NULL && dump[0] == '1') {
        char path[768];
        snprintf(path, sizeof(path), "%s.dump.px", golden_path(golden_test_name()));
        FILE* f = fopen(path, "wb");
        if (f == NULL) {
            printf("[golden] dump: файл не открыть: %s\n", path);
            return hash;
        }
        rpe_fwrite_u32_be(f, 320u);
        rpe_fwrite_u32_be(f, 240u);
        fwrite(s_capture_pixels, 1, sizeof(s_capture_pixels), f);
        printf("[golden] dump -> %s\n", path);
        fclose(f);
    }
    return hash;
}

bool check(const char* name, uint64_t hash, const uint32_t budget)
{
    const char* path = golden_path(name);

    // Rebase-режим: осознанная запись эталона (+ px-партнёр — кадр обязан
    // быть снят capture_readback, GL ещё жив).
    const char* rebase = getenv("RENDER_GOLDEN_REBASE");
    if (rebase != NULL && rebase[0] == '1') {
        FILE* f = fopen(path, "wb");
        if (f == NULL) {
            printf("[golden] rebase: не открыть %s\n", path);
            return false;
        }
        // Big-endian запись хэша (стабильна против endian-хоста).
        for (int shift = 56; shift >= 0; shift -= 8) {
            const uint8_t byte = (uint8_t)((hash >> shift) & 0xFFu);
            fwrite(&byte, 1, 1, f);
        }
        fclose(f);
        char px_path[704];
        snprintf(px_path, sizeof(px_path), "%s.px", path);
        FILE* pf = fopen(px_path, "wb");
        if (pf == NULL) {
            printf("[golden] rebase: px-партнёр не открыть %s\n", px_path);
            return false;
        }
        rpe_fwrite_u32_be(pf, 320u);
        rpe_fwrite_u32_be(pf, 240u);
        for (uint32_t i = 0u; i < 320u * 240u; ++i) {
            rpe_fwrite_u16_be(pf, px_pack15(&s_capture_pixels[i * 4u]));
        }
        fclose(pf);
        printf("[golden] rebase %s <- %016llx (+ px-партнёр)\n", path, (unsigned long long)hash);
        return true;
    }

    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        printf(
            "[golden] эталон отсутствует: %s (актуальный хэш %016llx) — "
            "запусти с RENDER_GOLDEN_REBASE=1 для записи\n",
            path, (unsigned long long)hash);
        return false;
    }
    uint64_t golden = 0u;
    for (int i = 0; i < 8; ++i) {
        const int byte = fgetc(f);
        if (byte == EOF) {
            fclose(f);
            return false;
        }
        golden = (golden << 8) | (uint64_t)(byte & 0xFFu);
    }
    fclose(f);
    if (golden == hash) {
        return true;
    }
    if (budget == 0u || !s_capture_pixels_valid) {
        printf("[golden] MISMATCH %s: эталон %016llx, фактический %016llx\n", path,
               (unsigned long long)golden, (unsigned long long)hash);
        return false;
    }

    // Фолбэк v2: счётчик отличных 15-битных px + кап каналов.
    char px_path[704];
    snprintf(px_path, sizeof(px_path), "%s.px", path);
    FILE* pf = fopen(px_path, "rb");
    if (pf == NULL) {
        printf("[golden] фолбэк: px-партнёр отсутствует: %s\n", px_path);
        return false;
    }
    uint32_t w = 0u;
    uint32_t h = 0u;
    bool px_ok = rpe_fread_u32_be(pf, &w) && rpe_fread_u32_be(pf, &h) && (w == 320u) && (h == 240u);
    uint32_t differ = 0u;
    uint32_t max_lsb = 0u;
    for (uint32_t i = 0u; px_ok && (i < 320u * 240u); ++i) {
        uint16_t expected = 0u;
        if (!rpe_fread_u16_be(pf, &expected)) {
            px_ok = false;
            break;
        }
        const uint16_t actual = px_pack15(&s_capture_pixels[i * 4u]);
        if (expected != actual) {
            const int dr = (int)((expected >> 10) & 31u) - (int)((actual >> 10) & 31u);
            const int dg = (int)((expected >> 5) & 31u) - (int)((actual >> 5) & 31u);
            const int db = (int)(expected & 31u) - (int)(actual & 31u);
            const uint32_t dgx = (uint32_t)abs(dr < -dr ? -dr : dr);
            const uint32_t max_pair = dgx > (uint32_t)abs(dg) ? dgx : (uint32_t)abs(dg);
            const uint32_t delta = max_pair > (uint32_t)abs(db) ? max_pair : (uint32_t)abs(db);
            max_lsb = delta > max_lsb ? delta : max_lsb;
            ++differ;
        }
    }
    fclose(pf);
    printf("[golden] фолбэк %s: отличий %u (бюджет %u), макс. LSB %u, px-файл %s\n", name,
           (unsigned int)differ, (unsigned int)budget, (unsigned int)max_lsb,
           px_ok ? "ok" : "поврежден");
    return px_ok && (differ <= budget) && (max_lsb <= GOLDEN_MAX_LSB);
}

} // namespace golden
