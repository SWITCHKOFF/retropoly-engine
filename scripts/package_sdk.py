#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/package_sdk.py (Спринт 33–34)
#
# Собирает детерминированный SDK-архив из install-tree:
#   sdk/
#     include/retro_engine/...       публичные заголовки + umbrella + version
#     lib/<platform>/<arch>/...      библиотеки (PC: + SDL3/glad)
#     lib/cmake/RetroPolyEngine/...  find_package(RetroPolyEngine CONFIG)
#     lib/pkgconfig/retro_engine.pc
#     cmake/...                      toolchain wrappers
#     tools/asset_converter[.exe]    host-инструмент (PC)
#     examples/...                   standalone-примеры (find_package)
#     templates/project/...          шаблон проекта
#     VERSION.txt, LEGAL.md, LICENSE, README.md
#     sdk_manifest.json, checksums.sha256
#
# Архивы: retro-engine-sdk-<ver>-<platform>[-<os>]-<arch>.{tar.gz,zip};
# mtime и порядок файлов фиксированы (SOURCE_DATE_EPOCH или дата git-коммита).
#
# Пример:
#   cmake --build build/pc-release --target sdk_package
#   python3 scripts/package_sdk.py --build-dir build/pc-release --platform pc
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
import zipfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK_PACKAGE = "retro-engine-sdk"
COMPONENTS_REQUIRED = [
    "public_headers",
    "libraries",
    "cmake_package",
    "asset_converter",
    "examples",
    "templates",
    "legal_notices",
    "version",
    "checksums",
    "manifest",
]
FORBIDDEN_PARTS = {".git", "build", "CMakeFiles", "bios", "roms", "secrets"}


def run(cmd, cwd=None, check=True):
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
    commit = run(["git", "-C", REPO_ROOT, "rev-parse", "HEAD"], check=False)
    if commit.returncode != 0:
        return "", "", "1970-01-01T00:00:00+00:00", True
    commit_sha = commit.stdout.strip()
    describe = run(["git", "-C", REPO_ROOT, "describe", "--tags", "--always",
                    "--dirty"], check=False).stdout.strip()
    date = run(["git", "-C", REPO_ROOT, "show", "-s", "--format=%cI",
                commit_sha], check=False).stdout.strip()
    dirty = run(["git", "-C", REPO_ROOT, "status", "--porcelain"], check=False)
    return commit_sha, describe, date or "1970-01-01T00:00:00+00:00", bool(dirty.stdout.strip())


