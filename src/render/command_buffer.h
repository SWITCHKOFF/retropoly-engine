// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/render/command_buffer.h
//
// Engine-side staging command buffer рендера (§7.3 норматива: PACK —
// «упаковка backend-neutral render commands в фиксированный command
// buffer; overflow возвращает status=false и диагностируется»).
//
// Контракты (нормативные):
//   - фиксированный буфер RENDER_COMMAND_BUFFER_BYTES = 64 KiB
//     (дефолт-минимум политики; вместимость команд выводится из размера
//     RENDER_TRI_CMD компилятором);
//   - draw_triangle(...) → status bool; ПЕРЕПОЛНЕНИЕ — ЯВНАЯ ОШИБКА
//     (status=false; никакой silent drop примитива запрещён);
//   - невалидный вход (NULL) — status=false;
//   - команды сохраняют ПОРЯДОК ВСТАВКИ (submission_index сортировки);
//   - reset() после EndFrame (BeginFrame — сброс состояния кадра).
//
// Память: экземпляр держит 64 KiB внутри — создавать ТОЛЬКО как static
// (renderer.cpp) или в тестах (хост-стек ≥ 256 KiB); на стеке PS1
// (32 KiB) экземпляр не создавать.
#pragma once

#include "hal/platform.h"

#include <string.h>

/** Размер буфера команд (байт) — нормативная вместимость кадра. */
#define RENDER_COMMAND_BUFFER_BYTES (64u * 1024u)

/**
 * Backend-neutral команда «draw one triangle» (PACK stage).
 * Хранит камера-space вершины (view-space, fixed 16.16) и MVP-матрицы,
 * нужные backend'у после SORT: PC — проекция на GPU (u_mvp), PS1 —
 * конвертация в GT3/GTE (Спринт 13–14).
 */
typedef struct RenderTriCmd
{
    Mat4 mvp;                  /**< proj·view·model (fixed 16.16). */
    TextureHandle texture;     /**< Привязанная CI8-текстура. */
    uint32_t depth_key;        /**< ключ Painter's-сортировки (far→near). */
    uint32_t submission_index; /**< Порядок вставки (tie-breaker). */
    fixed_t x[3];              /**< view-space координаты вершин (fixed). */
    fixed_t y[3];              /**< view-space координаты вершин (fixed). */
    fixed_t z[3];              /**< view-space координаты вершин (fixed). */
    uint8_t attr[18];          /**< per-vertex u0..u2,v,r,g,b,a повершинно. */
} RenderTriCmd;

/**
 * Фиксированный command buffer кадра. Команды — тривиально конвертируемые
 * POD; буфер линейный, записи плотные (без выравнивания-паддингов).
 */
class CommandBuffer {
public:
    CommandBuffer() { reset(); }

    /**
     * Добавление команды треугольника.
     *
     * @param cmd Команда (копируется целиком; NULL — отказ).
     * @return true — команда добавлена; false — НЕТ ВМЕСТИМОСТИ или NULL
     *         (явная ошибка: диагностический статус кадра фиксируется).
     */
    bool draw_triangle(const RenderTriCmd* cmd)
    {
        if (cmd == nullptr) {
            m_overflowed = true; // Диагностика: отказ тоже «виден» кадру.
            return false;
        }
        if (m_count >= capacity() || m_used + sizeof(RenderTriCmd) > SIZE) {
            m_overflowed = true; // Явная ошибка (silent drop запрещён).
            return false;
        }
        memcpy(&m_storage[m_count], cmd, sizeof(RenderTriCmd));
        ++m_count;
        m_used += sizeof(RenderTriCmd);
        return true;
    }

    /** Вместимость команд (компилируемая константа 64 KiB / sizeof cmd). */
    static uint32_t capacity() { return (uint32_t)(SIZE / sizeof(RenderTriCmd)); }

    /** Число команд кадра (упакованный PACK-подмассив). */
    uint32_t command_count() const { return m_count; }

    /** Команда по индексу (NULL — вне [0..command_count)). */
    const RenderTriCmd* command(uint32_t index) const
    {
        if (index >= m_count) {
            return nullptr;
        }
        return &m_storage[index];
    }

    /** Занято байт буфера (в том числе в overflow-сценарии). */
    uint32_t used_bytes() const { return m_used; }

    /** Явная ошибка кадра: хотя бы одна команда не была добавлена. */
    bool overflowed() const { return m_overflowed; }

    /** Сброс буфера кадра (перед BeginFrame). */
    void reset()
    {
        m_used = 0u;
        m_count = 0u;
        m_overflowed = false;
    }

private:
    static const uint32_t SIZE = RENDER_COMMAND_BUFFER_BYTES;

    RenderTriCmd m_storage[SIZE / sizeof(RenderTriCmd)];
    uint32_t m_used;
    uint32_t m_count;
    bool m_overflowed;
};
