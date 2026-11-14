// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp1_command_list.cpp
//
// Реализация командного списка VDP1 (контракты — заголовок).
#include "hal/saturn/vdp1_command_list.h"

#include "core/log/log.h"

#include <string.h>

namespace saturn {

namespace {

Vdp1CommandList s_command_list;

}  // namespace

bool Vdp1CommandList::init(void* buffer, size_t bytes)
{
    m_commands = nullptr;
    m_capacity = 0u;
    m_count = 0u;
    m_inited = false;
    m_finalized = false;

    if (buffer == nullptr || bytes < 2u * sizeof(Vdp1Command)) {
        m_diag = VDP1_CMD_LIST_NULL_ARGUMENT;
        return false;
    }

    const uintptr_t begin = reinterpret_cast<uintptr_t>(buffer);
    const uintptr_t aligned_begin = (begin + 31u) & ~static_cast<uintptr_t>(31u);
    const size_t skipped = static_cast<size_t>(aligned_begin - begin);
    if (skipped >= bytes) {
        m_diag = VDP1_CMD_LIST_BAD_ALIGNMENT;
        return false;
    }

    const size_t usable = (bytes - skipped) / sizeof(Vdp1Command);
    if (usable < 2u) {
        m_diag = VDP1_CMD_LIST_BAD_ALIGNMENT;
        return false;
    }

    m_commands = reinterpret_cast<Vdp1Command*>(aligned_begin);
    m_capacity = static_cast<uint32_t>(usable - 1u);  // reserve END
    m_inited = true;
    m_diag = VDP1_CMD_LIST_OK;
    return true;
}

void Vdp1CommandList::shutdown()
{
    m_commands = nullptr;
    m_capacity = 0u;
    m_count = 0u;
    m_inited = false;
    m_finalized = false;
    m_diag = VDP1_CMD_LIST_OK;
}

void Vdp1CommandList::reset()
{
    m_count = 0u;
    m_finalized = false;
    m_diag = VDP1_CMD_LIST_OK;
}

bool Vdp1CommandList::append(const Vdp1Command& command)
{
    if (!m_inited || m_commands == nullptr) {
        m_diag = VDP1_CMD_LIST_NOT_INITIALIZED;
        return false;
    }
    if (m_finalized) {
        m_diag = VDP1_CMD_LIST_ALREADY_FINALIZED;
        return false;
    }
    if (m_count >= m_capacity) {
        m_diag = VDP1_CMD_LIST_OVERFLOW;
        Log_Message(LOG_LEVEL_ERROR, "saturn_hal",
                    "vdp1 command list overflow: %u commands, capacity %u",
                    static_cast<unsigned>(m_count), static_cast<unsigned>(m_capacity));
        return false;
    }

    m_commands[m_count] = command;
    m_commands[m_count].link = 0u;
    ++m_count;
    m_diag = VDP1_CMD_LIST_OK;
    return true;
}

bool Vdp1CommandList::finalize(uint32_t table_base)
{
    if (!m_inited || m_commands == nullptr) {
        m_diag = VDP1_CMD_LIST_NOT_INITIALIZED;
        return false;
    }
    if (m_finalized) {
        m_diag = VDP1_CMD_LIST_ALREADY_FINALIZED;
        return false;
    }

    // link = адрес следующей команды / 8. Последняя команда ссылается на END.
    for (uint32_t i = 0u; i < m_count; ++i) {
        const uint32_t next_address =
            table_base + (i + 1u) * static_cast<uint32_t>(sizeof(Vdp1Command));
        m_commands[i].link = static_cast<uint16_t>(next_address / 8u);
    }

    Vdp1Command& end = m_commands[m_count];
    memset(&end, 0, sizeof(end));
    end.ctrl = VDP1_CMD_END;
    end.link = 0u;

    m_finalized = true;
    m_diag = VDP1_CMD_LIST_OK;
    return true;
}

const Vdp1Command* Vdp1CommandList::command(uint32_t index) const
{
    if (!m_inited || index >= m_count) {
        return nullptr;
    }
    return &m_commands[index];
}

const Vdp1Command* Vdp1CommandList::end_command() const
{
    if (!m_inited || !m_finalized) {
        return nullptr;
    }
    return &m_commands[m_count];
}

const char* Vdp1CommandList::diag_string(Vdp1CommandListDiag diag)
{
    switch (diag) {
        case VDP1_CMD_LIST_OK:
            return "ok";
        case VDP1_CMD_LIST_NOT_INITIALIZED:
            return "not initialized";
        case VDP1_CMD_LIST_NULL_ARGUMENT:
            return "null argument";
        case VDP1_CMD_LIST_OVERFLOW:
            return "overflow";
        case VDP1_CMD_LIST_ALREADY_FINALIZED:
            return "already finalized";
        case VDP1_CMD_LIST_BAD_ALIGNMENT:
            return "bad alignment";
        case VDP1_CMD_LIST_COUNT:
        default:
            return "unknown";
    }
}

Vdp1CommandList& vdp1_command_list(void)
{
    return s_command_list;
}

}  // namespace saturn
