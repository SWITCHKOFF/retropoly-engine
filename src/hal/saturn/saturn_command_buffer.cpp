// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_command_buffer.cpp
//
// Реализация каркаса командного буфера HAL Saturn (контракты — заголовок).
#include "hal/saturn/saturn_command_buffer.h"

#include "core/log/log.h"

#include <string.h>

namespace saturn {

namespace {

CommandBuffer s_command_buffer;

}  // namespace

void CommandBuffer::init(void* buffer, size_t capacity)
{
    m_arena.init(buffer, capacity);
    m_diag = COMMAND_BUFFER_OK;
}

void CommandBuffer::reset()
{
    m_arena.reset();
    m_diag = COMMAND_BUFFER_OK;
}

bool CommandBuffer::append(const void* data, size_t size, size_t alignment)
{
    if (m_arena.capacity() == 0u) {
        m_diag = COMMAND_BUFFER_NOT_INITIALIZED;
        return false;
    }
    if (size == 0u) {
        m_diag = COMMAND_BUFFER_OK;
        return true;
    }
    if (data == nullptr) {
        m_diag = COMMAND_BUFFER_NULL_ARGUMENT;
        return false;
    }

    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
        m_diag = COMMAND_BUFFER_BAD_ALIGNMENT;
        return false;
    }
    if (alignment < 4u) {
        alignment = 4u;
    }

    void* dst = m_arena.allocate(size, alignment);
    if (dst == nullptr) {
        m_diag = COMMAND_BUFFER_OVERFLOW;
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal",
                    "command buffer overflow: %u bytes requested, %u available",
                    static_cast<unsigned>(size), static_cast<unsigned>(m_arena.remaining()));
        return false;
    }

    memcpy(dst, data, size);
    m_diag = COMMAND_BUFFER_OK;
    return true;
}

const char* CommandBuffer::diag_string(CommandBufferDiag diag)
{
    switch (diag) {
        case COMMAND_BUFFER_OK:
            return "ok";
        case COMMAND_BUFFER_NOT_INITIALIZED:
            return "not initialized";
        case COMMAND_BUFFER_NULL_ARGUMENT:
            return "null argument";
        case COMMAND_BUFFER_OVERFLOW:
            return "overflow";
        case COMMAND_BUFFER_BAD_ALIGNMENT:
            return "bad alignment";
        case COMMAND_BUFFER_COUNT:
        default:
            return "unknown";
    }
}

CommandBuffer& command_buffer(void)
{
    return s_command_buffer;
}

}  // namespace saturn
