// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/memory/linear_allocator.h
//
// Linear (bump) allocator для загрузки уровней и ассетов.
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §5.2;
//   - docs/plans/sprint_03_04.md, Неделя 5.
//
// Контракт безопасности (ALLOCATOR SAFETY CONTRACT, §5):
//   - проверки переполнения ведутся против remaining, а не через m_offset+size;
//   - выравнивается фактический адрес, а не только offset;
//   - при нехватке памяти возвращается nullptr, состояние не изменяется;
//   - rollback принимает только marker этого же аллокатора и не «из будущего».
//
// Арена с мегабайтными буферами размещается только в static/global storage
// или выдаётся memory subsystem; никогда — на CPU-стеке.
#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Линейный (bump) аллокатор над внешней ареной.
 *
 * Не владеет буфером: память арены предоставляет вызывающий код; время жизни
 * буфера должно покрывать время жизни аллокатора. Освобождение — только
 * целиком через rollback(marker) или reset().
 */
class LinearAllocator
{
public:
    /** Снимок состояния аллокатора для отката. */
    struct Marker
    {
        const LinearAllocator* owner; /**< Аллокатор, выдавший marker. */
        size_t offset;                /**< Значение m_offset на момент mark(). */
    };

    /**
     * Конструктор над готовой ареной.
     *
     * @param buffer Начало арены; nullptr допустим (аллокатор пуст).
     * @param capacity Размер арены в байтах.
     */
    LinearAllocator(void* buffer, size_t capacity)
        : m_buffer(static_cast<uint8_t*>(buffer)), m_capacity(buffer ? capacity : 0), m_offset(0)
    {
    }

    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

    /**
     * Выделение size байт с выравниванием фактического адреса.
     *
     * Проверки overflow-safe: padding и size сравниваются с remaining до
     * формирования результирующего указателя. При нехватке памяти состояние
     * не изменяется.
     *
     * @param size Запрашиваемый размер в байтах (0 допустим).
     * @param alignment Требуемое выравнивание адреса: степень двойки, > 0.
     * @return Выровненный указатель либо nullptr (нет памяти / плохой alignment).
     */
    void* allocate(size_t size, size_t alignment = 8)
    {
        if (m_buffer == nullptr || alignment == 0 || (alignment & (alignment - 1)) != 0)
            return nullptr;
        if (m_offset > m_capacity)
            return nullptr;

        const uintptr_t base = reinterpret_cast<uintptr_t>(m_buffer);
        const uintptr_t current = base + static_cast<uintptr_t>(m_offset);
        const size_t misalignment =
            static_cast<size_t>(current & static_cast<uintptr_t>(alignment - 1));
        const size_t padding = (alignment - misalignment) & (alignment - 1);

        const size_t remaining = m_capacity - m_offset;
        if (padding > remaining)
            return nullptr;
        if (size > remaining - padding)
            return nullptr;

        void* result = m_buffer + m_offset + padding;
        m_offset += padding + size;
        return result;
    }

    /**
     * Снимок текущего состояния для последующего rollback.
     *
     * @return Marker, принадлежащий этому аллокатору.
     */
    Marker mark() const { return Marker{this, m_offset}; }

    /**
     * Откат к ранее сохранённому marker.
     *
     * @param marker Marker этого же аллокатора; offset не больше текущего.
     * @return true при успешном откате; false для foreign marker или marker
     *         «из будущего» (offset > m_offset / > capacity).
     */
    bool rollback(Marker marker)
    {
        if (marker.owner != this)
            return false;
        if (marker.offset > m_offset || marker.offset > m_capacity)
            return false;
        m_offset = marker.offset;
        return true;
    }

    /** Полный сброс арены: used() становится 0. */
    void reset() { m_offset = 0; }

    /** @return Число занятых байт (включая выравнивающий padding). */
    size_t used() const { return m_offset; }

    /** @return Число свободных байт. */
    size_t remaining() const { return m_offset <= m_capacity ? m_capacity - m_offset : 0; }

    /** @return Ёмкость арены в байтах. */
    size_t capacity() const { return m_capacity; }

private:
    uint8_t* m_buffer;
    size_t m_capacity;
    size_t m_offset;
};
