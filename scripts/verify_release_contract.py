#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# RetroPolyEngine — scripts/verify_release_contract.py
#
# Release gate v2: legacy-контракты + SDK (Спринт 33–34).
#   1. presets == {pc-debug, pc-release, pc-debug-render, ps1-release, ...}
#   2. ps1_executable_size <= 512000
#   3. ps1_peak_main_ram <= 1572864; ps1_peak_vram <= 819200
#   4. w0_framebuffer_hash == canonical_w0_hash
#   5. w1_pc_fps >= 60; w1_ps1_emulator_fps >= 30
#   6. all_asset_validation_cases_pass; no_renderer_overflow_is_silent
#   7. Saturn artifacts/map/golden
#   8. SDK: архивы + sidecar SHA-256, sdk_manifest.json, public headers,
#      checksums.sha256 и clean consumer smoke (find_package RetroPolyEngine)
#   9. Публикация результата как CI-artifact (release_contract_report.json)
#
# Вызов:
#   python3 scripts/verify_release_contract.py [--w1-seconds 60] \
#       [--sdk-archive dist/...tar.gz | --sdk-dir path]
#
# Требования: собранные пресеты pc-debug, pc-debug-render, pc-release,
# ps1-release, saturn-release в канонических каталогах build/<preset>
# и SDK-архивы в dist/.

import argparse
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

import yaml

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTRACT_PATH = os.path.join(REPO_ROOT, "ci", "normative_contracts.yaml")
PRESETS_PATH = os.path.join(REPO_ROOT, "CMakePresets.json")
REPORT_PATH = os.path.join(REPO_ROOT, "release_contract_report.json")
REFERENCE_ENV_PATH = os.path.join(
    REPO_ROOT, "docs", "benchmarks", "reference_environment.yaml")

PS1_EXE = os.path.join(REPO_ROOT, "build", "ps1-release", "src", "hal", "ps1",
                       "retro_ps1.exe")
PC_DEBUG_TESTS = os.path.join(REPO_ROOT, "build", "pc-debug", "tests",
                              "retro_engine_tests")
PC_RENDER_TESTS = os.path.join(REPO_ROOT, "build", "pc-debug-render", "tests",
                               "retro_engine_visual_tests")
W1_BENCH = os.path.join(REPO_ROOT, "build", "pc-release", "game",
                        "rotating_cube")
SATURN_DIR = os.path.join(REPO_ROOT, "build", "saturn-release", "src",
                          "hal", "saturn")
SATURN_REQUIRED = ["hello_triangle.elf", "hello_triangle.bin",
                   "hello_triangle.iso", "hello_triangle.cue",
                   "hello_triangle.map"]
DIST_DIR = os.path.join(REPO_ROOT, "dist")
SDK_CONSUMER_SCRIPT = os.path.join(REPO_ROOT, "scripts",
                                   "sdk_consumer_smoke.py")


