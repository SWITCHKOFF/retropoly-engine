#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/ui_smoke.py (Спринт 35–36)
#
# Headless UI smoke под xvfb-run:
#   старт → New Project (hello_triangle) → import cube.gltf/texture.png →
#   Build PC → Run.
#
# Примеры:
#   python3 scripts/ui_smoke.py --sdk-dir build/pc-release/sdk-stage-pc-x86_64 \
#       --ui-binary build/ui-release/ui/retro_engine_ui
#   python3 scripts/ui_smoke.py --sdk-archive dist/retro-engine-sdk-0.2.1-pc-*.tar.gz \
#       --ui-package dist/retro-engine-ui-0.2.1-linux-x86_64.tar.gz
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
    if os.path.isdir(root):
        return root
    return dest


def find_ui_binary(root):
    names = ("retro-engine-ui", "retro-engine-ui.exe", "retro_engine_ui",
             "retro_engine_ui.exe")
    for dirpath, _, filenames in os.walk(root):
        for name in names:
            if name in filenames:
                return os.path.join(dirpath, name)
    return None


def main():
    parser = argparse.ArgumentParser(description="RetroPolyEngine UI smoke")
    parser.add_argument("--sdk-dir", default=None)
    parser.add_argument("--sdk-archive", default=None)
    parser.add_argument("--ui-package", default=None,
                        help="архив UI package (ui/) вместо --ui-binary")
    parser.add_argument("--ui-binary", default=None)
    parser.add_argument("--samples", default=None)
    parser.add_argument("--work-dir", default=None)
    parser.add_argument("--screenshot-dir", default=None)
    parser.add_argument("--platform", choices=["pc", "ps1", "saturn"],
                        default="pc")
    parser.add_argument("--run-seconds", type=int, default=2)
    parser.add_argument("--no-run", action="store_true")
    parser.add_argument("--timeout", type=int, default=900)
    args = parser.parse_args()

    tempdir = tempfile.mkdtemp(prefix="rpe_ui_smoke_")
    sdk_dir = args.sdk_dir
    if args.sdk_archive and not sdk_dir:
        archive = glob.glob(args.sdk_archive)
        if not archive:
            raise SystemExit("SDK archive not found: %s" % args.sdk_archive)
        sdk_dir = extract(archive[0], os.path.join(tempdir, "sdk"), "sdk")
    if not sdk_dir:
        raise SystemExit("--sdk-dir or --sdk-archive is required")

    ui_binary = args.ui_binary
    samples = args.samples
    ui_root = None
    if args.ui_package:
        archive = glob.glob(args.ui_package)
        if not archive:
            raise SystemExit("UI package not found: %s" % args.ui_package)
        ui_root = extract(archive[0], os.path.join(tempdir, "ui_pkg"), "ui")
        ui_binary = find_ui_binary(ui_root)
        if samples is None and os.path.isdir(os.path.join(ui_root, "samples")):
            samples = os.path.join(ui_root, "samples")
    if not ui_binary or not os.path.isfile(ui_binary):
        raise SystemExit("UI binary not found")
    if samples is None and args.platform == "pc":
        samples = os.path.join(REPO_ROOT, "assets", "source")
    if samples is None:
        samples = os.path.join(tempdir, "empty-samples")
        os.makedirs(samples, exist_ok=True)

    work_dir = os.path.abspath(args.work_dir or os.path.join(tempdir, "work"))
    os.makedirs(work_dir, exist_ok=True)
    config_dir = os.path.join(work_dir, "config")
    os.makedirs(config_dir, exist_ok=True)

    command = [ui_binary, "--smoke", "--smoke-platform", args.platform,
               "--sdk", os.path.abspath(sdk_dir),
               "--work-dir", work_dir, "--samples", os.path.abspath(samples),
               "--run-seconds", str(args.run_seconds)]
    if args.no_run:
        command.append("--no-run")
    if args.screenshot_dir:
        os.makedirs(args.screenshot_dir, exist_ok=True)
        command += ["--screenshot-dir", os.path.abspath(args.screenshot_dir)]

    env = dict(os.environ)
    env["RPE_UI_CONFIG_DIR"] = config_dir
    env.pop("SDL_VIDEODRIVER", None)
    launcher = command
    if sys.platform.startswith("linux"):
        if shutil.which("xvfb-run") is None:
            raise SystemExit("xvfb-run not found (install xvfb for headless UI smoke)")
        launcher = ["xvfb-run", "-a", "-s", "-screen 0 1280x800x24"] + command
    elif sys.platform == "darwin" and not os.environ.get("DISPLAY"):
        env["SDL_VIDEODRIVER"] = "cocoa"

    print("[ui-smoke] $ " + " ".join(launcher), flush=True)
    try:
        proc = subprocess.run(launcher, cwd=REPO_ROOT, env=env,
                              capture_output=True, text=True,
                              timeout=args.timeout)
    except subprocess.TimeoutExpired:
        raise SystemExit("UI smoke timeout after %ss" % args.timeout)
    sys.stdout.write(proc.stdout)
    sys.stderr.write(proc.stderr)
    if proc.returncode != 0:
        raise SystemExit("UI smoke failed (exit %d)" % proc.returncode)
    if "[smoke] PASS" not in proc.stdout:
        raise SystemExit("UI smoke: PASS marker not found")

    project = os.path.join(work_dir, "smoke_project")
    if not os.path.isdir(project):
        matches = [os.path.join(work_dir, name) for name in os.listdir(work_dir)
                   if name.startswith("smoke_project")]
        if not matches:
            raise SystemExit("UI smoke: project directory not found")
        project = sorted(matches)[-1]
    if args.platform == "pc":
        binary = os.path.join(project, "build", "pc", "hello_triangle")
        if not os.path.isfile(binary) and not os.path.isfile(binary + ".exe"):
            raise SystemExit("UI smoke: built hello_triangle not found")
    if not os.path.isfile(os.path.join(project, ".rpeproject")):
        raise SystemExit("UI smoke: .rpeproject not found")

    print(json.dumps({"ok": True, "project": project, "ui": ui_binary,
                      "sdk": os.path.abspath(sdk_dir),
                      "samples": os.path.abspath(samples)},
                     ensure_ascii=False, indent=2))
    if not args.work_dir:
        shutil.rmtree(tempdir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
