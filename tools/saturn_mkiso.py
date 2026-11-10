#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# RetroPolyEngine — tools/saturn_mkiso.py
#
# Создание bootable Saturn ISO9660 (MODE1/2048) с generic-boot IP.BIN.
# Fallback для систем, где нет genisoimage/mkisofs (Flatpak и др.).
# Нормативные источники: docs/plans/sprint_21_22.md, docs/SATURN.md,
# docs/adr/ADR-008-saturn-backend.md.
#
# Использование:
#   python3 tools/saturn_mkiso.py \
#       --output retro_saturn.iso \
#       --ip-bin IP.BIN \
#       --input-dir disc \
#       --vol-id RETROENG \
#       --sys-id "SEGA SATURN" \
#       --publisher "SEGA ENTERPRISES, LTD." \
#       --preparer "SEGA ENTERPRISES, LTD." \
#       --app-id "RetroPolyEngine" \
#       --abstract disc/ABS.TXT \
#       --copyright disc/CPY.TXT \
#       --biblio disc/BIB.TXT
#
# Алгоритм:
#   1. Создать стандартный ISO9660 level 1 (8.3 upper-case) с файлами из
#      input-dir. ISO9660 PVD начинается с сектора 16 (байт 32768).
#   2. Записать IP.BIN поверх первых 16 секторов (32768 байт) — аналог
#      genisoimage -generic-boot. IP.BIN должен быть ровно 32768 байт;
#      если меньше — дополняется нулями, если больше — ошибка.
#   3. Проверить, что '/0.BIN;1' присутствует в ISO.

import argparse
import os
import sys

try:
    import pycdlib
except ImportError as exc:
    sys.stderr.write(
        "saturn_mkiso: требуется pycdlib. "
        "Установите: python3 -m pip install pycdlib\n"
    )
    raise SystemExit(1) from exc

SECTOR_SIZE = 2048
BOOT_SECTORS = 16
BOOT_SIZE = SECTOR_SIZE * BOOT_SECTORS


def parse_args():
    parser = argparse.ArgumentParser(
        description="Create a bootable Sega Saturn ISO9660 image with IP.BIN generic-boot."
    )
    parser.add_argument("--output", required=True, help="Output ISO file path")
    parser.add_argument("--ip-bin", required=True, help="IP.BIN boot header (16 sectors, 32768 bytes)")
    parser.add_argument("--input-dir", required=True, help="Directory with files to include")
    parser.add_argument("--vol-id", default="RETROENG", help="ISO volume identifier")
    parser.add_argument("--sys-id", default="SEGA SATURN", help="ISO system identifier")
    parser.add_argument("--publisher", default="SEGA ENTERPRISES, LTD.", help="Publisher ID")
    parser.add_argument("--preparer", default="SEGA ENTERPRISES, LTD.", help="Preparer ID")
    parser.add_argument("--app-id", default="RetroPolyEngine", help="Application ID")
    parser.add_argument("--abstract", help="Abstract file path")
    parser.add_argument("--copyright", help="Copyright file path")
    parser.add_argument("--biblio", help="Bibliographic file path")
    return parser.parse_args()


def make_iso(args):
    if not os.path.isdir(args.input_dir):
        sys.stderr.write(f"saturn_mkiso: input directory not found: {args.input_dir}\n")
        raise SystemExit(2)

    if not os.path.isfile(args.ip_bin):
        sys.stderr.write(f"saturn_mkiso: IP.BIN not found: {args.ip_bin}\n")
        raise SystemExit(2)

    iso = pycdlib.PyCdlib()
    iso.new(
        interchange_level=1,
        vol_ident=args.vol_id,
        sys_ident=args.sys_id,
        pub_ident_str=args.publisher,
        preparer_ident_str=args.preparer,
        app_ident_str=args.app_id,
        abstract_file=args.abstract if args.abstract else "",
        copyright_file=args.copyright if args.copyright else "",
        bibli_file=args.biblio if args.biblio else "",
    )

    has_zero_bin = False
    for root, _dirs, files in os.walk(args.input_dir):
        rel_root = os.path.relpath(root, args.input_dir)
        for name in files:
            local_path = os.path.join(root, name)
            if rel_root == ".":
                iso_path = "/" + name.upper() + ";1"
            else:
                iso_path = "/" + rel_root.upper().replace(os.sep, "/") + "/" + name.upper() + ";1"
            iso.add_file(local_path, iso_path)
            if iso_path.upper() == "/0.BIN;1":
                has_zero_bin = True

    if not has_zero_bin:
        sys.stderr.write("saturn_mkiso: WARNING: /0.BIN;1 not found in input directory\n")

    tmp_iso = args.output + ".tmp"
    iso.write(tmp_iso)
    iso.close()

    with open(args.ip_bin, "rb") as f:
        ip_data = f.read()
    if len(ip_data) > BOOT_SIZE:
        sys.stderr.write(
            f"saturn_mkiso: IP.BIN too large: {len(ip_data)} > {BOOT_SIZE} bytes\n"
        )
        raise SystemExit(2)
    if len(ip_data) < BOOT_SIZE:
        ip_data = ip_data + b"\x00" * (BOOT_SIZE - len(ip_data))

    with open(tmp_iso, "r+b") as f:
        f.seek(0)
        f.write(ip_data)

    os.replace(tmp_iso, args.output)

    # Структурная проверка: 0.BIN;1 присутствует и IP.BIN записан.
    check = pycdlib.PyCdlib()
    check.open(args.output)
    try:
        check.get_record(iso_path="/0.BIN;1")
    except Exception as exc:
        sys.stderr.write(f"saturn_mkiso: ISO validation failed: {exc}\n")
        raise SystemExit(2) from exc
    finally:
        check.close()

    with open(args.output, "rb") as f:
        first_bytes = f.read(16)
    if first_bytes[:15] != b"SEGA SEGASATURN":
        sys.stderr.write(
            "saturn_mkiso: WARNING: first 16 bytes of ISO do not start with 'SEGA SEGASATURN'; "
            "IP.BIN may be invalid\n"
        )

    print(f"saturn_mkiso: created {args.output}")


if __name__ == "__main__":
    make_iso(parse_args())
