#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# RetroPolyEngine — tools/testsprite_bridge.py
#
# QA-мост TestSprite (host-only, не часть движка): единственный способ
# исполнить сгенерированные TestSprite'ом бэкенд-тесты против нативного
# C++-проекта — предъявить облачному исполнителю детерминированный
# HTTP-контракт qa-гейтов движка. Мост слушает 127.0.0.1:5173
# (localEndpoint testsprite_tests/tmp/config.json), прокладывает поверх
# уже собранных артефактов:
#
#   GET /api/health         — живость моста;
#   GET /api/engine/info    — проект, git-нетривиальные → фикс-ревизия,
#                             состав гейг-наборов, наличие бинарей;
#   GET /api/tests/run      — один процесс gtest (ретро-пресеты):
#                             binary=unit → build/pc-debug/tests/retro_engine_tests,
#                             binary=visual → build/pc-debug-render/tests/
#                             retro_engine_visual_tests; suites — вайтлист;
#   GET /api/assets/digest — sha256-дерево assets/build/* (рецепт
#                             docs/benchmarks/reference_environment.yaml);
#   GET /api/assets/determinism — asset_converter дважды → байт-тождество;
#   GET /api/bench/smoke    — pc-release hello_triangle 1 (exit 0 + FPS ≥ 60).
#
# Безопасность: bind строго на loopback; окончания имён наборов и все пути —
# из фиксированной таблицы; параметры запроса фильтруются, произвольные
# команды не исполняются. Изменение состояния движка недоступно по сети.
#
# Запуск: python3 tools/testsprite_bridge.py (cwd — корень репозитория).
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

UNIT_BINARY = os.path.join(REPO_ROOT, "build/pc-debug/tests/retro_engine_tests")
VISUAL_BINARY = os.path.join(
    REPO_ROOT, "build/pc-debug-render/tests/retro_engine_visual_tests")
BENCH_BINARY = os.path.join(REPO_ROOT, "build/pc-release/game/hello_triangle")
CONVERTER_BINARY = os.path.join(
    REPO_ROOT, "build/pc-release/tools/asset_converter/asset_converter")

UNIT_SUITES = (
    "Fixed", "Vec2", "Vec3", "Vec4", "Trig", "Mat4", "Mat3",
    "LinearAllocator", "PoolAllocator", "StackAllocator", "AllocatorsIntegration",
    "Entity", "RegistryTest", "Log", "TextureRgb555", "TexturePoolTest",
    "CommandBufferLayout", "CommandBufferApi", "CommandBufferOverflow",
    "DepthKeyNormative", "SortKeyContract",
    "Hal", "HalMouse", "HalFileSystem",
    "MeshLoader", "TexContract", "TextureLoader",
)
VISUAL_SUITES = ("W0Golden", "RenderLifecycle", "HelloTriangleGolden")

# Гейт производительности W1 (docs/benchmarks/reference_environment.yaml).
W1_MIN_FPS = 60.0
RUN_TIMEOUT_S = 180.0
QUOTE_RE = re.compile(r"[^A-Za-z0-9_.]+")
# Итоговая строка bench лога: «итог: N кадров за M мс, среднее fps=X.YY».
# Дробь ровно 2 знака — только в итоговой строке (окна печатают целое
# «fps=N (окно M)» и не матчатся).
BENCH_FPS_RE = re.compile(r"fps=([0-9]+\.[0-9]{2})\b")

_run_lock = threading.Lock()


def probe_binaries():
    """Проверка наличия собранных артефактов (без исполнения)."""
    return {
        "unit": os.path.isfile(UNIT_BINARY),
        "visual": os.path.isfile(VISUAL_BINARY),
        "bench": os.path.isfile(BENCH_BINARY),
        "asset_converter": os.path.isfile(CONVERTER_BINARY),
    }