def detect_compiler(build_dir):
    cache = os.path.join(build_dir, "CMakeCache.txt")
    compiler = ""
    if os.path.isfile(cache):
        with open(cache, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("CMAKE_CXX_COMPILER:FILEPATH="):
                    compiler = line.split("=", 1)[1].strip()
                    break
    version = ""
    if compiler:
        proc = run([compiler, "--version"], check=False)
        version = (proc.stdout or proc.stderr).splitlines()[0].strip() if (
            proc.stdout or proc.stderr) else ""
    return {"path": compiler, "banner": version}


def parse_compiler(compiler):
    banner = compiler.get("banner", "")
    match = re.search(r"(GNU|Clang|MSVC|Apple clang)[^0-9]*([0-9][0-9.]*)", banner)
    if match:
        return {"id": match.group(1), "version": match.group(2)}
    if "Microsoft" in banner:
        match = re.search(r"([0-9]+\.[0-9]+\.[0-9]+)", banner)
        if match:
            return {"id": "MSVC", "version": match.group(1)}
    return {"id": "unknown", "version": banner or "unknown"}


def platform_defaults(name):
    system = _platform.system().lower()
    machine = _platform.machine().lower()
    if name == "pc":
        os_name = {"linux": "linux", "darwin": "macos", "windows": "windows"}.get(
            system, system)
        arch = "x86_64" if machine in ("x86_64", "amd64") else machine
    elif name == "ps1":
        os_name, arch = "ps1", "mips"
    elif name == "saturn":
        os_name, arch = "saturn", "sh2"
    else:
        raise SystemExit("unknown platform: %s" % name)
    return os_name, arch


def read_version(build_dir):
    cache = os.path.join(build_dir, "CMakeCache.txt")
    if os.path.isfile(cache):
        with open(cache, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
        match = re.search(r"CMAKE_PROJECT_VERSION:STATIC=([0-9.]+)", text)
        if match:
            return match.group(1)
    return "0.2.1"


def stage_install(build_dir, stage_dir, config):
    if os.path.isdir(stage_dir):
        shutil.rmtree(stage_dir)
    os.makedirs(stage_dir, exist_ok=True)
    cmd = ["cmake", "--install", build_dir, "--prefix", stage_dir]
    if config:
        cmd += ["--config", config]
    run(cmd)


def relocate_layout(stage_dir, platform):
    bin_dir = os.path.join(stage_dir, "bin")
    tools_dir = os.path.join(stage_dir, "tools")
    if os.path.isdir(bin_dir) and os.listdir(bin_dir):
        os.makedirs(tools_dir, exist_ok=True)
        for entry in os.listdir(bin_dir):
            shutil.move(os.path.join(bin_dir, entry), os.path.join(tools_dir, entry))
        os.rmdir(bin_dir)
    # asset_converter — единственный обязательный host-инструмент PC SDK.
    if platform == "pc":
        converter = [n for n in os.listdir(tools_dir)] if os.path.isdir(tools_dir) else []
        if not any(n.startswith("asset_converter") for n in converter):
            raise SystemExit("SDK: asset_converter отсутствует в tools/")


def write_version_txt(stage_dir, args, git_commit, git_describe, git_date,
                      dirty, compiler):
    target = os.path.join(stage_dir, "VERSION.txt")
    with open(target, "w", encoding="utf-8", newline="\n") as f:
        f.write("# RetroPolyEngine SDK\n")
        f.write("name=RetroPolyEngine\n")
        f.write("version=%s\n" % args.version)
        f.write("package=%s\n" % SDK_PACKAGE)
        f.write("platform=%s\n" % args.platform)
        f.write("os=%s\n" % args.os)
        f.write("arch=%s\n" % args.arch)
        f.write("git_commit=%s\n" % (git_commit or "unknown"))
        f.write("git_describe=%s\n" % (git_describe or "unknown"))
        f.write("git_dirty=%s\n" % ("true" if dirty else "false"))
        f.write("commit_date=%s\n" % git_date)
        f.write("build_type=%s\n" % args.build_type)
        f.write("compiler=%s\n" % compiler.get("banner", ""))
        f.write("cxx_standard=c++14\n")
        f.write("endianness=little\n")
        f.write("pointer_bits=%s\n" % ("64" if sys.maxsize > 2**32 else "32"))


def write_readme(stage_dir, args):
    target = os.path.join(stage_dir, "README.md")
    body = """# RetroPolyEngine SDK {version}

Публичный SDK движка RetroPolyEngine ({platform}/{arch}).

## Быстрый старт

```sh
# 1. Проверить версию/манифест
cat VERSION.txt
cat sdk_manifest.json

# 2. Собрать примеры из SDK
cmake --preset sdk-release
cmake --build --preset sdk-release

# 3. Или подключить SDK к своему проекту
cmake -S /path/to/project -B build -DCMAKE_PREFIX_PATH={sdk_path}
```

```cmake
find_package(RetroPolyEngine CONFIG REQUIRED)
target_link_libraries(app PRIVATE RetroPolyEngine::retro_engine)
```

```cpp
#include <retro_engine/retro_engine.h>
```

Публичный API: `include/retro_engine/retro_engine.h`; внутренние заголовки
(`render/*`, `hal/pc|ps1|saturn/*`) в SDK не поставляются. Полная
документация — `docs/SDK.md` в репозитории движка.

## Проверка целостности

```sh
sha256sum -c checksums.sha256
```
""".format(version=args.version, platform=args.platform, arch=args.arch,
           sdk_path="/path/to/retro-engine-sdk")
    with open(target, "w", encoding="utf-8", newline="\n") as f:
        f.write(body)


def write_examples_root(stage_dir):
    cmake = """cmake_minimum_required(VERSION 3.25)
project(retro_engine_sdk_examples LANGUAGES CXX)
find_package(RetroPolyEngine CONFIG REQUIRED)
foreach(_example hello_triangle rotating_cube hello_triangle_ps1 hello_triangle_saturn)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/${_example}/CMakeLists.txt")
        add_subdirectory("examples/${_example}")
    endif()
endforeach()
"""
    with open(os.path.join(stage_dir, "CMakeLists.txt"), "w",
              encoding="utf-8", newline="\n") as f:
        f.write(cmake)
    presets = {
        "version": 6,
        "configurePresets": [{
            "name": "sdk-release",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "CMAKE_PREFIX_PATH": "${sourceDir}",
            },
        }],
        "buildPresets": [{"name": "sdk-release", "configurePreset": "sdk-release"}],
    }
    with open(os.path.join(stage_dir, "CMakePresets.json"), "w",
              encoding="utf-8", newline="\n") as f:
        json.dump(presets, f, ensure_ascii=False, indent=2, sort_keys=True)
        f.write("\n")


def copy_legal(stage_dir):
    for name in ("LEGAL.md", "LICENSE"):
        src = os.path.join(REPO_ROOT, name)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(stage_dir, name))
    docs_legal = os.path.join(REPO_ROOT, "docs", "LEGAL.md")
    if os.path.isfile(docs_legal):
        shutil.copy2(docs_legal, os.path.join(stage_dir, "LEGAL-docs.md"))


