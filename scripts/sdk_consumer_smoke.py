#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/sdk_consumer_smoke.py (Спринт 33–34)
#
# Clean consumer smoke: отдельный CMake-проект видит только установленный SDK
# (find_package RetroPolyEngine CONFIG), собирает sdk_probe + примеры и
# запускает их headless с --version.
#
# Использование:
#   python3 scripts/sdk_consumer_smoke.py --sdk-dir dist/sdk/pc
#   python3 scripts/sdk_consumer_smoke.py --sdk-archive dist/retro-engine-sdk-*.tar.gz
# ============================================================================
import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONSUMER_DIR = os.path.join(REPO_ROOT, "tests", "sdk", "consumer")


def run(cmd, cwd=None, timeout=300):
    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                          timeout=timeout)
    return proc.returncode, proc.stdout, proc.stderr


def extract_archive(path, dest):
    if path.endswith(".zip"):
        with zipfile.ZipFile(path) as zf:
            zf.extractall(dest)
    else:
        with tarfile.open(path) as tf:
            try:
                tf.extractall(dest, filter="data")
            except TypeError:  # Python < 3.12
                tf.extractall(dest)
    sdk = os.path.join(dest, "sdk")
    return sdk if os.path.isdir(sdk) else dest


def find_sdk(sdk_dir, archive, workdir):
    if sdk_dir:
        return sdk_dir
    if archive:
        return extract_archive(archive, workdir)
    candidates = sorted(glob.glob(os.path.join(REPO_ROOT, "dist",
                                               "retro-engine-sdk-*.tar.gz")))
    if candidates:
        return extract_archive(candidates[-1], workdir)
    direct = os.path.join(REPO_ROOT, "dist", "sdk")
    if os.path.isdir(direct):
        return direct
    raise SystemExit("SDK не найден: укажите --sdk-dir/--sdk-archive")


def main():
    parser = argparse.ArgumentParser(description="SDK clean consumer smoke")
    parser.add_argument("--sdk-dir")
    parser.add_argument("--sdk-archive")
    parser.add_argument("--platform", choices=["pc", "ps1", "saturn"], default="pc")
    parser.add_argument("--generator", default="Ninja")
    parser.add_argument("--build-dir", default=None)
    parser.add_argument("--expected-version", default=None)
    parser.add_argument("--skip-run", action="store_true")
    args = parser.parse_args()

    workdir = tempfile.mkdtemp(prefix="rpe_sdk_consumer_")
    try:
        sdk_dir = os.path.abspath(find_sdk(args.sdk_dir, args.sdk_archive, workdir))
        build_dir = os.path.abspath(args.build_dir or os.path.join(
            workdir, "build"))
        source_dir = CONSUMER_DIR
        configure = [
            "cmake", "-S", source_dir, "-B", build_dir, "-G", args.generator,
            "-DCMAKE_PREFIX_PATH=%s" % sdk_dir,
            "-DRETRO_SDK_DIR=%s" % sdk_dir,
        ]
        if args.platform == "ps1":
            source_dir = os.path.join(sdk_dir, "examples", "hello_triangle_ps1")
            configure = [
                "cmake", "-S", source_dir, "-B", build_dir, "-G", args.generator,
                "-DCMAKE_PREFIX_PATH=%s" % sdk_dir,
                "-DCMAKE_TOOLCHAIN_FILE=%s" %
                os.path.join(sdk_dir, "cmake", "ps1-toolchain.cmake"),
            ]
        elif args.platform == "saturn":
            source_dir = os.path.join(sdk_dir, "examples", "hello_triangle_saturn")
            configure = [
                "cmake", "-S", source_dir, "-B", build_dir, "-G", args.generator,
                "-DCMAKE_PREFIX_PATH=%s" % sdk_dir,
                "-DCMAKE_TOOLCHAIN_FILE=%s" %
                os.path.join(sdk_dir, "cmake", "saturn-sdk-toolchain.cmake"),
            ]
        rc, out, err = run(configure)
        if rc != 0:
            print(out)
            print(err, file=sys.stderr)
            return rc
        rc, out, err = run(["cmake", "--build", build_dir])
        if rc != 0:
            print(out)
            print(err, file=sys.stderr)
            return rc

        probes = []
        if args.platform == "pc":
            exe = ".exe" if os.name == "nt" else ""
            probes = [
                os.path.join(build_dir, "sdk_probe" + exe),
                os.path.join(build_dir, "hello_triangle_sdk", "hello_triangle" + exe),
                os.path.join(build_dir, "rotating_cube_sdk", "rotating_cube" + exe),
            ]
        elif args.platform == "ps1":
            probes = [os.path.join(build_dir, "hello_triangle_ps1.exe"),
                      os.path.join(build_dir, "hello_triangle_ps1.elf")]
        elif args.platform == "saturn":
            probes = [os.path.join(build_dir, "hello_triangle_saturn.elf")]

        existing = [p for p in probes if os.path.isfile(p)]
        if not existing:
            raise SystemExit("consumer smoke: не найдены артефакты: %s" %
                             ", ".join(probes))
        outputs = {}
        if not args.skip_run and args.platform == "pc":
            for probe in existing:
                rc, out, err = run([probe, "--version"], timeout=60)
                if rc != 0:
                    print(out)
                    print(err, file=sys.stderr)
                    raise SystemExit("consumer smoke: %s --version failed" % probe)
                if args.expected_version and args.expected_version not in out:
                    raise SystemExit(
                        "consumer smoke: %s не сообщил версию %s: %s" %
                        (probe, args.expected_version, out.strip()))
                outputs[os.path.basename(probe)] = out.strip()
        print(json.dumps({
            "ok": True,
            "sdk_dir": sdk_dir,
            "platform": args.platform,
            "build_dir": build_dir,
            "artifacts": [os.path.basename(p) for p in existing],
            "outputs": outputs,
        }, ensure_ascii=False, indent=2))
        return 0
    finally:
        if not args.build_dir:
            shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
