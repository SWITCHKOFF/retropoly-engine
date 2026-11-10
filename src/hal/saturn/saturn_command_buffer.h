// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_command_buffer.h
//
// Каркас командного буфера HAL Saturn (Спринт 23–24, неделя 48).
//
// Ядро рендера упаковывает RenderTriCmd в собственный 64 KiB static buffer;
// этот модуль — будущая арена VDP1-пакетов (S27–28). Уже сейчас он задаёт
// контракт «переполнение = явный false + диагностика, не silent drop» и
// используется render-backend'ом для сброса состояния кадра.
//
// Арена — LinearAllocator над регионом WRAM-H (16 KiB), выделенным в
// saturn_memory.h. Модуль host-тестируемый: не включает Jo Engine/SH-2.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/memory/linear_allocator.h"

namespace saturn {

/** Диагностика командного буфера. Порядок — контракт. */
enum CommandBufferDiag
{
    COMMAND_BUFFER_OK = 0,
    COMMAND_BUFFER_NOT_INITIALIZED,
    COMMAND_BUFFER_NULL_ARGUMENT,
    COMMAND_BUFFER_OVERFLOW,
    COMMAND_BUFFER_BAD_ALIGNMENT,
    COMMAND_BUFFER_COUNT
};

/**
 * Линейная арена VDP1-команд.
 *
 * append() выделяет выровненный участок и копирует байты; при нехватке
 * места возвращает false, НЕ изменяя состояние (rollback через marker).
 */
class CommandBuffer {
public:
    constexpr CommandBuffer() : m_arena(), m_diag(COMMAND_BUFFER_OK) {}

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    /** Подключение арены; nullptr обнуляет буфер. */
    void init(void* buffer, size_t capacity);

    /** Сброс текущего кадра (used() == 0). */
    void reset();

    /**
     * Копирование @p size байт в арену.
     *
     * @param data Источник (nullptr допустим только при size == 0).
     * @param size Размер в байтах.
     * @param alignment Выравнивание (степень двойки; < 4 поднимается до 4).
     * @return true при успехе; false при overflow/плохих аргументах.
     */
    bool append(const void* data, size_t size, size_t alignment = 4u);

    /** @return Занятые байты текущего кадра. */
    size_t used() const { return m_arena.used(); }

    /** @return Свободные байты. */
    size_t remaining() const { return m_arena.remaining(); }

    /** @return Ёмкость арены. */
    size_t capacity() const { return m_arena.capacity(); }

    /** @return Последний диагностический код. */
    CommandBufferDiag diag() const { return m_diag; }

    /** @return Диагностический код строкой. */
    static const char* diag_string(CommandBufferDiag diag);

private:
    LinearAllocator m_arena;
    CommandBufferDiag m_diag;
};

/** @return Глобальный командный буфер HAL Saturn (WRAM-H, 16 KiB). */
CommandBuffer& command_buffer(void);

}  // namespace saturn