def load_contract():
    with open(CONTRACT_PATH, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def load_presets():
    with open(PRESETS_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


def run(cmd, cwd=REPO_ROOT, timeout=180):
    try:
        proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                              timeout=timeout)
        return proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "timeout after %ss" % timeout


def assert_presets(contract):
    expected = set(contract["presets"]["canonical"])
    presets = load_presets()
    actual = set(p["name"] for p in presets["configurePresets"]
                 if not p.get("hidden", False))
    missing = expected - actual
    ok = not missing
    return ok, {"expected": sorted(expected), "actual": sorted(actual),
                "missing": sorted(missing), "optional_present": sorted(actual - expected)}


def assert_ps1_exe_size(contract):
    limit = contract["release"]["ps1_exe_max_bytes"]
    if not os.path.isfile(PS1_EXE):
        return False, {"path": PS1_EXE, "size": None, "limit": limit,
                       "error": "PS-EXE not found"}
    size = os.path.getsize(PS1_EXE)
    return size <= limit, {"path": PS1_EXE, "size": size, "limit": limit}


def assert_ps1_ram_vram(contract):
    # MVP: значения берутся из reference_environment.yaml (ручной замер).
    # Скрипт проверяет, что задокументированные значения укладываются в
    # бюджет; полный автоматический замер в CI не выполняется.
    budgets = contract["memory_budgets_ps1"]
    ram_limit = budgets["peak_main_ram_bytes"]
    vram_limit = budgets["peak_vram_bytes"]

    if not os.path.isfile(REFERENCE_ENV_PATH):
        return False, {"error": "reference_environment.yaml not found",
                       "ram_limit": ram_limit, "vram_limit": vram_limit}

    with open(REFERENCE_ENV_PATH, "r", encoding="utf-8") as f:
        ref = yaml.safe_load(f)

    ps1 = ref.get("ps1_emulator", {})
    measurement = ps1.get("measurement", "")
    ram = _parse_field_bytes(measurement, "RAM:")
    vram = _parse_field_bytes(measurement, "VRAM:")

    ok = (ram is not None and vram is not None and
          ram <= ram_limit and vram <= vram_limit)
    return ok, {"ram": ram, "ram_limit": ram_limit,
                "vram": vram, "vram_limit": vram_limit,
                "source": "docs/benchmarks/reference_environment.yaml"}


def _parse_field_bytes(text, prefix):
    text = str(text)
    idx = text.find(prefix)
    if idx < 0:
        return None
    end = text.find("\n", idx)
    line = text[idx:end] if end >= 0 else text[idx:]
    return _parse_bytes(line)


def _parse_bytes(text):
    text = str(text).lower()
    m = re.search(r"([0-9]+)\s*ki?b", text)
    if m:
        return int(m.group(1)) * 1024
    m = re.search(r"([0-9]+)\s*bytes", text)
    if m:
        return int(m.group(1))
    m = re.search(r"([0-9]+)\s*b", text)
    if m:
        return int(m.group(1))
    return None


def assert_w0_hash(contract):
    # Запускаем визуальные тесты W0Golden; успех = hash-эталон совпал.
    if not os.path.isfile(PC_RENDER_TESTS):
        return False, {"error": "visual tests binary not found", "ok": False}
    cmd = [PC_RENDER_TESTS, "--gtest_filter=W0Golden.*"]
    if not os.environ.get("DISPLAY"):
        cmd = ["xvfb-run", "-a"] + cmd
    rc, out, err = run(cmd, timeout=120)
    ok = rc == 0
    return ok, {"exit_code": rc, "ok": ok,
                "tail": (err if err else out)[-800:]}


def assert_w1_pc_fps(contract, seconds):
    if not os.path.isfile(W1_BENCH):
        return False, {"error": "bench binary not found", "avg_fps": None}

    with tempfile.TemporaryDirectory(prefix="rpe_contract_") as tmp:
        rc, out, err = run([W1_BENCH, str(seconds)], cwd=tmp, timeout=seconds + 30)
        log = os.path.join(tmp, "retro_engine.log")
        avg = None
        if os.path.isfile(log):
            with open(log, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
            m = re.search(r"среднее fps=([0-9]+)\.([0-9]{2})", text)
            if m:
                avg = float("%s.%s" % (m.group(1), m.group(2)))
        ok = (rc == 0 and avg is not None and
              avg >= contract["workloads"]["w1"]["pc_min_fps"])
        return ok, {"exit_code": rc, "avg_fps": avg,
                    "min_fps": contract["workloads"]["w1"]["pc_min_fps"],
                    "measured_seconds": seconds,
                    "log_tail": (out + err)[-400:]}


def assert_w1_ps1_fps(contract):
    # PS1 FPS — ручной замер в DuckStation, задокументированный в
    # reference_environment.yaml. Скрипт проверяет наличие записи.
    if not os.path.isfile(REFERENCE_ENV_PATH):
        return False, {"error": "reference_environment.yaml not found"}
    with open(REFERENCE_ENV_PATH, "r", encoding="utf-8") as f:
        ref = yaml.safe_load(f)
    ps1 = ref.get("ps1_emulator", {})
    measurement = ps1.get("measurement", "")
    m = re.search(r"fps\s*([0-9]+(?:\.[0-9]+)?)", measurement.lower())
    avg = float(m.group(1)) if m else None
    min_fps = contract["workloads"]["w1"]["ps1_emulator_min_fps"]
    ok = avg is not None and avg >= min_fps
    return ok, {"avg_fps": avg, "min_fps": min_fps,
                "source": "docs/benchmarks/reference_environment.yaml"}


def assert_saturn_artifacts(contract):
    saturn = contract["saturn"]
    limits = saturn["budgets"]
    files = {}
    ok = True
    for name in contract["release"]["required_artifacts_saturn"]:
        path = os.path.join(SATURN_DIR, name)
        exists = os.path.isfile(path)
        size = os.path.getsize(path) if exists else None
        files[name] = {"path": path, "size": size}
        ok = ok and exists
    if not ok:
        return False, {"files": files, "error": "missing Saturn artifact"}

    # hello_triangle.* должны быть байт-идентичны retro_saturn.*
    pairs = [("hello_triangle.iso", "retro_saturn.iso"),
             ("hello_triangle.bin", "retro_saturn.bin")]
    for hello, legacy in pairs:
        a = os.path.join(SATURN_DIR, hello)
        b = os.path.join(SATURN_DIR, legacy)
        same = os.path.isfile(b) and open(a, "rb").read() == open(b, "rb").read()
        files[hello]["same_as_" + legacy] = same
        ok = ok and same

    checks = [
        ("hello_triangle.elf", "hello_triangle_elf_max_bytes"),
        ("hello_triangle.bin", "hello_triangle_bin_max_bytes"),
        ("hello_triangle.iso", "hello_triangle_iso_max_bytes"),
    ]
    for name, limit_key in checks:
        limit = limits[limit_key]
        size = files[name]["size"]
        within = size is not None and size <= limit
        files[name]["limit"] = limit
        files[name]["within_limit"] = within
        ok = ok and within
    return ok, {"files": files}


def assert_saturn_map_budget(contract):
    path = os.path.join(SATURN_DIR, "hello_triangle.map")
    if not os.path.isfile(path):
        return False, {"error": "hello_triangle.map not found", "path": path}
    text = open(path, "r", encoding="utf-8", errors="replace").read()
    m = re.search(r"0x([0-9a-fA-F]+)\s+__jo_work_end", text)
    if not m:
        return False, {"error": "__jo_work_end not found"}
    work_end = int(m.group(1), 16)
    stack_top = int(contract["saturn"]["budgets"]["master_stack_top"])
    ok = work_end <= stack_top
    return ok, {"__jo_work_end": hex(work_end),
                "master_stack_top": hex(stack_top),
                "headroom_bytes": stack_top - work_end}


def assert_saturn_golden(contract):
    golden = contract["saturn"]["golden"]
    path = os.path.join(REPO_ROOT, golden["screenshot"])
    if not os.path.isfile(path):
        return False, {"error": "Saturn golden screenshot not found",
                       "path": path}
    digest = hashlib.sha256(open(path, "rb").read()).hexdigest()
    ok = digest == golden["sha256"]
    return ok, {"path": golden["screenshot"], "sha256": digest,
                "expected": golden["sha256"]}


def assert_asset_validation():
    if not os.path.isfile(PC_DEBUG_TESTS):
        return False, {"error": "unit tests binary not found", "ok": False}
    suites = ["MeshLoader.*", "TextureLoader.*", "TexContract.*"]
    cmd = [PC_DEBUG_TESTS, "--gtest_filter=" + ":".join(suites)]
    rc, out, err = run(cmd, timeout=120)
    ok = rc == 0
    return ok, {"exit_code": rc, "ok": ok,
                "tail": (err if err else out)[-600:]}


def assert_renderer_overflow():
    if not os.path.isfile(PC_DEBUG_TESTS):
        return False, {"error": "unit tests binary not found", "ok": False}
    cmd = [PC_DEBUG_TESTS, "--gtest_filter=CommandBufferOverflow.*"]
    rc, out, err = run(cmd, timeout=60)
    ok = rc == 0
    return ok, {"exit_code": rc, "ok": ok,
                "tail": (err if err else out)[-600:]}


# ---------------------------------------------------------------------------
# SDK (Спринт 33–34): архив, manifest, public headers, clean consumer smoke.
# ---------------------------------------------------------------------------

def _extract_sdk(archive, dest):
    if archive.endswith(".zip"):
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(dest)
    else:
        with tarfile.open(archive) as tf:
            try:
                tf.extractall(dest, filter="data")
            except TypeError:  # Python < 3.12
                tf.extractall(dest)
    root = os.path.join(dest, "sdk")
    return root if os.path.isdir(root) else dest


def _sdk_platform(path, sdk_dir=None):
    manifest = os.path.join(sdk_dir or "", "sdk_manifest.json")
    if os.path.isfile(manifest):
        try:
            with open(manifest, "r", encoding="utf-8") as f:
                return json.load(f).get("platform", "pc")
        except (OSError, ValueError):
            pass
    base = os.path.basename(path or "")
    for platform in ("ps1", "saturn", "pc"):
        if "-%s-" % platform in base:
            return platform
    return "pc"


def _find_sdk(args, tempdir):
    if args.sdk_dir:
        sdk_dir = os.path.abspath(args.sdk_dir)
        return sdk_dir, None, _sdk_platform(sdk_dir, sdk_dir)
    if args.sdk_archive:
        archive = os.path.abspath(args.sdk_archive)
        return _extract_sdk(archive, tempdir), archive, \
            _sdk_platform(archive)
    candidates = sorted(glob.glob(os.path.join(DIST_DIR,
                                               "retro-engine-sdk-*.tar.gz")))
    pc_candidates = [c for c in candidates if "-pc-" in os.path.basename(c)]
    if pc_candidates:
        return _extract_sdk(pc_candidates[-1], tempdir), pc_candidates[-1], \
            "pc"
    if candidates:
        return _extract_sdk(candidates[-1], tempdir), candidates[-1], \
            _sdk_platform(candidates[-1])
    direct = os.path.join(DIST_DIR, "sdk")
    if os.path.isdir(direct):
        return direct, None, _sdk_platform(direct, direct)
    return None, None, "pc"


def _sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _assert_sdk_archive(contract, archive):
    required = contract.get("sdk", {}).get("required_archive_entries", [
        "sdk/include/retro_engine/retro_engine.h",
        "sdk/lib/cmake/RetroPolyEngine/RetroPolyEngineConfig.cmake",
        "sdk/sdk_manifest.json",
        "sdk/checksums.sha256",
    ])
    if not os.path.isfile(archive):
        return False, {"error": "SDK archive missing", "path": archive}
    sidecar = archive + ".sha256"
    if not os.path.isfile(sidecar):
        return False, {"error": "SDK archive checksum sidecar missing",
                       "path": sidecar}
    expected = open(sidecar, "r", encoding="utf-8").read().split()[0]
    actual = _sha256(archive)
    names = []
    if archive.endswith(".zip"):
        with zipfile.ZipFile(archive) as zf:
            names = zf.namelist()
    else:
        with tarfile.open(archive) as tf:
            names = tf.getnames()
    missing = [name for name in required if name not in names]
    forbidden = []
    for name in names:
        low = name.lower()
        parts = set(low.split("/"))
        if (".git" in parts or "bios" in parts or "roms" in parts or
                "secrets" in parts or "cmakefiles" in parts):
            forbidden.append(name)
        if low.endswith((".rom", ".bios", ".iso", ".cue", ".elf", ".bin")):
            forbidden.append(name)
    ok = (expected == actual and not missing and not forbidden)
    return ok, {"path": archive, "sha256": actual, "expected": expected,
                "missing": missing, "forbidden": forbidden[:20],
                "entries": len(names)}


def assert_sdk_archives(contract, archives):
    if not archives:
        return False, {"error": "SDK archives not found "
                               "(build sdk_package first)"}
    details = []
    ok = True
    for archive in archives:
        archive_ok, archive_details = _assert_sdk_archive(contract, archive)
        archive_details["ok"] = archive_ok
        details.append(archive_details)
        ok = ok and archive_ok
    return ok, {"archives": details}


def assert_sdk_manifest(contract, sdk_dir):
    if sdk_dir is None:
        return False, {"error": "SDK directory not found"}
    manifest_path = os.path.join(sdk_dir, "sdk_manifest.json")
    if not os.path.isfile(manifest_path):
        return False, {"error": "sdk_manifest.json not found",
                       "path": manifest_path}
    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
    except (OSError, ValueError) as exc:
        return False, {"error": "sdk_manifest.json invalid: %s" % exc}

    sdk_contract = contract.get("sdk", {})
    expected_version = contract["project"]["version"]
    required_targets = sdk_contract.get("required_targets", [
        "RetroPolyEngine::retro_engine", "RetroPolyEngine::hal",
        "RetroPolyEngine::log"])
    required_components = sdk_contract.get("required_components", [
        "public_headers", "libraries", "cmake_package", "asset_converter",
        "examples", "templates", "legal_notices", "version", "checksums",
        "manifest"])
    required_platforms = sdk_contract.get("platforms", ["pc", "ps1", "saturn"])

    checks = []
    checks.append(("schema", manifest.get("schema") == 1))
    checks.append(("version", manifest.get("version") == expected_version))
    checks.append(("platform", manifest.get("platform") in required_platforms))
    checks.append(("targets", set(required_targets).issubset(
        set(manifest.get("targets", [])))))
    components = manifest.get("components", {})
    platform = manifest.get("platform")
    # asset_converter — host-only инструмент PC SDK; в PS1/Saturn пакетах
    # его нет by design.
    expected_components = [
        c for c in required_components
        if not (c == "asset_converter" and platform != "pc")
    ]
    checks.append(("components", all(components.get(c) is True
                                     for c in expected_components)))
    public_header = os.path.join(sdk_dir,
                                 manifest.get("public_header", ""))
    checks.append(("public_header", os.path.isfile(public_header)))

    artifacts = manifest.get("artifacts", [])
    missing = []
    bad_hashes = []
    for item in artifacts:
        path = os.path.join(sdk_dir, item.get("path", ""))
        if not os.path.isfile(path):
            missing.append(item.get("path"))
            continue
        if os.path.getsize(path) != item.get("size"):
            bad_hashes.append(item.get("path"))
            continue
        if _sha256(path) != item.get("sha256"):
            bad_hashes.append(item.get("path"))
    checks.append(("artifacts_present", not missing))
    checks.append(("artifact_hashes", not bad_hashes))

    checksums_path = os.path.join(sdk_dir, "checksums.sha256")
    checksum_ok = False
    checksum_errors = []
    if os.path.isfile(checksums_path):
        checksum_ok = True
        for line in open(checksums_path, "r", encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            digest, _, rel = line.partition("  ")
            path = os.path.join(sdk_dir, rel)
            if not os.path.isfile(path):
                checksum_ok = False
                checksum_errors.append("missing: " + rel)
            elif _sha256(path) != digest:
                checksum_ok = False
                checksum_errors.append("bad: " + rel)
    checks.append(("checksums", checksum_ok))

    ok = all(value for _, value in checks)
    return ok, {"version": manifest.get("version"),
                "platform": manifest.get("platform"),
                "arch": manifest.get("arch"),
                "git_commit": manifest.get("git_commit"),
                "artifacts": len(artifacts),
                "failed_checks": [name for name, value in checks if not value],
                "missing": missing[:20], "bad_hashes": bad_hashes[:20],
                "checksum_errors": checksum_errors[:20]}


def assert_sdk_headers(contract, sdk_dir):
    if sdk_dir is None:
        return False, {"error": "SDK directory not found"}
    sdk_contract = contract.get("sdk", {})
    umbrella = os.path.join(sdk_dir, sdk_contract.get(
        "umbrella_header", "include/retro_engine/retro_engine.h"))
    if not os.path.isfile(umbrella):
        return False, {"error": "umbrella header missing", "path": umbrella}

    internal_dirs = sdk_contract.get("forbidden_header_dirs", [
        "include/retro_engine/render",
        "include/retro_engine/hal/pc",
        "include/retro_engine/hal/ps1",
        "include/retro_engine/hal/saturn",
    ])
    present_internal = [d for d in internal_dirs
                        if os.path.exists(os.path.join(sdk_dir, d))]

    include_root = os.path.join(sdk_dir, "include", "retro_engine")
    forbidden_patterns = ("SDL", "glad/", "psx", "jo/jo.h", "internal/",
                          "windows.h", "GL/")
    violations = []
    header_count = 0
    for dirpath, _, filenames in os.walk(include_root):
        for name in filenames:
            if not name.endswith((".h", ".hpp")):
                continue
            header_count += 1
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, sdk_dir)
            for line in open(path, "r", encoding="utf-8", errors="replace"):
                stripped = line.strip()
                if not stripped.startswith("#include"):
                    continue
                for pattern in forbidden_patterns:
                    if pattern in stripped:
                        violations.append("%s: %s" % (rel, stripped))
    ok = not present_internal and not violations and header_count > 0
    return ok, {"headers": header_count,
                "internal_dirs": present_internal,
                "third_party_includes": violations[:20]}


def assert_sdk_consumer(contract, sdk_dir, skip, platform):
    if sdk_dir is None:
        return False, {"error": "SDK directory not found"}
    if skip:
        return True, {"skipped": True}
    cmd = [sys.executable, SDK_CONSUMER_SCRIPT, "--sdk-dir", sdk_dir,
           "--platform", platform,
           "--expected-version", contract["project"]["version"]]
    rc, out, err = run(cmd, timeout=900)
    ok = rc == 0
    return ok, {"exit_code": rc, "platform": platform, "ok": ok,
                "tail": (err if err else out)[-1200:]}


def main():
    parser = argparse.ArgumentParser(description="Release contract verifier")
    parser.add_argument("--w1-seconds", type=int, default=60,
                        help="W1 bench duration (default: 60)")
    parser.add_argument("--output", type=str, default=REPORT_PATH,
                        help="Report JSON path")
    parser.add_argument("--skip-build-checks", action="store_true",
                        help="Skip ctest/bench execution (validate manifest only)")
    parser.add_argument("--sdk-dir", default=os.environ.get("RPE_SDK_DIR"),
                        help="Путь к распакованному SDK (default: dist/sdk)")
    parser.add_argument("--sdk-archive", default=None,
                        help="Путь к retro-engine-sdk-*.tar.gz/.zip")
    parser.add_argument("--skip-sdk-consumer", action="store_true",
                        help="Не собирать clean consumer (только manifest)")
    args = parser.parse_args()

    contract = load_contract()
    results = {}
    sdk_temp = tempfile.mkdtemp(prefix="rpe_sdk_gate_")
    sdk_dir, sdk_archive, sdk_platform = _find_sdk(args, sdk_temp)
    if args.sdk_archive:
        sdk_archives = [os.path.abspath(args.sdk_archive)]
    else:
        sdk_archives = sorted(glob.glob(os.path.join(
            DIST_DIR, "retro-engine-sdk-*.tar.gz")))
    results["sdk_archives"] = assert_sdk_archives(contract, sdk_archives)
    results["sdk_manifest"] = assert_sdk_manifest(contract, sdk_dir)
    results["sdk_headers"] = assert_sdk_headers(contract, sdk_dir)
    results["sdk_consumer"] = assert_sdk_consumer(
        contract, sdk_dir, args.skip_build_checks or args.skip_sdk_consumer,
        sdk_platform)

    results["presets"] = assert_presets(contract)
    results["ps1_exe_size"] = assert_ps1_exe_size(contract)
    results["ps1_ram_vram"] = assert_ps1_ram_vram(contract)
    results["saturn_artifacts"] = assert_saturn_artifacts(contract)
    results["saturn_map_budget"] = assert_saturn_map_budget(contract)
    results["saturn_golden"] = assert_saturn_golden(contract)

    if not args.skip_build_checks:
        results["w0_hash"] = assert_w0_hash(contract)
        results["w1_pc_fps"] = assert_w1_pc_fps(contract, args.w1_seconds)
        results["w1_ps1_fps"] = assert_w1_ps1_fps(contract)
        results["asset_validation"] = assert_asset_validation()
        results["renderer_overflow"] = assert_renderer_overflow()
    else:
        results["w0_hash"] = (True, {"skipped": True})
        results["w1_pc_fps"] = (True, {"skipped": True})
        results["w1_ps1_fps"] = (True, {"skipped": True})
        results["asset_validation"] = (True, {"skipped": True})
        results["renderer_overflow"] = (True, {"skipped": True})

    shutil.rmtree(sdk_temp, ignore_errors=True)

    all_ok = all(ok for ok, _ in results.values())
    report = {
        "project": contract["project"]["name"],
        "version": contract["project"]["version"],
        "all_ok": all_ok,
        "asserts": {k: {"ok": v[0], "details": v[1]} for k, v in results.items()},
    }

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)

    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
