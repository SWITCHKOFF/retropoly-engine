// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/memory/pool_allocator.h
//
// Pool allocator для динамических объектов (пули, враги, частицы).
// Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §5.3;
//   - docs/plans/sprint_03_04.md, Неделя 6.
//
// Контракт безопасности (ALLOCATOR SAFETY CONTRACT, §5):
//   - free-list индексов uint16_t (ёмкость пула ≤ 65535);
//   - deallocate отклоняет foreign-указатели (вне [begin,end));
//   - отклоняет interior-указатели (byte_offset % sizeof(T) != 0);
//   - отклоняет double-free (индекс уже не в m_allocated);
//   - НИКАКОГО UB до завершения валидации: объект не разыменовывается,
//     проверяются только числовые адреса.
//
// T — POD-компонент/объект без собственных ресурсов: пул хранит заранее
// сконструированный массив m_objects, deallocate не вызывает деструктор.
#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Фиксированный пул из N объектов T.
 *
 * Свободные слоты связаны односвязным списком через m_indices; m_allocated
 * хранит состояние каждого слота и защищает от double-free. Пул не делает
 * heap-аллокаций и не бросает исключений.
 *
 * @tparam T Тип хранимого объекта (POD, trivially destructible).
 * @tparam N Ёмкость пула: 1..65535 (индекс free-list — uint16_t).
 */
template<typename T, size_t N>
class Pool
{
public:
    static_assert(N > 0, "Pool capacity must be non-zero");
    static_assert(N <= 65535, "Pool capacity must fit uint16_t free-list index");

    /** Инициализация free-list и состояний слотов. */
    Pool() : m_free_head(0), m_used_count(0)
    {
        for (size_t i = 0; i + 1 < N; ++i)
            m_indices[i] = static_cast<uint16_t>(i + 1);
        m_indices[N - 1] = 0xFFFFu;

        for (size_t i = 0; i < N; ++i)
            m_allocated[i] = false;
    }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    /**
     * Выделение слота пула.
     *
     * @return Указатель на объект T либо nullptr, если пул исчерпан.
     */
    T* allocate()
    {
        if (m_free_head == 0xFFFFu)
            return nullptr;

        const uint16_t index = m_free_head;
        m_free_head = m_indices[index];
        m_allocated[index] = true;
        ++m_used_count;
        return &m_objects[index];
    }

    /**
     * Возврат слота в пул.
     *
     * Валидация выполняется до любой работы с объектом: foreign-указатели,
     * interior-указатели и повторный free отклоняются.
     *
     * @param ptr Указатель, ранее выданный allocate() этого пула.
     * @return true при успешном освобождении; false при отказе.
     */
    bool deallocate(T* ptr)
    {
        if (ptr == nullptr)
            return false;

        const uintptr_t pointer = reinterpret_cast<uintptr_t>(ptr);
        const uintptr_t begin = reinterpret_cast<uintptr_t>(&m_objects[0]);
        const uintptr_t end = begin + sizeof(m_objects);
        if (pointer < begin || pointer >= end)
            return false;

        const uintptr_t byte_offset = pointer - begin;
        if (byte_offset % sizeof(T) != 0)
            return false;

        const size_t index = static_cast<size_t>(byte_offset / sizeof(T));
        if (index >= N || !m_allocated[index])
            return false;

        m_allocated[index] = false;
        m_indices[index] = m_free_head;
        m_free_head = static_cast<uint16_t>(index);
        --m_used_count;
        return true;
    }

    /**
     * Обход живых объектов в порядке индексов пула.
     *
     * @param func Функтор, принимающий T& (может изменять объект).
     */
    template<typename Func>
    void for_each(Func&& func)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (m_allocated[i])
                func(m_objects[i]);
        }
    }

    /**
     * Обход живых объектов в порядке индексов пула (только чтение).
     *
     * @param func Функтор, принимающий const T&.
     */
    template<typename Func>
    void for_each(Func&& func) const
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (m_allocated[i])
                func(m_objects[i]);
        }
    }

    /** @return Число занятых слотов. */
    size_t used() const { return m_used_count; }

    /** @return Ёмкость пула. */
    static constexpr size_t capacity() { return N; }

private:
    T m_objects[N];
    uint16_t m_indices[N];
    bool m_allocated[N];
    uint16_t m_free_head;
    size_t m_used_count;
};
