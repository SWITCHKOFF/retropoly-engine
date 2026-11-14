#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/build_release_bundle.py (Спринт 37–38)
#
# Собирает публичный payload v0.2.1 из готовых архивов SDK/UI и отчёта gate:
#
#   dist/retro-engine-v0.2.1/
#     sdk/                        # распакованный PC SDK
#     ui/                         # распакованный UI client (host)
#     demos/                      # OPTIONAL: PC/PS1/Saturn demo-образы
#     checksums.sha256            # все файлы payload (кроме себя)
#     release_contract_report.json
#     README.md LICENSE LEGAL.md
#   dist/retro-engine-v0.2.1.tar.gz (+ .sha256)
#
# Demo-образы не могут подменить SDK/UI: без sdk/ и ui/ сборка падает.
#
# Пример:
#   python3 scripts/build_release_bundle.py \
#       --sdk-archive dist/retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz \
#       --ui-archive dist/retro-engine-ui-0.2.1-linux-x86_64.tar.gz \
#       --report release_contract_report.json
# ============================================================================
import argparse
import datetime as _dt
import gzip
import json
import os
import platform as _platform
import shutil
import sys
import tarfile
import tempfile
import zipfile

import yaml

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTRACT_PATH = os.path.join(REPO_ROOT, "ci", "normative_contracts.yaml")
DIST_DIR = os.path.join(REPO_ROOT, "dist")
DEFAULT_REPORT = os.path.join(REPO_ROOT, "release_contract_report.json")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import release_payload  # noqa: E402


