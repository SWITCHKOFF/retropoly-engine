#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/verify_n64_rom.py (Спринт 39–40, ADR-014)
#
# Структурная проверка N64 ROM (.z64) без эмулятора:
#   - big-endian header (magic 80 37 12 40, entry 0x80000400, title);
#   - libdragon IPL3: header CRC не используется (нули допустимы);
#     при ненулевых CRC1/CRC2 сверяется стандартный N64 boot checksum;
#   - ELF32 BE MIPS (загружаемый образ) выровнен на 256 и имеет PT_LOAD;
#   - размер ROM в пределах 4–64 MiB-политики (libdragon допускает меньше,
#     engine policy — не больше 64 MiB);
#   - SHA-256 и размер печатаются/пишутся в JSON.
#
# Использование:
#   python3 scripts/verify_n64_rom.py build/n64-release/src/hal/n64/hello_n64.z64
#   python3 scripts/verify_n64_rom.py --json rom.z64
#
# Exit code: 0 — ROM валиден, 1 — нарушение контракта, 2 — ошибка аргументов.
# ============================================================================
import argparse
import hashlib
import json
import os
import struct
import sys

ROM_MAGIC = b"\x80\x37\x12\x40"
ROM_ENTRY = 0x80000400
ROM_MAX_BYTES = 64 * 1024 * 1024
ROM_MIN_BYTES = 0x2000  # header + IPL3 + минимальный ELF
ELF_MAGIC = b"\x7fELF"
ELF_HEADER_ALIGN = 256


def be32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def rol32(value, bits):
    bits &= 31
    return ((value << bits) | (value >> (32 - bits))) & 0xFFFFFFFF


def n64_boot_crc(data):
    """Стандартный N64 boot checksum по 0x1000..0x101000 (CIC 6102 family)."""
    if len(data) < 0x101000:
        return None
    t1 = t2 = t3 = t4 = t5 = t6 = 0xF8CA4DDC
    for offset in range(0x1000, 0x101000, 4):
        d = be32(data, offset)
        if ((t6 + d) & 0xFFFFFFFF) < t6:
            t4 = (t4 + 1) & 0xFFFFFFFF
        t6 = (t6 + d) & 0xFFFFFFFF
        t3 ^= d
        r = rol32(d, d & 0x1F)
        t5 = (t5 + r) & 0xFFFFFFFF
        if t2 > d:
            t2 ^= r
        else:
            t2 ^= t6 ^ d
        t1 = (t1 + (t5 ^ d)) & 0xFFFFFFFF
    return (t6 ^ t4 ^ t3) & 0xFFFFFFFF, (t5 ^ t2 ^ t1) & 0xFFFFFFFF


def find_elf(data):
    for offset in range(0x1000, len(data) - 0x34, ELF_HEADER_ALIGN):
        if data[offset:offset + 4] == ELF_MAGIC:
            return offset
    return None


def parse_elf32_be(data, offset):
    ident = data[offset:offset + 16]
    if len(ident) < 16 or ident[:4] != ELF_MAGIC:
        raise ValueError("ELF magic отсутствует")
    if ident[4] != 1:
        raise ValueError("ELF не 32-bit (EI_CLASS=%d)" % ident[4])
    if ident[5] != 2:
        raise ValueError("ELF не big-endian (EI_DATA=%d)" % ident[5])
    e_type, e_machine, e_version, e_entry = struct.unpack_from(">HHII", data, offset + 16)
    e_phoff, e_shoff, e_flags, e_ehsize = struct.unpack_from(">IIII", data, offset + 28)
    e_phentsize, e_phnum = struct.unpack_from(">HH", data, offset + 42)
    if e_machine != 8:
        raise ValueError("ELF e_machine=%d, ожидается MIPS (8)" % e_machine)
    if e_entry != ROM_ENTRY:
        raise ValueError("ELF entry=0x%08x, ожидается 0x%08x" % (e_entry, ROM_ENTRY))
    if e_phentsize < 32 or e_phnum == 0:
        raise ValueError("ELF без program headers")
    load_segments = 0
    for index in range(e_phnum):
        ph = offset + e_phoff + index * e_phentsize
        if ph + 32 > len(data):
            raise ValueError("program header %d вне ROM" % index)
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = \
            struct.unpack_from(">IIIIIIII", data, ph)
        if p_type == 1:  # PT_LOAD
            if offset + p_offset + p_filesz > len(data):
                raise ValueError("PT_LOAD %d выходит за пределы ROM" % index)
            load_segments += 1
    if load_segments == 0:
        raise ValueError("ELF не содержит PT_LOAD")
    return {
        "offset": offset,
        "entry": e_entry,
        "phnum": e_phnum,
        "load_segments": load_segments,
        "flags": e_flags,
    }