def run_unit_or_visual(binary, suites):
    """Один процесс gtest поверх фильтра вайтлиста; zero вторжения.
    @param binary ключ «unit» | «visual»."""
    binaries = probe_binaries()
    if not binaries.get(binary):
        return None, "binary not built (%s)" % binary, {}
    bin_path = UNIT_BINARY if binary == "unit" else VISUAL_BINARY
    filter_expr = ":".join(s + ".*" for s in suites)
    started = time.time()
    try:
        proc = subprocess.run(
            [bin_path, "--gtest_filter=" + filter_expr],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=RUN_TIMEOUT_S,
        )
    except subprocess.TimeoutExpired:
        return None, "timeout after %ss" % RUN_TIMEOUT_S, \
               {"timeout_seconds": RUN_TIMEOUT_S}
    stdout = proc.stdout or ""

    def count(line):
        m = re.search(r"\]\s+(\d+) tests?", line)
        return int(m.group(1)) if m else 0

    header = re.search(r"(\d+) tests from (\d+) test suites? ran", stdout)
    ran = int(header.group(1)) if header else 0
    passed, failed, skipped = 0, 0, 0
    fail_names = []
    in_failures = False
    for line in stdout.splitlines():
        if "[  PASSED" in line:
            passed += count(line)
        elif "[  SKIPPED" in line:
            skipped += count(line)
        elif "[  FAILED" in line:
            failed += count(line)
            if "listed below" in line:
                in_failures = True
        elif in_failures and line.strip().startswith("["):
            fail_names.append(line.strip())
    elapsed = round(time.time() - started, 2)
    return {
        "exit_code": proc.returncode,
        "ran": ran,
        "passed": passed,
        "failed": failed,
        "skipped": skipped,
        "failures": fail_names[0:32],
        "elapsed_seconds": elapsed,
    }, (proc.stdout[-2000:] if failed else ""), {"suites": list(suites)}


def assets_digest():
    """Тот же состав, что у W1-рецепта: без dot-файлов (как shell-glob),
    только файлы, отсортированные по имени."""
    assets_dir = os.path.join(REPO_ROOT, "assets/build")
    files = sorted(n for n in os.listdir(assets_dir) if not n.startswith("."))
    per_file = []
    for name in files:
        path = os.path.join(assets_dir, name)
        if not os.path.isfile(path):
            continue
        with open(path, "rb") as f:
            per_file.append({"name": name,
                             "sha256": hashlib.sha256(f.read()).hexdigest()})
    joined = "\n".join(item["sha256"] for item in per_file) + "\n"
    return {"digest": hashlib.sha256(joined.encode()).hexdigest(),
            "files": per_file}


def converter_determinism():
    if not os.path.isfile(CONVERTER_BINARY):
        return None, "asset_converter not built"
    src_dir = os.path.join(REPO_ROOT, "assets/source")
    with tempfile.TemporaryDirectory(prefix="qa_bridge_") as tmp:
        results = {}
        run = {}
        for fmt in ("mesh", "tex"):
            src = os.path.join(
                src_dir, "triangle.gltf" if fmt == "mesh" else "texture.png")
            hashes = []
            for idx in (1, 2):
                dst = os.path.join(
                    tmp, "out_%d_%s" % (idx, fmt)
                      + (".mesh" if fmt == "mesh" else ".tex"))
                proc = subprocess.run(
                    [CONVERTER_BINARY, src, dst],
                    capture_output=True, text=True, timeout=60)
                if proc.returncode != 0 or not os.path.isfile(dst):
                    return None, ("converter failed: %s (rc=%d)"
                                  % (fmt, proc.returncode)), \
                        {"stderr": (proc.stderr or "")[-400:]}
                with open(dst, "rb") as f:
                    hashes.append(hashlib.sha256(f.read()).hexdigest())
            results[fmt] = (hashes[0] == hashes[1])
            run[fmt] = {"sha256_first": hashes[0], "sha256_second": hashes[1]}
        return {"mesh_identical": results["mesh"],
                "tex_identical": results["tex"],
                "runs": run}, "", {"source_dir": "assets/source"}


def bench_smoke():
    if not os.path.isfile(BENCH_BINARY):
        return None, "bench binary not built"
    with tempfile.TemporaryDirectory(prefix="qa_bench_") as tmp:
        try:
            proc = subprocess.run(
                [BENCH_BINARY, "1"],
                cwd=tmp,
                capture_output=True,
                text=True,
                timeout=30,
            )
        except subprocess.TimeoutExpired:
            return None, "bench timed out"
        log_file = os.path.join(tmp, "retro_engine.log")
        log_tail = ""
        avg_fps = None
        if os.path.isfile(log_file):
            with open(log_file, encoding="utf-8", errors="replace") as f:
                log_tail = f.read()[-1600:]
            m = BENCH_FPS_RE.search(log_tail)
            if m:
                avg_fps = float(m.group(1))
        ok = proc.returncode == 0 and avg_fps is not None and avg_fps >= W1_MIN_FPS
        return {"exit_code": proc.returncode,
                "avg_fps": avg_fps,
                "min_required_fps": W1_MIN_FPS,
                "measured_seconds": 1,
                "ok": ok}, log_tail, {}


