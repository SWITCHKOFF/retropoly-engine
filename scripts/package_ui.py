#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/package_ui.py (Спринт 35–36)
#
# Собирает детерминированный пакет UI client из build/<preset>/ui:
#   ui/
#     retro-engine-ui[.exe]
#     assets/DejaVuSans.ttf
#     samples/cube.gltf, texture.png
#     README-ui.md
#     VERSION.txt
#     LEGAL.md
#     ui_manifest.json
#     checksums.sha256
#
# Архивы: retro-engine-ui-<ver>-<os>-<arch>.{tar.gz,zip};
# macOS дополнительно .dmg (если доступен hdiutil).
#
# Пример:
#   cmake --preset ui-release && cmake --build --preset ui-release
#   python3 scripts/package_ui.py --build-dir build/ui-release \
#       --version 0.2.1 --output-dir dist
# ============================================================================
import argparse
import datetime as _dt
import gzip
import hashlib
import json
import os
import platform as _platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PACKAGE = "retro-engine-ui"
FORBIDDEN_PARTS = {".git", "build", "CMakeFiles", "bios", "roms", "secrets"}


def run(cmd, cwd=None, check=False):
    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if check and proc.returncode != 0:
        raise SystemExit("command failed: %s\n%s\n%s" %
                         (" ".join(cmd), proc.stdout, proc.stderr))
    return proc


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_info():
    commit = run(["git", "-C", REPO_ROOT, "rev-parse", "HEAD"])
    if commit.returncode != 0:
        return "", "", "1970-01-01T00:00:00+00:00", True
    sha = commit.stdout.strip()
    describe = run(["git", "-C", REPO_ROOT, "describe", "--tags", "--always",
                    "--dirty"]).stdout.strip()
    date = run(["git", "-C", REPO_ROOT, "show", "-s", "--format=%cI",
                sha]).stdout.strip()
    dirty = bool(run(["git", "-C", REPO_ROOT, "status", "--porcelain"]).stdout.strip())
    return sha, describe, date or "1970-01-01T00:00:00+00:00", dirty


def host_names():
    system = _platform.system().lower()
    machine = _platform.machine().lower()
    os_name = {"linux": "linux", "darwin": "macos", "windows": "windows"}.get(
        system, system)
    if machine in ("amd64", "x86_64", "x64"):
        arch = "x86_64"
    elif machine in ("arm64", "aarch64"):
        arch = "arm64"
    else:
        arch = machine
    return os_name, arch


def find_binary(build_dir):
    name = "retro_engine_ui.exe" if os.name == "nt" else "retro_engine_ui"
    for candidate in (
            os.path.join(build_dir, "ui", name),
            os.path.join(build_dir, "bin", name),
            os.path.join(build_dir, name)):
        if os.path.isfile(candidate):
            return candidate
    raise SystemExit("UI: не найден retro_engine_ui в %s/ui" % build_dir)


def read_version(build_dir):
    cache = os.path.join(build_dir, "CMakeCache.txt")
    if os.path.isfile(cache):
        with open(cache, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
        match = re.search(r"CMAKE_PROJECT_VERSION:STATIC=([0-9.]+)", text)
        if match:
            return match.group(1)
    return "0.2.1"


def iter_files(root):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if d not in FORBIDDEN_PARTS)
        for name in sorted(filenames):
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            yield path, rel