def load_contract():
    with open(CONTRACT_PATH, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


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


def extract(archive, dest, root_name):
    os.makedirs(dest, exist_ok=True)
    if archive.endswith((".tar.gz", ".tgz")):
        with tarfile.open(archive, "r:gz") as tar:
            try:
                tar.extractall(dest, filter="data")
            except TypeError:  # Python < 3.12
                tar.extractall(dest)
    elif archive.endswith(".zip"):
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(dest)
    else:
        raise SystemExit("unsupported archive: %s" % archive)
    root = os.path.join(dest, root_name)
    if not os.path.isdir(root):
        raise SystemExit("архив %s не содержит %s/" % (archive, root_name))
    return root


def verify_sidecar(archive):
    sidecar = archive + ".sha256"
    if not os.path.isfile(sidecar):
        return False, {"archive": archive, "error": "sidecar .sha256 missing"}
    parts = open(sidecar, "r", encoding="utf-8").read().split()
    if not parts:
        return False, {"archive": archive, "error": "sidecar .sha256 empty"}
    expected = parts[0]
    actual = release_payload.sha256_file(archive)
    ok = expected == actual
    return ok, {"archive": os.path.basename(archive), "sha256": actual,
                "expected": expected, "ok": ok}


def find_archive(version, kind, explicit, host_os, host_arch):
    if explicit:
        path = os.path.abspath(explicit)
        if not os.path.isfile(path):
            raise SystemExit("%s archive not found: %s" % (kind, path))
        return path
    if not os.path.isdir(DIST_DIR):
        raise SystemExit("dist/ not found; build SDK/UI packages first")
    if kind == "sdk":
        pattern = "retro-engine-sdk-%s-pc-%s-%s.tar.gz" % (
            version, host_os, host_arch)
        candidates = sorted(
            os.path.join(DIST_DIR, name) for name in os.listdir(DIST_DIR)
            if name.startswith("retro-engine-sdk-%s-pc-" % version) and
            name.endswith(".tar.gz"))
    else:
        pattern = "retro-engine-ui-%s-%s-%s.tar.gz" % (
            version, host_os, host_arch)
        candidates = sorted(
            os.path.join(DIST_DIR, name) for name in os.listdir(DIST_DIR)
            if name.startswith("retro-engine-ui-%s-" % version) and
            name.endswith(".tar.gz"))
    preferred = os.path.join(DIST_DIR, pattern)
    if os.path.isfile(preferred):
        return preferred
    if candidates:
        return candidates[0]
    raise SystemExit(
        "%s archive not found in dist/ for %s-%s; pass --%s-archive" %
        (kind.upper(), host_os, host_arch, kind))


def copy_demos(demos_dir, target):
    if not demos_dir:
        return None
    demos_dir = os.path.abspath(demos_dir)
    if not os.path.isdir(demos_dir):
        raise SystemExit("demos dir not found: %s" % demos_dir)
    shutil.copytree(demos_dir, target, dirs_exist_ok=True)
    readme = os.path.join(target, "README.md")
    if not os.path.isfile(readme):
        with open(readme, "w", encoding="utf-8", newline="\n") as f:
            f.write(
                "# Demo images (optional)\n\n"
                "Эти образы — только примеры для эмуляторов. Публичный релиз\n"
                "состоит из `sdk/` и `ui/`; demo-образы не заменяют SDK/UI.\n"
                "BIOS/ROM в payload не входят.\n")
    return sorted(
        os.path.relpath(os.path.join(d, n), target).replace(os.sep, "/")
        for d, _, names in os.walk(target) for n in names)


def epoch_seconds():
    if os.environ.get("SOURCE_DATE_EPOCH"):
        return int(os.environ["SOURCE_DATE_EPOCH"])
    proc = shutil.which("git")
    if proc:
        import subprocess
        result = subprocess.run(
            ["git", "-C", REPO_ROOT, "show", "-s", "--format=%ct", "HEAD"],
            capture_output=True, text=True)
        if result.returncode == 0:
            try:
                return int(result.stdout.strip())
            except ValueError:
                pass
    return 0


def make_archive(bundle_dir, output_dir, version, epoch):
    base = "retro-engine-v%s" % version
    archive = os.path.join(output_dir, base + ".tar.gz")
    files = sorted(list(release_payload.iter_files(bundle_dir)),
                   key=lambda item: item[1])
    with open(archive, "wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=epoch,
                           compresslevel=9) as gz:
            with tarfile.open(fileobj=gz, mode="w",
                              format=tarfile.PAX_FORMAT) as tar:
                for path, rel in files:
                    info = tar.gettarinfo(path, arcname=base + "/" + rel)
                    info.mtime = epoch
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    mode = 0o755 if os.access(path, os.X_OK) else 0o644
                    info.mode = mode
                    with open(path, "rb") as f:
                        tar.addfile(info, f)
    digest = release_payload.sha256_file(archive)
    with open(archive + ".sha256", "w", encoding="utf-8", newline="\n") as f:
        f.write("%s  %s\n" % (digest, os.path.basename(archive)))
    return {"path": archive, "sha256": digest,
            "size": os.path.getsize(archive)}


def main():
    parser = argparse.ArgumentParser(
        description="Сборка публичного release payload SDK+UI")
    parser.add_argument("--version", default=None,
                        help="Версия (default: ci/normative_contracts.yaml)")
    parser.add_argument("--output-dir", default=DIST_DIR)
    parser.add_argument("--sdk-archive", default=None)
    parser.add_argument("--ui-archive", default=None)
    parser.add_argument("--report", default=DEFAULT_REPORT)
    parser.add_argument("--demos-dir", default=None,
                        help="OPTIONAL каталог demo-образов → payload/demos/")
    parser.add_argument("--host-os", default=None,
                        help="linux|windows|macos (default: текущий host)")
    parser.add_argument("--host-arch", default=None)
    parser.add_argument("--no-archive", action="store_true")
    parser.add_argument("--keep-temp", action="store_true")
    args = parser.parse_args()

    contract = load_contract()
    version = str(args.version or contract["project"]["version"])
    expected = str(contract["project"]["version"])
    if version != expected:
        raise SystemExit("version %s != normative %s" % (version, expected))

    host_os, host_arch = host_names()
    host_os = args.host_os or host_os
    host_arch = args.host_arch or host_arch

    sdk_archive = find_archive(version, "sdk", args.sdk_archive,
                               host_os, host_arch)
    ui_archive = find_archive(version, "ui", args.ui_archive,
                              host_os, host_arch)
    for archive in (sdk_archive, ui_archive):
        ok, detail = verify_sidecar(archive)
        if not ok:
            raise SystemExit("checksum sidecar failed: %s" %
                             json.dumps(detail, ensure_ascii=False))

    report_path = os.path.abspath(args.report)
    if not os.path.isfile(report_path):
        raise SystemExit("release gate report not found: %s\n"
                         "Сначала: python3 scripts/verify_release_contract.py "
                         "--skip-payload" % report_path)
    try:
        with open(report_path, "r", encoding="utf-8") as f:
            report = json.load(f)
    except (OSError, ValueError) as exc:
        raise SystemExit("release report invalid: %s" % exc)
    if report.get("all_ok") is not True:
        raise SystemExit("release gate report all_ok is not true")
    if str(report.get("version")) != version:
        raise SystemExit("release report version %s != %s" %
                         (report.get("version"), version))

    output_dir = os.path.abspath(args.output_dir)
    os.makedirs(output_dir, exist_ok=True)
    bundle_name = "retro-engine-v%s" % version
    bundle_dir = os.path.join(output_dir, bundle_name)
    if os.path.isdir(bundle_dir):
        shutil.rmtree(bundle_dir)
    os.makedirs(bundle_dir)

    tempdir = tempfile.mkdtemp(prefix="rpe_bundle_")
    try:
        sdk_root = extract(sdk_archive, os.path.join(tempdir, "sdk"), "sdk")
        ui_root = extract(ui_archive, os.path.join(tempdir, "ui"), "ui")
        shutil.copytree(sdk_root, os.path.join(bundle_dir, "sdk"))
        shutil.copytree(ui_root, os.path.join(bundle_dir, "ui"))
    finally:
        if not args.keep_temp:
            shutil.rmtree(tempdir, ignore_errors=True)

    shutil.copy2(report_path,
                 os.path.join(bundle_dir, "release_contract_report.json"))
    for name in ("README.md", "LICENSE", "LEGAL.md"):
        source = os.path.join(REPO_ROOT, name)
        if not os.path.isfile(source):
            raise SystemExit("bundle source missing: %s" % source)
        shutil.copy2(source, os.path.join(bundle_dir, name))
    demos = copy_demos(args.demos_dir, os.path.join(bundle_dir, "demos"))

    checksum_count = release_payload.write_checksums(bundle_dir)
    ok, details = release_payload.validate_payload(bundle_dir, contract)
    details["sdk_archive"] = os.path.basename(sdk_archive)
    details["ui_archive"] = os.path.basename(ui_archive)
    details["host"] = "%s-%s" % (host_os, host_arch)
    details["demos"] = demos
    details["checksum_entries"] = checksum_count
    if not ok:
        print(json.dumps({"ok": False, "bundle": bundle_dir,
                          "details": details}, ensure_ascii=False, indent=2))
        return 1

    result = {"ok": True, "bundle": bundle_dir,
              "version": version, "host": details["host"],
              "sdk_archive": details["sdk_archive"],
              "ui_archive": details["ui_archive"],
              "demos": demos, "checksum_entries": checksum_count,
              "archive": None}
    if not args.no_archive:
        result["archive"] = make_archive(bundle_dir, output_dir, version,
                                         epoch_seconds())
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
