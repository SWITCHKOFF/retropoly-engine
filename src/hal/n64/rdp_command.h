// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/rdp_command.h
//
// Bounded command/display-list arena N64 (Спринт 41–42, неделя 83).
//
// Рендер-ядро упаковывает собственные RenderTriCmd в 64 KiB static buffer;
// этот модуль — арена RDP-команд/display-list (128 KiB, static .bss).
// Контракт «переполнение = явный false + диагностика, не silent drop»
// сохраняется до реального RSP/RDP-пути S43–44.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace n64 {

/** Диагностика командной арены. Порядок — контракт. */
enum RdpCommandDiag
{
    RDP_COMMAND_OK = 0,
    RDP_COMMAND_NOT_INITIALIZED,
    RDP_COMMAND_NULL_ARGUMENT,
    RDP_COMMAND_BAD_ALIGNMENT,
    RDP_COMMAND_OVERFLOW,
    RDP_COMMAND_COUNT
};

/** Заголовок записи арены (host-структура; в ROM/RDP не сериализуется). */
struct RdpCommandHeader
{
    uint32_t opcode;
    uint32_t size; /**< Размер payload в байтах (без заголовка). */
};

/** Opcode-заглушки skeleton'а S41–42 (реальные RDP-команды — S43–44). */
enum RdpCommandOpcode
{
    RDP_OPCODE_BEGIN_FRAME = 1,
    RDP_OPCODE_CLEAR = 2,
    RDP_OPCODE_END_FRAME = 3
};

/**
 * Линейная арена RDP-команд над n64::command_arena().
 *
 * append() при переполнении возвращает false и НЕ изменяет состояние.
 */
class RdpCommandBuffer {
public:
    constexpr RdpCommandBuffer() : m_diag(RDP_COMMAND_OK), m_count(0u), m_overflow_count(0u) {}

    RdpCommandBuffer(const RdpCommandBuffer&) = delete;
    RdpCommandBuffer& operator=(const RdpCommandBuffer&) = delete;

    /** Сброс кадра (used == 0, diag == OK). */
    void reset();

    /**
     * Добавить запись: заголовок + payload.
     *
     * @param opcode RdpCommandOpcode (0 запрещён).
     * @param payload Данные (nullptr допустим только при size == 0).
     * @param size Размер payload.
     * @param alignment Выравнивание записи (степень двойки; < 4 → 4).
     * @return true при успехе; false — overflow/плохие аргументы.
     */
    bool append(uint32_t opcode, const void* payload, size_t size, size_t alignment = 8u);

    size_t used() const;
    size_t peak() const;
    size_t remaining() const;
    size_t capacity() const;
    uint32_t count() const { return m_count; }
    uint32_t overflow_count() const { return m_overflow_count; }
    RdpCommandDiag diag() const { return m_diag; }

    /** @return Статическая строка диагностического кода. */
    static const char* diag_string(RdpCommandDiag diag);

private:
    RdpCommandDiag m_diag;
    uint32_t m_count;
    uint32_t m_overflow_count;
};

/** @return Глобальная командная арена RDP (128 KiB static). */
RdpCommandBuffer& rdp_command_buffer();

}  // namespace n64
