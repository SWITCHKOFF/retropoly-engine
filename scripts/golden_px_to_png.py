#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# RetroPolyEngine — scripts/golden_px_to_png.py
#
# Конвертация px-партнёра visual golden (.bin.px) в PNG без внешних
# зависимостей: 8-byte BE header (width, height) + width*height BE u16
# RGB555 (r5 | g5<<5 | b5<<10), как пишет tests/visual/golden_protocol.cpp.
#
# Запуск:
#   python3 scripts/golden_px_to_png.py <input.bin.px> <output.png>

import struct
import sys
import zlib


def read_px(path):
    with open(path, "rb") as f:
        header = f.read(8)
        if len(header) != 8:
            raise ValueError(f"{path}: truncated header")
        width, height = struct.unpack(">II", header)
        data = f.read(width * height * 2)
    if width == 0 or height == 0 or len(data) != width * height * 2:
        raise ValueError(f"{path}: bad payload (expected {width}x{height} RGB555)")
    return width, height, data


def write_png(path, width, height, data):
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # PNG filter: None
        row = data[y * width * 2 : (y + 1) * width * 2]
        for x in range(width):
            value = (row[x * 2] << 8) | row[x * 2 + 1]
            raw.append(((value & 0x1F) << 3) | ((value & 0x1F) >> 2))
            raw.append((((value >> 5) & 0x1F) << 3) | (((value >> 5) & 0x1F) >> 2))
            raw.append((((value >> 10) & 0x1F) << 3) | (((value >> 10) & 0x1F) >> 2))

    def chunk(tag, payload):
        body = tag + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main(argv):
    if len(argv) != 3:
        print("usage: golden_px_to_png.py <input.bin.px> <output.png>", file=sys.stderr)
        return 2
    width, height, data = read_px(argv[1])
    write_png(argv[2], width, height, data)
    print(f"{argv[2]}: {width}x{height}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