class Handler(BaseHTTPRequestHandler):
    server_version = "RetroPolyEngineQABridge/1.0"

    def _send(self, code, payload):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"
        qs = parse_qs(parsed.query)
        try:
            self.dispatch_get(path, qs)
        except Exception as error:  # noqa: BLE001 — отчёт по эндпоинту
            self._send(500, {"ok": False,
                             "error": "internal: %s" % error})

    def dispatch_get(self, path, qs):
        if path == "/api/health":
            self._send(200, {"ok": True, "status": "ok",
                             "service": "retropolyengine-qa-bridge",
                             "binaries": probe_binaries()})
        elif path == "/api/engine/info":
            info = {
                "ok": True,
                "project": "RetroPolyEngine",
                "language": "C (ISO C11 core) / C++14 engine",
                "qa_suites": {"unit": list(UNIT_SUITES),
                              "visual": list(VISUAL_SUITES)},
                "git_revision_file": ".git/HEAD read-only",
                "binaries": probe_binaries(),
            }
            head_path = os.path.join(REPO_ROOT, ".git/HEAD")
            if os.path.isfile(head_path):
                with open(head_path, "r") as head:
                    info["git_head"] = head.read().strip()
                # Символический ref → SHA (read-only разбор .git без git-CLI).
                match = re.match(r"ref: (.+)", info["git_head"])
                if match:
                    ref_file = os.path.join(REPO_ROOT, ".git",
                                            match.group(1).strip())
                    if os.path.isfile(ref_file):
                        with open(ref_file, "r") as ref:
                            info["git_head_sha"] = ref.read().strip()
            self._send(200, info)
        elif path == "/api/tests/run":
            if not _run_lock.acquire(blocking=False):
                self._send(503, {"ok": False, "error": "concurrent run busy"})
                return
            try:
                binary = (qs.get("binary") or ["unit"])[0]
                if binary not in ("unit", "visual"):
                    self._send(400, {"ok": False,
                                     "error": "binary: unit|visual"})
                    return
                suites_raw = (qs.get("suites") or ["all"])[0]
                allowed = UNIT_SUITES if binary == "unit" else VISUAL_SUITES
                if suites_raw == "all":
                    suites = list(allowed)
                else:
                    requested = [
                        QUOTE_RE.sub("", s).rstrip(".")
                        for s in suites_raw.split(",")
                    ]
                    bad = [s for s in requested
                           if s and s not in allowed]
                    if bad:
                        self._send(400, {"ok": False,
                                         "error": "unknown suites: %s"
                                                  % ",".join(bad),
                                         "allowed": list(allowed)})
                        return
                    suites = [s for s in requested if s]
                result, tail, extra = run_unit_or_visual(
                    binary, suites if suites else list(allowed))
                if result is None:
                    self._send(500, {"ok": False, "error": tail,
                                     **extra})
                    return
                payload = {"ok": result["failed"] == 0 and
                                    result["exit_code"] == 0,
                           "binary": binary,
                           **result, **extra}
                if result["failed"]:
                    payload["tail"] = tail
                self._send(200, payload)
            finally:
                _run_lock.release()
        elif path == "/api/assets/digest":
            self._send(200, {"ok": True, **assets_digest()})
        elif path == "/api/assets/determinism":
            result, err, extra = converter_determinism()
            if result is None:
                self._send(500, {"ok": False, "error": err, **extra})
            else:
                self._send(200, {"ok": result["mesh_identical"] and
                                          result["tex_identical"],
                                 "source": extra.get("source_dir"),
                                 **result})
        elif path == "/api/bench/smoke":
            result, tail, extra = bench_smoke()
            if result is None:
                self._send(500, {"ok": False, "error": tail, **extra})
            else:
                payload = {"ok": result.pop("ok"), **result, **extra}
                if result.get("exit_code") != 0:
                    payload["engine_log_tail"] = tail
                self._send(200, payload)
        else:
            self._send(404, {"ok": False, "error": "not found", "path": path})

    def do_POST(self):
        self._send(405, {"ok": False,
                         "error": "bridge exposes read-only GET endpoints"})

    def log_message(self, fmt, *args):
        sys.stderr.write("[bridge] %s - %s\n" % (self.address_string(),
                                                 fmt % args))


def main():
    os.chdir(REPO_ROOT)
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 5173
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    sys.stderr.write("[bridge] listening on 127.0.0.1:%d (root %s)\n"
                     % (port, REPO_ROOT))
    server.serve_forever()


if __name__ == "__main__":
    main()
