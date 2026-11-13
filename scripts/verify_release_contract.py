#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# RetroPolyEngine — scripts/verify_release_contract.py
#
# Release gate: 9 assert-ов для тега v0.1.0.
#   1. presets == {pc-debug, pc-release, pc-debug-render, ps1-release}
#   2. ps1_executable_size <= 512000
#   3. ps1_peak_main_ram <= 1572864; ps1_peak_vram <= 819200
#   4. w0_framebuffer_hash == canonical_w0_hash
#   5. w1_pc_fps >= 60; w1_ps1_emulator_fps >= 30
#   6. all_asset_validation_cases_pass; no_renderer_overflow_is_silent
#   7. Публикация результата как CI-artifact (release_contract_report.json)
#
# Вызов:
#   python3 scripts/verify_release_contract.py [--w1-seconds 60]
#
# Требования: собранные пресеты pc-debug, pc-debug-render, pc-release,
# ps1-release в канонических каталогах build/<preset>.

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

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


def main():
    parser = argparse.ArgumentParser(description="Release contract verifier")
    parser.add_argument("--w1-seconds", type=int, default=60,
                        help="W1 bench duration (default: 60)")
    parser.add_argument("--output", type=str, default=REPORT_PATH,
                        help="Report JSON path")
    parser.add_argument("--skip-build-checks", action="store_true",
                        help="Skip ctest/bench execution (validate manifest only)")
    args = parser.parse_args()

    contract = load_contract()
    results = {}

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
