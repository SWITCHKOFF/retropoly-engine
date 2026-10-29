// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/memory/stack_allocator.h
//
// Stack (LIFO) allocator для временных данных кадра.
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §5.4;
//   - docs/plans/sprint_03_04.md, Неделя 7.
//
// Контракт безопасности (ALLOCATOR SAFETY CONTRACT, §5):
//   - проверки overflow-safe (remaining, а не m_offset+size);
//   - выравнивается фактический адрес;
//   - pop принимает только marker этого же аллокатора и не «из будущего»;
//   - при нехватке памяти возвращается nullptr, состояние не изменяется.
//
// Арена кадра размещается в static/global storage или выдаётся memory
// subsystem; никогда — на CPU-стеке.
#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Стековый (LIFO) аллокатор над внешней ареной.
 *
 * Не владеет буфером. Поддерживает вложенные кадры через mark()/pop():
 * pop освобождает всё, что было выделено после соответствующего marker.
 */
class StackAllocator
{
public:
    /** Снимок состояния аллокатора для отката. */
    struct Marker
    {
        const StackAllocator* owner; /**< Аллокатор, выдавший marker. */
        size_t offset;               /**< Значение m_offset на момент mark(). */
    };

    /**
     * Конструктор над готовой ареной.
     *
     * @param buffer Начало арены; nullptr допустим (аллокатор пуст).
     * @param capacity Размер арены в байтах.
     */
    StackAllocator(void* buffer, size_t capacity)
        : m_buffer(static_cast<uint8_t*>(buffer)), m_capacity(buffer ? capacity : 0), m_offset(0)
    {
    }

    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    /**
     * Выделение size байт с выравниванием фактического адреса.
     *
     * @param size Запрашиваемый размер в байтах (0 допустим).
     * @param alignment Требуемое выравнивание адреса: степень двойки, > 0.
     * @return Выровненный указатель либо nullptr (нет памяти / плохой alignment).
     */
    void* push(size_t size, size_t alignment = 8)
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
     * Снимок текущего состояния для последующего pop.
     *
     * @return Marker, принадлежащий этому аллокатору.
     */
    Marker mark() const { return Marker{this, m_offset}; }

    /**
     * Откат к ранее сохранённому marker (LIFO).
     *
     * @param marker Marker этого же аллокатора; offset не больше текущего.
     * @return true при успешном откате; false для foreign marker или marker
     *         «из будущего» (offset > m_offset / > capacity).
     */
    bool pop(Marker marker)
    {
        if (marker.owner != this)
            return false;
        if (marker.offset > m_offset || marker.offset > m_capacity)
            return false;
        m_offset = marker.offset;
        return true;
    }

    /** Сброс текущего кадра: used() становится 0. */
    void reset_frame() { m_offset = 0; }

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