def write_readme(stage_dir, args):
    body = """# RetroPolyEngine UI client {version}

Launcher/editor для проектов RetroPolyEngine ({os}/{arch}).

## Запуск

```sh
./retro-engine-ui
```

При первом запуске укажите SDK в **Настройки → SDK PC** (каталог с
`sdk_manifest.json`). Поддерживаются SDK той же major/minor серии, что и UI.

## Быстрый старт

1. **Новый проект** → имя/каталог → target PC/PS1/Saturn → шаблон.
2. **Импортировать ассеты** (`.gltf`, `.glb`, `.png`, `.jpg`) — вызывается
   `asset_converter` из SDK.
3. **Собрать** — CMake/Ninja в отдельном процессе, лог в UI.
4. **Запустить** — PC executable или `.cue` в настроенном эмуляторе.

Примеры ассетов: `samples/cube.gltf`, `samples/texture.png`.

Документация: `docs/UI_CLIENT.md` в репозитории движка.

## Проверка целостности

```sh
sha256sum -c checksums.sha256
```
""".format(version=args.version, os=args.os, arch=args.arch)
    with open(os.path.join(stage_dir, "README-ui.md"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write(body)


def write_version(stage_dir, args, git_sha, git_describe, git_date, dirty):
    with open(os.path.join(stage_dir, "VERSION.txt"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write("# RetroPolyEngine UI client\n")
        f.write("name=RetroPolyEngine-UI\n")
        f.write("version=%s\n" % args.version)
        f.write("package=%s\n" % PACKAGE)
        f.write("os=%s\n" % args.os)
        f.write("arch=%s\n" % args.arch)
        f.write("git_commit=%s\n" % (git_sha or "unknown"))
        f.write("git_describe=%s\n" % (git_describe or "unknown"))
        f.write("git_dirty=%s\n" % ("true" if dirty else "false"))
        f.write("commit_date=%s\n" % git_date)
        f.write("sdk_series=0.2\n")
        f.write("ui_toolkit=SDL3+Dear ImGui\n")


def write_manifest(stage_dir, args, git_sha, git_describe, git_date, dirty):
    files = []
    for path, rel in iter_files(stage_dir):
        if rel in ("ui_manifest.json", "checksums.sha256"):
            continue
        files.append({"path": rel, "size": os.path.getsize(path),
                      "sha256": sha256_file(path)})
    manifest = {
        "schema": 1,
        "package": PACKAGE,
        "name": "RetroPolyEngine UI",
        "version": args.version,
        "sdk_series": "0.2",
        "ui_toolkit": "SDL3+Dear ImGui",
        "git_commit": git_sha or "unknown",
        "git_describe": git_describe or "unknown",
        "git_dirty": dirty,
        "commit_date": git_date,
        "os": args.os,
        "arch": args.arch,
        "components": {
            "project_create_open": True,
            "target_select": True,
            "asset_import": True,
            "build_and_run": True,
            "diagnostics": True,
            "version_compatibility": True,
        },
        "files": files,
    }
    with open(os.path.join(stage_dir, "ui_manifest.json"), "w", encoding="utf-8",
              newline="\n") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2, sort_keys=True)
        f.write("\n")


def write_checksums(stage_dir):
    lines = []
    for path, rel in iter_files(stage_dir):
        if rel == "checksums.sha256":
            continue
        lines.append("%s  %s" % (sha256_file(path), rel))
    with open(os.path.join(stage_dir, "checksums.sha256"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def epoch_seconds(git_date):
    if os.environ.get("SOURCE_DATE_EPOCH"):
        return int(os.environ["SOURCE_DATE_EPOCH"])
    try:
        return int(_dt.datetime.fromisoformat(git_date).timestamp())
    except (ValueError, TypeError):
        return 0


def make_archives(stage_dir, output_dir, args, epoch):
    base = "%s-%s-%s-%s" % (PACKAGE, args.version, args.os, args.arch)
    tar_path = os.path.join(output_dir, base + ".tar.gz")
    zip_path = os.path.join(output_dir, base + ".zip")
    files = sorted(list(iter_files(stage_dir)), key=lambda item: item[1])
    with open(tar_path, "wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=epoch,
                           compresslevel=9) as gz:
            with tarfile.open(fileobj=gz, mode="w", format=tarfile.PAX_FORMAT) as tar:
                for path, rel in files:
                    info = tar.gettarinfo(path, arcname="ui/" + rel)
                    info.mtime = epoch
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    info.mode = 0o755 if rel == "retro-engine-ui" else 0o644
                    with open(path, "rb") as f:
                        tar.addfile(info, f)
    zip_date = _dt.datetime.fromtimestamp(max(epoch, 315532800),
                                          tz=_dt.timezone.utc)
    date_time = (zip_date.year, zip_date.month, zip_date.day,
                 zip_date.hour, zip_date.minute, zip_date.second)
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9) as zf:
        for path, rel in files:
            info = zipfile.ZipInfo("ui/" + rel, date_time=date_time)
            info.external_attr = (0o755 if rel == "retro-engine-ui" else 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as f:
                zf.writestr(info, f.read())

    archives = []
    for path in (tar_path, zip_path):
        digest = sha256_file(path)
        with open(path + ".sha256", "w", encoding="utf-8", newline="\n") as f:
            f.write("%s  %s\n" % (digest, os.path.basename(path)))
        archives.append({"path": path, "sha256": digest,
                         "size": os.path.getsize(path)})

    if args.os == "macos" and shutil.which("hdiutil"):
        dmg_path = os.path.join(output_dir, base + ".dmg")
        tmp_dir = tempfile.mkdtemp(prefix="rpe_ui_dmg_")
        target = os.path.join(tmp_dir, "RetroPolyEngine UI")
        shutil.copytree(stage_dir, target)
        proc = run(["hdiutil", "create", "-volname", "RetroPolyEngine UI",
                    "-srcfolder", tmp_dir, "-ov", "-format", "UDZO", dmg_path])
        shutil.rmtree(tmp_dir, ignore_errors=True)
        if proc.returncode == 0:
            digest = sha256_file(dmg_path)
            with open(dmg_path + ".sha256", "w", encoding="utf-8", newline="\n") as f:
                f.write("%s  %s\n" % (digest, os.path.basename(dmg_path)))
            archives.append({"path": dmg_path, "sha256": digest,
                             "size": os.path.getsize(dmg_path)})
        else:
            print("UI: hdiutil failed, .dmg skipped", file=sys.stderr)
    return archives


def main():
    parser = argparse.ArgumentParser(description="RetroPolyEngine UI packager")
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--version", default=None)
    parser.add_argument("--output-dir", default=os.path.join(REPO_ROOT, "dist"))
    parser.add_argument("--stage-dir", default=None)
    parser.add_argument("--samples-dir", default=os.path.join(REPO_ROOT,
                                                              "assets", "source"))
    parser.add_argument("--keep-stage", action="store_true")
    args = parser.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    args.version = args.version or read_version(build_dir)
    args.os, args.arch = host_names()
    output_dir = os.path.abspath(args.output_dir)
    os.makedirs(output_dir, exist_ok=True)
    stage_dir = os.path.abspath(args.stage_dir or os.path.join(
        build_dir, "ui-stage-%s-%s" % (args.os, args.arch)))
    if os.path.isdir(stage_dir):
        shutil.rmtree(stage_dir)
    os.makedirs(stage_dir)

    binary = find_binary(build_dir)
    exe_name = "retro-engine-ui.exe" if binary.lower().endswith(".exe") else "retro-engine-ui"
    shutil.copy2(binary, os.path.join(stage_dir, exe_name))
    assets = os.path.join(REPO_ROOT, "tools", "ui_client", "assets")
    if os.path.isdir(assets):
        shutil.copytree(assets, os.path.join(stage_dir, "assets"))
    samples_dir = os.path.abspath(args.samples_dir)
    samples = os.path.join(stage_dir, "samples")
    os.makedirs(samples, exist_ok=True)
    for name in ("cube.gltf", "texture.png"):
        source = os.path.join(samples_dir, name)
        if os.path.isfile(source):
            shutil.copy2(source, os.path.join(samples, name))

    git_sha, git_describe, git_date, dirty = git_info()
    for name in ("LEGAL.md", "LICENSE"):
        source = os.path.join(REPO_ROOT, name)
        if os.path.isfile(source):
            shutil.copy2(source, os.path.join(stage_dir, name))
    write_version(stage_dir, args, git_sha, git_describe, git_date, dirty)
    write_readme(stage_dir, args)
    write_manifest(stage_dir, args, git_sha, git_describe, git_date, dirty)
    write_checksums(stage_dir)

    required = ["retro-engine-ui", "assets/DejaVuSans.ttf",
                "assets/DejaVu-LICENSE.txt", "ui_manifest.json",
                "checksums.sha256", "VERSION.txt", "README-ui.md"]
    for rel in required:
        path = os.path.join(stage_dir, rel)
        if not os.path.isfile(path):
            if rel == "retro-engine-ui" and os.path.isfile(
                    os.path.join(stage_dir, "retro-engine-ui.exe")):
                continue
            raise SystemExit("UI layout: отсутствует %s" % rel)

    archives = make_archives(stage_dir, output_dir, args,
                             epoch_seconds(git_date))
    if not args.keep_stage:
        shutil.rmtree(stage_dir, ignore_errors=True)
    print(json.dumps({"ok": True, "package": PACKAGE, "version": args.version,
                      "os": args.os, "arch": args.arch, "archives": archives},
                     ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