def verify(path):
    result = {
        "file": path,
        "size_bytes": 0,
        "sha256": None,
        "magic_ok": False,
        "entry": None,
        "title": None,
        "category": None,
        "country": None,
        "version": None,
        "crc1": None,
        "crc2": None,
        "crc_policy": None,
        "elf": None,
        "errors": [],
    }
    try:
        with open(path, "rb") as handle:
            data = handle.read()
    except OSError as exc:
        result["errors"].append("не удалось прочитать ROM: %s" % exc)
        return result

    result["size_bytes"] = len(data)
    result["sha256"] = hashlib.sha256(data).hexdigest()

    if len(data) < ROM_MIN_BYTES:
        result["errors"].append("ROM слишком мал: %d B (< %d)" % (len(data), ROM_MIN_BYTES))
        return result
    if len(data) > ROM_MAX_BYTES:
        result["errors"].append("ROM больше 64 MiB: %d B" % len(data))

    if data[:4] != ROM_MAGIC:
        result["errors"].append("неверный N64 magic: %s" % data[:4].hex())
    else:
        result["magic_ok"] = True

    entry = be32(data, 0x08)
    result["entry"] = "0x%08x" % entry
    if entry != ROM_ENTRY:
        result["errors"].append("entry=0x%08x, ожидается 0x%08x" % (entry, ROM_ENTRY))

    title_raw = data[0x20:0x34].split(b"\x00", 1)[0]
    try:
        title = title_raw.decode("ascii")
    except UnicodeDecodeError:
        title = title_raw.decode("latin-1", "replace")
        result["errors"].append("title не ASCII")
    result["title"] = title
    if not title.strip():
        result["errors"].append("пустой ROM title")

    result["category"] = data[0x3B:0x3C].decode("ascii", "replace")
    result["country"] = data[0x3E:0x3F].decode("ascii", "replace")
    result["version"] = data[0x3F]

    crc1 = be32(data, 0x10)
    crc2 = be32(data, 0x14)
    result["crc1"] = "0x%08x" % crc1
    result["crc2"] = "0x%08x" % crc2
    if crc1 == 0 and crc2 == 0:
        # libdragon IPL3 (boot/README.md): «IPL3 does not read or care about
        # the 64-byte header of the ROM», checksum не требуется.
        result["crc_policy"] = "libdragon_ipl3_header_crc_unused"
    else:
        expected = n64_boot_crc(data)
        if expected is None:
            result["errors"].append("ненулевой CRC при ROM < 1 MiB (нельзя проверить)")
        elif (crc1, crc2) != expected:
            result["errors"].append(
                "CRC mismatch: header=0x%08x/0x%08x, рассчитано=0x%08x/0x%08x" %
                (crc1, crc2, expected[0], expected[1]))
        else:
            result["crc_policy"] = "n64_boot_crc_valid"

    elf_offset = find_elf(data)
    if elf_offset is None:
        result["errors"].append("ELF-образ не найден (256-выравнивание от 0x1000)")
    else:
        try:
            result["elf"] = parse_elf32_be(data, elf_offset)
        except ValueError as exc:
            result["errors"].append("ELF: %s" % exc)

    return result


def main(argv):
    parser = argparse.ArgumentParser(description="Verify N64 .z64 ROM structure (ADR-014)")
    parser.add_argument("rom", help="путь к .z64")
    parser.add_argument("--json", action="store_true", help="печатать JSON-отчёт")
    args = parser.parse_args(argv)

    result = verify(args.rom)
    ok = not result["errors"]

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print("N64 ROM: %s" % result["file"])
        print("  size: %d B (%.1f KiB)" % (result["size_bytes"], result["size_bytes"] / 1024.0))
        print("  sha256: %s" % result["sha256"])
        print("  title: %r category=%r country=%r version=%d" %
              (result["title"], result["category"], result["country"], result["version"] or 0))
        print("  entry: %s magic_ok=%s" % (result["entry"], result["magic_ok"]))
        print("  crc: %s/%s policy=%s" %
              (result["crc1"], result["crc2"], result["crc_policy"]))
        if result["elf"]:
            print("  elf: offset=0x%x entry=%s pt_load=%d" %
                  (result["elf"]["offset"], "0x%08x" % result["elf"]["entry"],
                   result["elf"]["load_segments"]))
        if ok:
            print("RESULT: PASS")
        else:
            print("RESULT: FAIL")
            for error in result["errors"]:
                print("  - %s" % error)

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
