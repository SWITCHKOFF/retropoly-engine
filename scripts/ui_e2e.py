#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/ui_e2e.py (Спринт 35–36, неделя 72)
#
# E2E на чистых распаковках:
#   SDK archive + UI package → UI smoke (New Project → import → Build PC → Run).
# Проверяет manifest/checksums, отсутствие BIOS/ROM/секретов/build-каталогов.
#
# Пример:
#   python3 scripts/ui_e2e.py \
#       --sdk-archive dist/retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz \
#       --ui-package dist/retro-engine-ui-0.2.1-linux-x86_64.tar.gz
# ============================================================================
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FORBIDDEN = ("bios", "roms", "secrets", ".git", "CMakeFiles")


def extract(archive, dest, root_name):
    os.makedirs(dest, exist_ok=True)
    if archive.endswith((".tar.gz", ".tgz")):
        with tarfile.open(archive, "r:gz") as tar:
            tar.extractall(dest)
    elif archive.endswith(".zip"):
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(dest)
    else:
        raise SystemExit("unsupported archive: %s" % archive)
    root = os.path.join(dest, root_name)
    return root if os.path.isdir(root) else dest


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_checksums(root):
    path = os.path.join(root, "checksums.sha256")
    if not os.path.isfile(path):
        raise SystemExit("checksums.sha256 not found in %s" % root)
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            digest, name = line.split("  ", 1)
            target = os.path.join(root, name)
            if not os.path.isfile(target):
                raise SystemExit("checksum target missing: %s" % name)
            if sha256(target) != digest:
                raise SystemExit("checksum mismatch: %s" % name)


def verify_no_forbidden(root):
    for dirpath, dirnames, filenames in os.walk(root):
        for name in list(dirnames):
            if name.lower() in FORBIDDEN:
                raise SystemExit("forbidden directory in package: %s" %
                                 os.path.join(dirpath, name))
        for name in filenames:
            lower = name.lower()
            if lower.endswith((".bin", ".rom", ".bios", ".key", ".pem")):
                # .bin может быть CD-образом demo; в UI package его быть не должно.
                raise SystemExit("forbidden file in UI package: %s" %
                                 os.path.join(dirpath, name))


def main():
    parser = argparse.ArgumentParser(description="RetroPolyEngine UI E2E")
    parser.add_argument("--sdk-archive", required=True)
    parser.add_argument("--ui-package", required=True)
    parser.add_argument("--work-dir", default=None)
    parser.add_argument("--screenshot-dir", default=None)
    parser.add_argument("--run-seconds", type=int, default=2)
    parser.add_argument("--no-run", action="store_true")
    args = parser.parse_args()

    tempdir = tempfile.mkdtemp(prefix="rpe_ui_e2e_")
    work = os.path.abspath(args.work_dir or tempdir)
    os.makedirs(work, exist_ok=True)
    sdk_dir = extract(args.sdk_archive, os.path.join(work, "sdk"), "sdk")
    ui_dir = extract(args.ui_package, os.path.join(work, "ui"), "ui")

    for rel in ("ui_manifest.json", "checksums.sha256", "VERSION.txt",
                "assets/DejaVuSans.ttf", "README-ui.md"):
        if not os.path.exists(os.path.join(ui_dir, rel)):
            raise SystemExit("UI package missing: %s" % rel)
    verify_checksums(ui_dir)
    verify_no_forbidden(ui_dir)

    ui_manifest = json.load(open(os.path.join(ui_dir, "ui_manifest.json"),
                                 encoding="utf-8"))
    sdk_manifest_path = os.path.join(sdk_dir, "sdk_manifest.json")
    if not os.path.isfile(sdk_manifest_path):
        raise SystemExit("SDK manifest not found in clean SDK")
    sdk_manifest = json.load(open(sdk_manifest_path, encoding="utf-8"))
    if sdk_manifest.get("version") != ui_manifest.get("version"):
        raise SystemExit("SDK/UI version mismatch: %s vs %s" %
                         (sdk_manifest.get("version"), ui_manifest.get("version")))
    if sdk_manifest.get("platform") != "pc":
        raise SystemExit("E2E requires PC SDK, got %s" % sdk_manifest.get("platform"))

    samples = os.path.join(ui_dir, "samples")
    if not os.path.isfile(os.path.join(samples, "cube.gltf")):
        raise SystemExit("UI package missing samples/cube.gltf")

    command = [sys.executable, os.path.join(REPO_ROOT, "scripts", "ui_smoke.py"),
               "--sdk-dir", sdk_dir, "--ui-package", args.ui_package,
               "--work-dir", os.path.join(work, "smoke"),
               "--samples", samples, "--run-seconds", str(args.run_seconds)]
    if args.no_run:
        command.append("--no-run")
    if args.screenshot_dir:
        command += ["--screenshot-dir", os.path.abspath(args.screenshot_dir)]
    print("[ui-e2e] $ " + " ".join(command), flush=True)
    proc = subprocess.run(command, cwd=REPO_ROOT)
    if proc.returncode != 0:
        raise SystemExit("UI E2E failed (exit %d)" % proc.returncode)

    project = os.path.join(work, "smoke", "smoke_project")
    if not os.path.isdir(project):
        matches = [os.path.join(work, "smoke", name)
                   for name in os.listdir(os.path.join(work, "smoke"))
                   if name.startswith("smoke_project")]
        project = sorted(matches)[-1] if matches else project
    outputs = os.path.join(project, "assets", "build")
    if not os.path.isfile(os.path.join(outputs, "cube.mesh")):
        raise SystemExit("E2E: imported cube.mesh not found")
    textures = [name for name in os.listdir(outputs)
                if name.startswith("texture") and name.endswith(".tex")]
    if not textures:
        raise SystemExit("E2E: imported texture .tex not found")
    binary = os.path.join(project, "build", "pc", "hello_triangle")
    if not os.path.isfile(binary) and not os.path.isfile(binary + ".exe"):
        raise SystemExit("E2E: hello_triangle build artifact not found")

    print(json.dumps({"ok": True, "sdk": sdk_dir, "ui": ui_dir,
                      "project": project, "sdk_version": sdk_manifest.get("version"),
                      "ui_version": ui_manifest.get("version")},
                     ensure_ascii=False, indent=2))
    print("[ui-e2e] PASS")
    if not args.work_dir:
        shutil.rmtree(tempdir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