def iter_files(root):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if d not in FORBIDDEN_PARTS)
        for name in sorted(filenames):
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            yield path, rel


def assert_layout(stage_dir, args):
    required = [
        "include/retro_engine/retro_engine.h",
        "include/retro_engine/version.h",
        "include/retro_engine/hal/platform.h",
        "include/retro_engine/core/math/fixed.h",
        "include/retro_engine/assets/assets.h",
        "lib/cmake/RetroPolyEngine/RetroPolyEngineConfig.cmake",
        "lib/cmake/RetroPolyEngine/RetroPolyEngineTargets.cmake",
        "lib/cmake/RetroPolyEngine/RetroPolyEngineConfigVersion.cmake",
        "templates/project/CMakeLists.txt",
    ]
    for rel in required:
        if not os.path.isfile(os.path.join(stage_dir, rel)):
            raise SystemExit("SDK layout: отсутствует %s" % rel)

    libdir = os.path.join(stage_dir, "lib", args.platform, args.arch)
    if not os.path.isdir(libdir) or not any(
            n.startswith("libretro_engine") or n.startswith("retro_engine")
            for n in os.listdir(libdir)):
        raise SystemExit("SDK layout: нет библиотек в lib/%s/%s" %
                         (args.platform, args.arch))

    if args.platform == "pc":
        for rel in ("include/retro_engine/retro_engine.h",
                    "include/retro_engine/hal/platform.h",
                    "include/retro_engine/assets/assets.h"):
            text = open(os.path.join(stage_dir, rel), encoding="utf-8",
                        errors="replace").read()
            for forbidden in ("#include <SDL", 'include "SDL', "glad/gl.h",
                              "psx", "jo/jo.h"):
                if forbidden in text:
                    raise SystemExit(
                        "SDK public header %s тянет третью сторону: %s" %
                        (rel, forbidden))

    # Internal-заголовки не должны попасть в SDK.
    forbidden_headers = [
        "include/retro_engine/render",
        "include/retro_engine/hal/pc",
        "include/retro_engine/hal/ps1",
        "include/retro_engine/hal/saturn",
    ]
    for rel in forbidden_headers:
        if os.path.exists(os.path.join(stage_dir, rel)):
            raise SystemExit("SDK layout: internal-заголовки попали в %s" % rel)


def build_manifest(stage_dir, args, git_commit, git_describe, git_date, dirty,
                   compiler):
    artifacts = []
    for path, rel in iter_files(stage_dir):
        if rel in ("sdk_manifest.json", "checksums.sha256"):
            continue
        artifacts.append({"path": rel, "size": os.path.getsize(path),
                          "sha256": sha256_file(path)})
    examples = sorted({
        rel.split("/")[1] for _, rel in iter_files(stage_dir)
        if rel.startswith("examples/") and rel.count("/") >= 2
    })
    manifest = {
        "schema": 1,
        "package": SDK_PACKAGE,
        "name": "RetroPolyEngine",
        "version": args.version,
        "git_commit": git_commit or "unknown",
        "git_describe": git_describe or "unknown",
        "git_dirty": dirty,
        "commit_date": git_date,
        "platform": args.platform,
        "os": args.os,
        "arch": args.arch,
        "build_type": args.build_type,
        "compiler": parse_compiler(compiler),
        "compiler_banner": compiler.get("banner", ""),
        "abi": {
            "cxx_standard": "c++14",
            "endianness": "little",
            "pointer_bits": 64 if sys.maxsize > 2**32 else 32,
        },
        "public_header": "include/retro_engine/retro_engine.h",
        "targets": [
            "RetroPolyEngine::retro_engine",
            "RetroPolyEngine::hal",
            "RetroPolyEngine::log",
        ],
        "components": {
            name: (name != "asset_converter" or args.platform == "pc")
            for name in COMPONENTS_REQUIRED
        },
        "examples": examples,
        "artifacts": artifacts,
    }
    with open(os.path.join(stage_dir, "sdk_manifest.json"), "w",
              encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2, sort_keys=True)
        f.write("\n")
    return manifest


