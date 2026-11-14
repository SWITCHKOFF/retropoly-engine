#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/verify_n64_budgets.py (S45–46)
#
# Проверяет бюджеты N64 v0.3.0 по фактическим артефактам n64-release:
#   - hello_triangle.z64 / rotating_cube.z64 проходят ROM-verifier;
#   - ROM/DFS ≤ engine_rom_budget_bytes (32 MiB, contract), абсолютный
#     лимит картриджа — 64 MiB;
#   - alias-файлы побайтово совпадают (hello_n64.z64, retro_n64.z64);
#   - зафиксированы sizes/sha256 для benchmark/G12.
#
# Использование:
#   python3 scripts/verify_n64_budgets.py \
#       --artifacts build/n64-release/src/hal/n64 \
#       --output /tmp/n64_budgets.json
# ============================================================================
import argparse
import hashlib
import json
import os
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTRACT = os.path.join(REPO_ROOT, "ci", "normative_contracts.yaml")
ROM_VERIFIER = os.path.join(REPO_ROOT, "scripts", "verify_n64_rom.py")


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_contract(path):
    try:
        import yaml
    except ImportError as exc:  # pragma: no cover
        raise SystemExit("PyYAML требуется для verify_n64_budgets.py: %s" % exc)
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def verify_rom(path):
    proc = subprocess.run(
        [sys.executable, ROM_VERIFIER, "--json", path],
        capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit("ROM verifier failed for %s:\n%s\n%s" %
                         (path, proc.stdout, proc.stderr))
    return json.loads(proc.stdout)


def main():
    parser = argparse.ArgumentParser(description="N64 v0.3.0 budget verifier")
    parser.add_argument("--artifacts", default=os.path.join(
        REPO_ROOT, "build", "n64-release", "src", "hal", "n64"))
    parser.add_argument("--contract", default=CONTRACT)
    parser.add_argument("--output", default=None)
    parser.add_argument("--skip-rom-verifier", action="store_true")
    args = parser.parse_args()

    contract = load_contract(args.contract)
    n64 = contract.get("platforms", {}).get("n64", {})
    rom_budget = int(n64.get("rom", {}).get(
        "engine_rom_budget_bytes", 32 * 1024 * 1024))
    rom_max = int(n64.get("rom", {}).get("max_bytes", 64 * 1024 * 1024))
    stack_budget = int(n64.get("memory", {}).get("cpu_stack_bytes", 64 * 1024))
    tmem_budget = int(n64.get("memory", {}).get("tmem_bytes", 4096))
    rdram_budget = int(n64.get("memory", {}).get("rdram_base_bytes", 4 * 1024 * 1024))
    peak_limit = int(n64.get("memory", {}).get(
        "runtime_peak_limit_bytes", 3584 * 1024))

    art = os.path.abspath(args.artifacts)
    required = [
        "hello_triangle.z64", "rotating_cube.z64",
        "hello_triangle.z64.sha256", "rotating_cube.z64.sha256",
        "hello_triangle_n64.map", "rotating_cube_n64.map",
        "retro_n64.dfs", "retro_n64.dfs.txt",
        "retro_n64.z64", "hello_n64.z64",
    ]
    missing = [name for name in required
               if not os.path.isfile(os.path.join(art, name))]
    failures = []
    if missing:
        failures.append("missing artifacts: " + ", ".join(missing))

    roms = {}
    for name in ("hello_triangle.z64", "rotating_cube.z64"):
        path = os.path.join(art, name)
        if not os.path.isfile(path):
            continue
        size = os.path.getsize(path)
        info = {
            "size_bytes": size,
            "within_engine_budget": size <= rom_budget,
            "within_cartridge_limit": size <= rom_max,
            "sha256": sha256_file(path),
        }
        if not info["within_engine_budget"]:
            failures.append("%s exceeds engine ROM budget (%d > %d)" %
                            (name, size, rom_budget))
        if not info["within_cartridge_limit"]:
            failures.append("%s exceeds cartridge limit (%d > %d)" %
                            (name, size, rom_max))
        if not args.skip_rom_verifier:
            report = verify_rom(path)
            info["verifier"] = {
                "ok": report.get("errors") == [] and report.get("magic_ok") is True,
                "entry": report.get("entry"),
                "elf_load_segments": (report.get("elf") or {}).get(
                    "load_segments"),
                "crc_policy": report.get("crc_policy"),
            }
            if not info["verifier"]["ok"]:
                failures.append("%s: ROM verifier failed" % name)
        roms[name] = info

    dfs_path = os.path.join(art, "retro_n64.dfs")
    dfs = {"path": dfs_path, "exists": os.path.isfile(dfs_path)}
    if dfs["exists"]:
        dfs["size_bytes"] = os.path.getsize(dfs_path)
        dfs["within_budget"] = dfs["size_bytes"] <= rom_budget
        dfs["sha256"] = sha256_file(dfs_path)
        if not dfs["within_budget"]:
            failures.append("retro_n64.dfs exceeds ROM/DFS budget")
        listing_path = os.path.join(art, "retro_n64.dfs.txt")
        if os.path.isfile(listing_path):
            listing = open(listing_path, "r", encoding="utf-8",
                           errors="replace").read()
            dfs["files"] = ["CUBE.MSH", "TEX0.TEX", "TRIANGLE.MSH"]
            for name in dfs["files"]:
                if name not in listing:
                    failures.append("DFS listing misses " + name)
    else:
        failures.append("retro_n64.dfs missing")

    aliases = {}
    pairs = [("hello_n64.z64", "hello_triangle.z64"),
             ("retro_n64.z64", "rotating_cube.z64")]
    for alias, canonical in pairs:
        a = os.path.join(art, alias)
        c = os.path.join(art, canonical)
        if os.path.isfile(a) and os.path.isfile(c):
            same = sha256_file(a) == sha256_file(c)
            aliases[alias] = {"canonical": canonical, "identical": same}
            if not same:
                failures.append("%s != %s" % (alias, canonical))

    budgets = {
        "rom_engine_budget_bytes": rom_budget,
        "rom_cartridge_max_bytes": rom_max,
        "dfs_max_bytes": rom_budget,
        "cpu_stack_bytes": stack_budget,
        "tmem_bytes": tmem_budget,
        "rdram_base_bytes": rdram_budget,
        "rdram_peak_limit_bytes": peak_limit,
        "heap": "forbidden",
    }
    result = {
        "ok": not failures,
        "artifacts": art,
        "budgets": budgets,
        "roms": roms,
        "dfs": dfs,
        "aliases": aliases,
        "failures": failures,
    }
    text = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        with open(args.output, "w", encoding="utf-8", newline="\n") as f:
            f.write(text + "\n")
    print(text)
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
