#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/release_gate_negative_tests.py (Спринт 37–38)
#
# Negative-tests release gate v2 на синтетических копиях готового payload:
#   missing sdk/ ui/ checksums/report → FAIL
#   повреждённый checksum              → FAIL
#   BIOS/ROM/секреты/build-каталог      → FAIL
#   demo-образ вне demos/              → FAIL
#   report all_ok: false               → FAIL
#
# Baseline (валидный payload) обязан пройти. Скрипт возвращает 0, только если
# все ожидания выполнены.
#
# Пример:
#   python3 scripts/build_release_bundle.py ...
#   python3 scripts/release_gate_negative_tests.py \
#       --bundle-dir dist/retro-engine-v0.2.1
# ============================================================================
import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import release_payload  # noqa: E402

VERIFIER = os.path.join(REPO_ROOT, "scripts", "verify_release_contract.py")
DIST_DIR = os.path.join(REPO_ROOT, "dist")


def load_version():
    import yaml
    with open(os.path.join(REPO_ROOT, "ci", "normative_contracts.yaml"),
              "r", encoding="utf-8") as f:
        return str(yaml.safe_load(f)["project"]["version"])


def find_bundle():
    version = load_version()
    expected = os.path.join(DIST_DIR, "retro-engine-v%s" % version)
    if os.path.isdir(expected):
        return expected
    candidates = sorted(path for path in
                        glob.glob(os.path.join(DIST_DIR, "retro-engine-v*"))
                        if os.path.isdir(path))
    if candidates:
        return candidates[-1]
    raise SystemExit("release payload not found; run "
                     "scripts/build_release_bundle.py first")


def run_gate(payload_dir, report_path):
    proc = subprocess.run(
        [sys.executable, VERIFIER, "--payload-only",
         "--payload-dir", payload_dir, "--output", report_path],
        capture_output=True, text=True)
    try:
        report = json.load(open(report_path, encoding="utf-8"))
    except (OSError, ValueError):
        report = {}
    details = report.get("asserts", {}).get(
        "release_payload", {}).get("details", {})
    return proc.returncode, report, details, (proc.stdout + proc.stderr)


def mutate_missing(name):
    if os.path.isdir(name):
        shutil.rmtree(name)
    else:
        os.remove(name)


def refresh_checksums(path):
    release_payload.write_checksums(path)


def case_missing_sdk(path):
    mutate_missing(os.path.join(path, "sdk"))


def case_missing_ui(path):
    mutate_missing(os.path.join(path, "ui"))


def case_missing_checksums(path):
    mutate_missing(os.path.join(path, "checksums.sha256"))


def case_missing_report(path):
    mutate_missing(os.path.join(path, "release_contract_report.json"))


def case_bad_checksum(path):
    with open(os.path.join(path, "README.md"), "a", encoding="utf-8") as f:
        f.write("\nchecksum mutation\n")


def case_bios(path):
    os.makedirs(os.path.join(path, "bios"), exist_ok=True)
    with open(os.path.join(path, "bios", "scph1001.bin"), "wb") as f:
        f.write(b"not-a-real-bios\x00")
    refresh_checksums(path)


def case_rom(path):
    os.makedirs(os.path.join(path, "roms"), exist_ok=True)
    with open(os.path.join(path, "roms", "game.z64"), "wb") as f:
        f.write(b"not-a-real-rom\x00")
    refresh_checksums(path)


def case_secrets(path):
    os.makedirs(os.path.join(path, "secrets"), exist_ok=True)
    with open(os.path.join(path, "secrets", "token.key"), "w",
              encoding="utf-8") as f:
        f.write("secret\n")
    refresh_checksums(path)


def case_build_dir(path):
    os.makedirs(os.path.join(path, "build"), exist_ok=True)
    with open(os.path.join(path, "build", "CMakeCache.txt"), "w",
              encoding="utf-8") as f:
        f.write("build junk\n")
    refresh_checksums(path)


def case_demo_outside_demos(path):
    with open(os.path.join(path, "game.iso"), "wb") as f:
        f.write(b"demo image outside demos/\n")
    refresh_checksums(path)


def case_report_false(path):
    report_path = os.path.join(path, "release_contract_report.json")
    with open(report_path, "r", encoding="utf-8") as f:
        report = json.load(f)
    report["all_ok"] = False
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    refresh_checksums(path)


def case_unexpected_top_level(path):
    with open(os.path.join(path, "notes.txt"), "w", encoding="utf-8") as f:
        f.write("unexpected\n")
    refresh_checksums(path)


CASES = [
    ("missing-sdk", case_missing_sdk, "sdk"),
    ("missing-ui", case_missing_ui, "ui"),
    ("missing-checksums", case_missing_checksums, "checksums"),
    ("missing-report", case_missing_report, "report"),
    ("bad-checksum", case_bad_checksum, "checksums"),
    ("bios", case_bios, "forbidden"),
    ("roms", case_rom, "forbidden"),
    ("secrets", case_secrets, "forbidden"),
    ("build-dir", case_build_dir, "forbidden"),
    ("demo-outside-demos", case_demo_outside_demos, "non-demo"),
    ("report-all-ok-false", case_report_false, "all_ok"),
    ("unexpected-top-level", case_unexpected_top_level, "unexpected"),
]


def main():
    parser = argparse.ArgumentParser(
        description="Negative-tests release payload gate")
    parser.add_argument("--bundle-dir", default=None)
    parser.add_argument("--keep", action="store_true")
    args = parser.parse_args()

    base = os.path.abspath(args.bundle_dir or find_bundle())
    if not os.path.isdir(base):
        raise SystemExit("bundle dir not found: %s" % base)
    tempdir = tempfile.mkdtemp(prefix="rpe_gate_negative_")
    failures = []
    try:
        baseline = os.path.join(tempdir, "baseline")
        shutil.copytree(base, baseline)
        rc, report, details, log = run_gate(
            baseline, os.path.join(tempdir, "baseline.json"))
        print("[baseline] exit=%d all_ok=%s" %
              (rc, report.get("all_ok")))
        if rc != 0 or report.get("all_ok") is not True:
            print(json.dumps(details, ensure_ascii=False, indent=2))
            print(log[-2000:])
            raise SystemExit("baseline payload must pass before negative tests")

        for name, mutate, expected in CASES:
            case_dir = os.path.join(tempdir, name)
            shutil.copytree(base, case_dir)
            mutate(case_dir)
            rc, report, details, log = run_gate(
                case_dir, os.path.join(tempdir, name + ".json"))
            failed = details.get("failed", [])
            text = json.dumps(details, ensure_ascii=False)
            expected_hit = expected.lower() in text.lower()
            ok = rc != 0 and report.get("all_ok") is False and expected_hit
            print("[%-22s] exit=%d all_ok=%s expected=%s -> %s" %
                  (name, rc, report.get("all_ok"), expected,
                   "PASS" if ok else "FAIL"))
            if not ok:
                failures.append({"case": name, "exit": rc,
                                 "failed": failed, "log": log[-1200:]})
    finally:
        if not args.keep:
            shutil.rmtree(tempdir, ignore_errors=True)
        else:
            print("kept temp dir: %s" % tempdir)

    if failures:
        print(json.dumps({"ok": False, "failures": failures},
                         ensure_ascii=False, indent=2))
        return 1
    print(json.dumps({"ok": True, "cases": len(CASES) + 1,
                      "bundle": base}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
