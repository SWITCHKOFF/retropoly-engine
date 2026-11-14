#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/release_payload.py (Спринт 37–38)
#
# Общая проверка release payload v0.2.1:
#   dist/retro-engine-v<ver>/
#     sdk/                         # установленный PC SDK (headers/libs/config)
#     ui/                          # пакет UI client для host-платформы
#     demos/                       # OPTIONAL demo-образы PC/PS1/Saturn
#     checksums.sha256             # полный список файлов payload
#     release_contract_report.json # отчёт verify_release_contract.py
#     README.md LICENSE LEGAL.md
#
# Модуль используется scripts/verify_release_contract.py и
# scripts/build_release_bundle.py, чтобы правила gate были едиными.
# ============================================================================
import hashlib
import json
import os


REQUIRED_TOP_LEVEL = [
    "sdk",
    "ui",
    "checksums.sha256",
    "release_contract_report.json",
    "README.md",
    "LICENSE",
    "LEGAL.md",
]
OPTIONAL_TOP_LEVEL = ["demos"]
FORBIDDEN_DIR_PARTS = {
    ".git", ".svn", "build", "cmakefiles", "bios", "roms", "secrets",
    "node_modules", ".cache",
}
FORBIDDEN_FILE_SUFFIXES = (
    ".rom", ".bios", ".key", ".pem", ".p12", ".pfx", ".jks", ".keystore",
)
FORBIDDEN_FILE_NAMES = {
    "id_rsa", "id_dsa", "id_ecdsa", "id_ed25519", ".env",
}
DEMO_ONLY_SUFFIXES = (".iso", ".cue", ".bin", ".elf", ".map")


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def iter_files(root, exclude=()):
    """Детерминированный список (path, rel) без служебных файлов."""
    excluded = set(exclude)
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(dirnames)
        for name in sorted(filenames):
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            if rel in excluded:
                continue
            yield path, rel


def write_checksums(root, output_name="checksums.sha256"):
    """Пишет checksums.sha256 (sorted, POSIX-пути) и возвращает число строк."""
    lines = []
    for path, rel in iter_files(root, exclude=(output_name,)):
        lines.append("%s  %s" % (sha256_file(path), rel))
    target = os.path.join(root, output_name)
    with open(target, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return len(lines)


def verify_checksums(root, filename="checksums.sha256", require_complete=False):
    """Проверяет checksums.sha256; возвращает (ok, details)."""
    path = os.path.join(root, filename)
    if not os.path.isfile(path):
        return False, {"error": "checksums file missing", "path": path}
    listed = []
    bad = []
    missing = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            digest, _, rel = line.partition("  ")
            if not rel:
                bad.append("malformed: " + line[:120])
                continue
            listed.append(rel)
            target = os.path.join(root, rel)
            if not os.path.isfile(target):
                missing.append(rel)
            elif sha256_file(target) != digest:
                bad.append(rel)
    details = {"path": filename, "entries": len(listed),
               "missing": missing[:20], "bad": bad[:20]}
    if require_complete:
        actual = {rel for _, rel in iter_files(root, exclude=(filename,))}
        uncovered = sorted(actual - set(listed))
        details["uncovered"] = uncovered[:20]
    else:
        uncovered = []
    ok = not missing and not bad and not uncovered
    return ok, details


def _read_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f), None
    except (OSError, ValueError) as exc:
        return None, str(exc)


def _forbidden_entries(root, forbidden_dir_parts=None):
    """BIOS/ROM/секреты/build-каталоги; demo-образы разрешены только в demos/."""
    dir_parts = set(FORBIDDEN_DIR_PARTS) | set(forbidden_dir_parts or [])
    forbidden = []
    for dirpath, dirnames, filenames in os.walk(root):
        rel_dir = os.path.relpath(dirpath, root).replace(os.sep, "/")
        for name in list(dirnames):
            if name.lower() in dir_parts:
                forbidden.append("dir: " + os.path.join(rel_dir, name).lstrip("./"))
        for name in filenames:
            rel = os.path.relpath(os.path.join(dirpath, name), root).replace(
                os.sep, "/")
            lower = name.lower()
            parts = lower.split("/")
            if lower in FORBIDDEN_FILE_NAMES or any(
                    p in FORBIDDEN_FILE_NAMES for p in parts):
                forbidden.append("file: " + rel)
                continue
            if lower.endswith(FORBIDDEN_FILE_SUFFIXES):
                forbidden.append("file: " + rel)
                continue
            if lower.endswith(DEMO_ONLY_SUFFIXES) and not rel.startswith("demos/"):
                forbidden.append("non-demo image: " + rel)
    return sorted(set(forbidden))


