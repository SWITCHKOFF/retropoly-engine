#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/verify_n64_assets.py (S45–46)
#
# Host-only проверка N64 asset-пайплайна:
#   - `asset_converter --platform n64` даёт детерминированный .tex v1 64×64
#     CI8 + 256-entry CLUT (header v1 не меняется);
#   - повторный прогон побайтово совпадает;
#   - `.mesh` v1 конвертируется детерминированно и не меняет формат;
#   - committed assets/build/*.tex остаются валидными для N64 runtime.
#
# Использование:
#   python3 scripts/verify_n64_assets.py \
#       --converter build/pc-debug/tools/asset_converter/asset_converter
# ============================================================================
import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(REPO_ROOT, "assets", "source")
BUILD = os.path.join(REPO_ROOT, "assets", "build")
TEX_MAGIC = 0x00584554  # ASSETS_TEX_MAGIC ("TEX\0" LE)
TEX_VERSION = 1
TEX_HEADER_BYTES = 20
TEX_PALETTE_BYTES = 512
MESH_MAGIC = 0x4853454D  # "MESH"
MESH_VERSION = 1


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_tex(path):
    data = open(path, "rb").read()
    if len(data) < TEX_HEADER_BYTES:
        raise SystemExit("tex too small: %s" % path)
    magic, version, width, height, palette_offset, pixel_offset = struct.unpack(
        "<IIHHII", data[:TEX_HEADER_BYTES])
    result = {
        "path": path,
        "size_bytes": len(data),
        "magic": "0x%08x" % magic,
        "version": version,
        "width": width,
        "height": height,
        "palette_offset": palette_offset,
        "pixel_offset": pixel_offset,
        "palette_bytes": TEX_PALETTE_BYTES,
        "pixel_bytes": width * height,
    }
    errors = []
    if magic != TEX_MAGIC:
        errors.append("bad magic")
    if version != TEX_VERSION:
        errors.append("bad version")
    if palette_offset != TEX_HEADER_BYTES:
        errors.append("palette offset != header")
    if pixel_offset != TEX_HEADER_BYTES + TEX_PALETTE_BYTES:
        errors.append("pixel offset != palette end")
    if len(data) != pixel_offset + width * height:
        errors.append("payload size mismatch")
    result["errors"] = errors
    return result


def run_converter(converter, args):
    proc = subprocess.run([converter] + args, capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit("asset_converter failed (%d): %s\n%s" %
                         (proc.returncode, proc.stdout, proc.stderr))
    return proc.stdout.strip()


def main():
    parser = argparse.ArgumentParser(description="N64 asset pipeline verifier")
    parser.add_argument("--converter", default=None,
                        help="host asset_converter binary")
    parser.add_argument("--skip-converter", action="store_true")
    parser.add_argument("--output", default=None)
    args = parser.parse_args()

    failures = []
    result = {"ok": False, "converter": args.converter, "checks": {}}

    # 1) Committed N64 runtime assets.
    committed = {}
    for name in ("texture_0.tex", "texture.tex"):
        path = os.path.join(BUILD, name)
        if not os.path.isfile(path):
            failures.append("missing committed asset " + name)
            continue
        info = read_tex(path)
        committed[name] = info
        if info["errors"]:
            failures.append("%s: %s" % (name, ", ".join(info["errors"])))
        if not (8 <= info["width"] <= 256 and info["height"] >= 8):
            failures.append("%s: dimension unsupported" % name)
    result["checks"]["committed_assets"] = committed

    # 2) Host-only N64 conversion: deterministic 64×64 CI8+CLUT .tex v1.
    if not args.skip_converter:
        converter = args.converter or os.path.join(
            REPO_ROOT, "build", "pc-debug", "tools",
            "asset_converter", "asset_converter")
        if not os.path.isfile(converter):
            failures.append("asset_converter not found: " + converter)
        else:
            source = os.path.join(SOURCE, "texture.png")
            if not os.path.isfile(source):
                failures.append("assets/source/texture.png missing")
            else:
                with tempfile.TemporaryDirectory(prefix="rpe_n64_asset_") as tmp:
                    a = os.path.join(tmp, "a.tex")
                    b = os.path.join(tmp, "b.tex")
                    run_converter(converter, ["--platform", "n64", source, a])
                    run_converter(converter, ["--platform=n64", source, b])
                    info = read_tex(a)
                    deterministic = sha256_file(a) == sha256_file(b)
                    result["checks"]["n64_texture"] = {
                        "sha256": sha256_file(a),
                        "deterministic": deterministic,
                        "width": info["width"],
                        "height": info["height"],
                        "errors": info["errors"],
                    }
                    if info["width"] != 64 or info["height"] != 64:
                        failures.append("n64 texture is not 64x64")
                    if info["errors"]:
                        failures.append("n64 texture invalid: " +
                                        ", ".join(info["errors"]))
                    if not deterministic:
                        failures.append("n64 texture conversion is not deterministic")

                    # .mesh v1 остаётся тем же форматом/байтами при --platform n64.
                    mesh_src = os.path.join(SOURCE, "triangle.gltf")
                    if os.path.isfile(mesh_src):
                        m1 = os.path.join(tmp, "a.mesh")
                        m2 = os.path.join(tmp, "b.mesh")
                        run_converter(converter, ["--platform", "n64", mesh_src, m1])
                        run_converter(converter, [mesh_src, m2])
                        data = open(m1, "rb").read()
                        mesh_ok = (len(data) >= 24 and
                                   struct.unpack("<I", data[:4])[0] == MESH_MAGIC and
                                   struct.unpack("<I", data[4:8])[0] == MESH_VERSION)
                        result["checks"]["n64_mesh"] = {
                            "sha256": sha256_file(m1),
                            "same_as_default": sha256_file(m1) == sha256_file(m2),
                            "format_v1": mesh_ok,
                        }
                        if not mesh_ok:
                            failures.append("n64 mesh is not .mesh v1")
                        if sha256_file(m1) != sha256_file(m2):
                            failures.append("n64 mesh conversion changed by platform flag")
    else:
        result["checks"]["n64_texture"] = {"skipped": True}

    result["ok"] = not failures
    result["failures"] = failures
    text = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        with open(args.output, "w", encoding="utf-8", newline="\n") as f:
            f.write(text + "\n")
    print(text)
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
