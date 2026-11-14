// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/n64/rdp_command.cpp
//
// Реализация bounded command/display-list арены N64 (контракт — rdp_command.h).
#include "hal/n64/rdp_command.h"

#include "core/log/log.h"
#include "hal/n64/n64_memory.h"

#include <string.h>

namespace n64 {

namespace {

RdpCommandBuffer s_command_buffer;

}  // namespace

void RdpCommandBuffer::reset()
{
    command_arena().reset();
    m_diag = RDP_COMMAND_OK;
    m_count = 0u;
}

bool RdpCommandBuffer::append(uint32_t opcode, const void* payload, size_t size, size_t alignment)
{
    if (!command_arena().inited() || command_arena().capacity() == 0u) {
        m_diag = RDP_COMMAND_NOT_INITIALIZED;
        return false;
    }
    if (opcode == 0u) {
        m_diag = RDP_COMMAND_NULL_ARGUMENT;
        return false;
    }
    if (size != 0u && payload == nullptr) {
        m_diag = RDP_COMMAND_NULL_ARGUMENT;
        return false;
    }
    if (alignment == 0u || (alignment & (alignment - 1u)) != 0u) {
        m_diag = RDP_COMMAND_BAD_ALIGNMENT;
        return false;
    }
    if (alignment < 4u) {
        alignment = 4u;
    }

    const size_t total = sizeof(RdpCommandHeader) + size;
    void* dst = command_arena().allocate(total, alignment);
    if (dst == nullptr) {
        m_diag = RDP_COMMAND_OVERFLOW;
        ++m_overflow_count;
        Log_Message(LOG_LEVEL_ERROR, "n64_rdp",
                    "command arena overflow: %u bytes requested, %u available",
                    static_cast<unsigned>(total),
                    static_cast<unsigned>(command_arena().capacity() - command_arena().used()));
        return false;
    }

    RdpCommandHeader header;
    header.opcode = opcode;
    header.size = static_cast<uint32_t>(size);
    memcpy(dst, &header, sizeof(header));
    if (size != 0u) {
        memcpy(static_cast<uint8_t*>(dst) + sizeof(header), payload, size);
    }
    ++m_count;
    m_diag = RDP_COMMAND_OK;
    return true;
}

size_t RdpCommandBuffer::used() const
{
    return command_arena().used();
}

size_t RdpCommandBuffer::peak() const
{
    return command_arena().peak();
}

size_t RdpCommandBuffer::remaining() const
{
    return command_arena().capacity() - command_arena().used();
}

size_t RdpCommandBuffer::capacity() const
{
    return command_arena().capacity();
}

const char* RdpCommandBuffer::diag_string(RdpCommandDiag diag)
{
    switch (diag) {
        case RDP_COMMAND_OK:
            return "ok";
        case RDP_COMMAND_NOT_INITIALIZED:
            return "not initialized";
        case RDP_COMMAND_NULL_ARGUMENT:
            return "null argument";
        case RDP_COMMAND_BAD_ALIGNMENT:
            return "bad alignment";
        case RDP_COMMAND_OVERFLOW:
            return "overflow";
        case RDP_COMMAND_COUNT:
        default:
            return "unknown";
    }
}

RdpCommandBuffer& rdp_command_buffer()
{
    return s_command_buffer;
}

}  // namespace n64