def write_checksums(stage_dir):
    lines = []
    for path, rel in iter_files(stage_dir):
        if rel == "checksums.sha256":
            continue
        lines.append("%s  %s" % (sha256_file(path), rel))
    with open(os.path.join(stage_dir, "checksums.sha256"), "w",
              encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def _archive_mode(rel):
    # Host-инструменты должны оставаться исполняемыми при распаковке zip/tar.
    if rel.startswith("tools/") and not rel.endswith(".exe"):
        return 0o755
    return 0o644


def epoch_seconds(git_date):
    if os.environ.get("SOURCE_DATE_EPOCH"):
        return int(os.environ["SOURCE_DATE_EPOCH"])
    try:
        return int(_dt.datetime.fromisoformat(git_date).timestamp())
    except (ValueError, TypeError):
        return 0


def make_archive(stage_dir, output_dir, args, epoch):
    parts = [SDK_PACKAGE, args.version, args.platform]
    if args.os != args.platform:
        parts.append(args.os)
    parts.append(args.arch)
    base = "-".join(parts)
    tar_path = os.path.join(output_dir, base + ".tar.gz")
    zip_path = os.path.join(output_dir, base + ".zip")

    files = list(iter_files(stage_dir))
    files.sort(key=lambda item: item[1])

    # Deterministic gzip header (mtime/name) + PAX tar entries.
    with open(tar_path, "wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=epoch,
                           compresslevel=9) as gz:
            with tarfile.open(fileobj=gz, mode="w",
                              format=tarfile.PAX_FORMAT) as tar:
                for path, rel in files:
                    info = tar.gettarinfo(path, arcname="sdk/" + rel)
                    info.mtime = epoch
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    info.mode = _archive_mode(rel)
                    with open(path, "rb") as f:
                        tar.addfile(info, f)

    zip_date = _dt.datetime.fromtimestamp(max(epoch, 315532800),
                                          tz=_dt.timezone.utc)
    date_time = (zip_date.year, zip_date.month, zip_date.day,
                 zip_date.hour, zip_date.minute, zip_date.second)
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9) as zf:
        for path, rel in files:
            info = zipfile.ZipInfo("sdk/" + rel, date_time=date_time)
            info.external_attr = (_archive_mode(rel) & 0xFFFF) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as f:
                zf.writestr(info, f.read())

    archives = []
    for path in (tar_path, zip_path):
        digest = sha256_file(path)
        sidecar = path + ".sha256"
        with open(sidecar, "w", encoding="utf-8", newline="\n") as f:
            f.write("%s  %s\n" % (digest, os.path.basename(path)))
        archives.append({"path": path, "sha256": digest,
                         "size": os.path.getsize(path)})
    return archives


def main():
    parser = argparse.ArgumentParser(description="RetroPolyEngine SDK packager")
    parser.add_argument("--build-dir", required=True,
                        help="build/<preset> с установленными целями")
    parser.add_argument("--platform", choices=["pc", "ps1", "saturn"],
                        default="pc")
    parser.add_argument("--arch", default=None)
    parser.add_argument("--version", default=None)
    parser.add_argument("--output-dir", default=os.path.join(REPO_ROOT, "dist"))
    parser.add_argument("--stage-dir", default=None)
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--config", default=None,
                        help="multi-config generator config (например Release)")
    parser.add_argument("--skip-install", action="store_true")
    parser.add_argument("--keep-stage", action="store_true")
    parser.add_argument("--no-archive", action="store_true")
    args = parser.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    args.version = args.version or read_version(build_dir)
    default_os, default_arch = platform_defaults(args.platform)
    args.arch = args.arch or default_arch
    # platform в манифесте — целевая платформа; os отдельно.
    args.os = default_os
    args.stage_dir = os.path.abspath(args.stage_dir or os.path.join(
        build_dir, "sdk-stage-%s-%s" % (args.platform, args.arch)))
    output_dir = os.path.abspath(args.output_dir)
    os.makedirs(output_dir, exist_ok=True)

    git_commit, git_describe, git_date, dirty = git_info()
    compiler = detect_compiler(build_dir)

    if not args.skip_install:
        stage_install(build_dir, args.stage_dir, args.config)
    if not os.path.isdir(args.stage_dir):
        raise SystemExit("SDK: stage-каталог не найден: %s" % args.stage_dir)

    relocate_layout(args.stage_dir, args.platform)
    write_version_txt(args.stage_dir, args, git_commit, git_describe, git_date,
                      dirty, compiler)
    write_readme(args.stage_dir, args)
    write_examples_root(args.stage_dir)
    copy_legal(args.stage_dir)
    assert_layout(args.stage_dir, args)

    manifest = build_manifest(args.stage_dir, args, git_commit, git_describe,
                              git_date, dirty, compiler)
    write_checksums(args.stage_dir)

    result = {
        "ok": True,
        "package": SDK_PACKAGE,
        "version": args.version,
        "platform": args.platform,
        "arch": args.arch,
        "stage_dir": args.stage_dir,
        "manifest": os.path.join(args.stage_dir, "sdk_manifest.json"),
        "files": len(manifest["artifacts"]),
        "archives": [],
    }
    if not args.no_archive:
        epoch = epoch_seconds(git_date)
        result["archives"] = make_archive(args.stage_dir, output_dir, args, epoch)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