def validate_payload(payload_dir, contract, require_report=True):
    """Проверяет layout dist/retro-engine-v<ver>/ и возвращает (ok, details)."""
    payload_dir = os.path.abspath(payload_dir)
    details = {"path": payload_dir, "failed": []}
    if not os.path.isdir(payload_dir):
        return False, {"path": payload_dir, "failed": ["payload directory missing"]}

    actual_top = sorted(os.listdir(payload_dir))
    release_contract = contract.get("release", {})
    required_toplist = release_contract.get(
        "payload_required_entries", REQUIRED_TOP_LEVEL)
    optional_toplist = release_contract.get(
        "payload_optional_entries", OPTIONAL_TOP_LEVEL)
    required_top = set(required_toplist)
    optional_top = set(optional_toplist)
    missing_top = sorted(required_top - set(actual_top))
    unexpected_top = sorted(set(actual_top) - required_top - optional_top)
    if missing_top:
        details["failed"].append("missing top-level: " + ", ".join(missing_top))
    if unexpected_top:
        details["failed"].append("unexpected top-level: " +
                                 ", ".join(unexpected_top))
    details["top_level"] = actual_top

    expected_version = str(contract["project"]["version"])

    sdk_dir = os.path.join(payload_dir, "sdk")
    sdk_manifest, sdk_error = _read_json(
        os.path.join(sdk_dir, "sdk_manifest.json"))
    if sdk_error:
        details["failed"].append("sdk_manifest.json: " + sdk_error)
    else:
        details["sdk_version"] = sdk_manifest.get("version")
        details["sdk_platform"] = sdk_manifest.get("platform")
        if sdk_manifest.get("version") != expected_version:
            details["failed"].append("sdk version mismatch: %s != %s" %
                                     (sdk_manifest.get("version"),
                                      expected_version))
        if sdk_manifest.get("platform") not in ("pc", "ps1", "saturn"):
            details["failed"].append("sdk platform invalid: %s" %
                                     sdk_manifest.get("platform"))

    ui_dir = os.path.join(payload_dir, "ui")
    ui_manifest, ui_error = _read_json(
        os.path.join(ui_dir, "ui_manifest.json"))
    if ui_error:
        details["failed"].append("ui_manifest.json: " + ui_error)
    else:
        details["ui_version"] = ui_manifest.get("version")
        details["ui_os"] = ui_manifest.get("os")
        details["ui_arch"] = ui_manifest.get("arch")
        if ui_manifest.get("version") != expected_version:
            details["failed"].append("ui version mismatch: %s != %s" %
                                     (ui_manifest.get("version"),
                                      expected_version))
        if not str(ui_manifest.get("version", "")).startswith(
                str(ui_manifest.get("sdk_series", "")) + "."):
            details["failed"].append("ui sdk_series mismatch")

    required_files = [
        "sdk/include/retro_engine/retro_engine.h",
        "sdk/include/retro_engine/version.h",
        "sdk/lib/cmake/RetroPolyEngine/RetroPolyEngineConfig.cmake",
        "sdk/lib/cmake/RetroPolyEngine/RetroPolyEngineTargets.cmake",
        "sdk/sdk_manifest.json",
        "sdk/checksums.sha256",
        "sdk/VERSION.txt",
        "sdk/LEGAL.md",
        "ui/ui_manifest.json",
        "ui/checksums.sha256",
        "ui/VERSION.txt",
        "ui/LEGAL.md",
        "ui/README-ui.md",
    ]
    missing_files = [rel for rel in required_files
                     if not os.path.isfile(os.path.join(payload_dir, rel))]
    ui_binary = ["ui/retro-engine-ui", "ui/retro-engine-ui.exe"]
    if not any(os.path.isfile(os.path.join(payload_dir, rel))
               for rel in ui_binary):
        missing_files.append("ui/retro-engine-ui[.exe]")
    if missing_files:
        details["failed"].append("missing files: " + ", ".join(missing_files))

    sdk_checksums_ok, sdk_checksums = verify_checksums(sdk_dir)
    ui_checksums_ok, ui_checksums = verify_checksums(ui_dir)
    details["sdk_checksums"] = sdk_checksums
    details["ui_checksums"] = ui_checksums
    if not sdk_checksums_ok:
        details["failed"].append("sdk checksums invalid")
    if not ui_checksums_ok:
        details["failed"].append("ui checksums invalid")

    required_checksums = release_contract.get(
        "payload_required_checksums",
        ["sdk/checksums.sha256", "ui/checksums.sha256",
         "checksums.sha256"])
    missing_checksums = [rel for rel in required_checksums
                         if not os.path.isfile(os.path.join(payload_dir, rel))]
    if missing_checksums:
        details["failed"].append(
            "missing required checksums: " + ", ".join(missing_checksums))

    top_checksums_ok, top_checksums = verify_checksums(
        payload_dir, require_complete=True)
    details["checksums"] = top_checksums
    if not top_checksums_ok:
        details["failed"].append("payload checksums invalid/incomplete")

    report_path = os.path.join(payload_dir, "release_contract_report.json")
    if require_report:
        report, report_error = _read_json(report_path)
        if report_error:
            details["failed"].append("release report: " + report_error)
        else:
            details["report_all_ok"] = report.get("all_ok")
            details["report_version"] = report.get("version")
            if report.get("all_ok") is not True:
                details["failed"].append("release report all_ok is not true")
            if str(report.get("version")) != expected_version:
                details["failed"].append("release report version mismatch")
            asserts = report.get("asserts", {})
            for name in ("sdk_archives", "sdk_manifest", "sdk_consumer",
                         "ui_archives", "ui_manifest", "ui_smoke"):
                item = asserts.get(name)
                if not isinstance(item, dict) or item.get("ok") is not True:
                    details["failed"].append(
                        "release report assert not ok: " + name)
                    continue
                if name in ("sdk_consumer", "ui_smoke") and \
                        item.get("details", {}).get("skipped") is True:
                    details["failed"].append(
                        "release report assert skipped: " + name)
    else:
        details["report_all_ok"] = None

    forbidden = _forbidden_entries(
        payload_dir,
        release_contract.get("payload_forbidden_parts"))
    details["forbidden"] = forbidden[:40]
    if forbidden:
        details["failed"].append("forbidden content: %d entr%s" %
                                 (len(forbidden),
                                  "y" if len(forbidden) == 1 else "ies"))

    demos_dir = os.path.join(payload_dir, "demos")
    if os.path.isdir(demos_dir):
        details["demos"] = sorted(
            os.path.relpath(os.path.join(d, n), payload_dir).replace(os.sep, "/")
            for d, _, names in os.walk(demos_dir) for n in names)[:60]
    else:
        details["demos"] = None

    ok = not details["failed"]
    details["ok"] = ok
    return ok, details
