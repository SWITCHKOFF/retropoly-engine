// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/vdp1_command_list.h
//
// Детерминированный командный список VDP1 (Спринт 27–28).
//
// Список строится в command arena Work RAM-L (16 KiB, saturn_memory.h) и
// моделирует реальную таблицу команд VDP1: 32-байтовые команды, link —
// адрес следующей команды / 8, end-маркер 0x8000. На Saturn команды
// передаются в pinned command pipeline Jo Engine (__jo_vdp1_add_command):
// pipeline сам сортирует их по depth-key, перелинковывает и копирует в
// VRAM. Наш link-контракт остаётся проверяемым host-тестами и служит
// диагностическим инвариантом.
//
// Переполнение — явный false + диагностика; silent drop запрещён.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace saturn {

/** Команда VDP1: 8 слов по 32 бита (big-endian layout железа). */
struct Vdp1Command
{
    uint16_t ctrl;
    uint16_t link;
    uint16_t pmod;
    uint16_t colr;
    uint16_t srca;
    uint16_t size;
    int16_t xa;
    int16_t ya;
    int16_t xb;
    int16_t yb;
    int16_t xc;
    int16_t yc;
    int16_t xd;
    int16_t yd;
    uint16_t grda;
    uint16_t reserved;
};

static_assert(sizeof(Vdp1Command) == 32u, "VDP1 command must stay 32 bytes");

/** CMDCTRL: конец списка. */
constexpr uint16_t VDP1_CMD_END = 0x8000u;
/** CMDCTRL: полигон (flat/Gouraud, без текстуры). */
constexpr uint16_t VDP1_CMD_POLYGON = 0x0004u;
/** CMDCTRL: distorted sprite (текстурированный quad). */
constexpr uint16_t VDP1_CMD_DISTORTED_SPRITE = 0x0002u;

/** CMDPMOD: расчёт цвета Gouraud. */
constexpr uint16_t VDP1_PMOD_GOURAUD = 0x0004u;
/** CMDPMOD: 256-цветный CLUT (8bpp). */
constexpr uint16_t VDP1_PMOD_COLOR_MODE_256 = (4u << 3);
/** CMDPMOD: RGB (16bpp). */
constexpr uint16_t VDP1_PMOD_COLOR_MODE_RGB = (5u << 3);
/** CMDPMOD: отключить end-code текстуры. */
constexpr uint16_t VDP1_PMOD_END_CODE_DISABLE = (1u << 7);
/** CMDPMOD: отключить transparent pixel. */
constexpr uint16_t VDP1_PMOD_TRANSPARENT_DISABLE = (1u << 6);
/** CMDPMOD: pre-clipping отключён (команда целиком внутри clip area). */
constexpr uint16_t VDP1_PMOD_PRE_CLIPPING_DISABLE = (1u << 11);

/** Диагностика командного списка. Порядок — контракт. */
enum Vdp1CommandListDiag
{
    VDP1_CMD_LIST_OK = 0,
    VDP1_CMD_LIST_NOT_INITIALIZED,
    VDP1_CMD_LIST_NULL_ARGUMENT,
    VDP1_CMD_LIST_OVERFLOW,
    VDP1_CMD_LIST_ALREADY_FINALIZED,
    VDP1_CMD_LIST_BAD_ALIGNMENT,
    VDP1_CMD_LIST_COUNT
};

/**
 * Линейный список команд VDP1 поверх внешней арены.
 *
 * Не владеет буфером: арена подключается через init(). Хранит один
 * резервный слот под END, поэтому capacity() = usable_commands.
 */
class Vdp1CommandList {
public:
    constexpr Vdp1CommandList()
        : m_commands(nullptr),
          m_capacity(0u),
          m_count(0u),
          m_inited(false),
          m_finalized(false),
          m_diag(VDP1_CMD_LIST_OK)
    {
    }

    Vdp1CommandList(const Vdp1CommandList&) = delete;
    Vdp1CommandList& operator=(const Vdp1CommandList&) = delete;

    /**
     * Подключение арены.
     *
     * @param buffer Начало арены (выравнивается до 32).
     * @param bytes Размер арены.
     * @return true при успехе (минимум 2 команды).
     */
    bool init(void* buffer, size_t bytes);

    /** Отключение арены (capacity = 0). */
    void shutdown();

    /** Сброс текущего кадра (count = 0, finalized = false). */
    void reset();

    /**
     * Добавление команды.
     *
     * @param command Команда (link игнорируется и перезаписывается finalize).
     * @return true при успехе; false — overflow/not initialized.
     */
    bool append(const Vdp1Command& command);

    /**
     * Финализация: запись END и link-цепочки.
     *
     * @param table_base Байтовое смещение таблицы в VDP1 VRAM (для link).
     * @return true при успехе.
     */
    bool finalize(uint32_t table_base = 0u);

    /** @return Команда по индексу (0..count-1) или nullptr. */
    const Vdp1Command* command(uint32_t index) const;

    /** @return END-команда (валидна после finalize). */
    const Vdp1Command* end_command() const;

    /** @return Число команд без END. */
    uint32_t count() const { return m_count; }

    /** @return Число команд включая END (после finalize). */
    uint32_t total_count() const { return m_finalized ? (m_count + 1u) : m_count; }

    /** @return Ёмкость команд без END. */
    uint32_t capacity() const { return m_capacity; }

    /** @return Занятые байты (count × 32). */
    uint32_t used_bytes() const { return m_count * static_cast<uint32_t>(sizeof(Vdp1Command)); }

    /** @return true после init(). */
    bool inited() const { return m_inited; }

    /** @return true после finalize(). */
    bool finalized() const { return m_finalized; }

    /** @return Последний диагностический код. */
    Vdp1CommandListDiag diag() const { return m_diag; }

    /** @return Диагностический код строкой. */
    static const char* diag_string(Vdp1CommandListDiag diag);

private:
    Vdp1Command* m_commands;
    uint32_t m_capacity;
    uint32_t m_count;
    bool m_inited;
    bool m_finalized;
    Vdp1CommandListDiag m_diag;
};

/** @return Глобальный командный список VDP1 (command arena 16 KiB). */
Vdp1CommandList& vdp1_command_list(void);

}  // namespace saturn
