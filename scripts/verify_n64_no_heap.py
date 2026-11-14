#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/verify_n64_no_heap.py (Спринт 41–42)
#
# Статическая проверка запрета heap в N64 HAL: new/delete/malloc/calloc/
# realloc/free не должны вызываться из src/hal/n64/*.cc|cpp|h. libdragon
# внутри себя использует malloc для DFS-хэндлов/шрифтов и boot-heap — это
# SDK-reserve, а не аллокации движка (docs/N64.md §6).
#
# Использование:
#   python3 scripts/verify_n64_no_heap.py
#
# Exit code: 0 — нарушений нет; 1 — найдены запрещённые вызовы.
# ============================================================================
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
N64_DIR = ROOT / "src" / "hal" / "n64"
FORBIDDEN = re.compile(r"\b(?:malloc|calloc|realloc|free)\s*\(|\bnew\b|\bdelete\b")


def strip_comments_and_strings(text):
    """Убрать //, /* */ и строковые/символьные литералы (простая FSM)."""
    out = []
    i = 0
    n = len(text)
    state = "code"
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == "/" and nxt == "/":
                state = "line"
                i += 2
                continue
            if c == "/" and nxt == "*":
                state = "block"
                i += 2
                continue
            if c in ('"', "'"):
                state = "string"
                quote = c
                out.append(" ")
                i += 1
                continue
            out.append(c)
            i += 1
        elif state == "line":
            if c == "\n":
                state = "code"
                out.append("\n")
            i += 1
        elif state == "block":
            if c == "*" and nxt == "/":
                state = "code"
                i += 2
                continue
            i += 1
        else:  # string
            if c == "\\":
                i += 2
                continue
            if c == quote:
                state = "code"
            i += 1
    return "".join(out)


def main():
    if not N64_DIR.is_dir():
        print("FAIL: src/hal/n64 отсутствует")
        return 1

    violations = []
    files = sorted(
        p for p in N64_DIR.iterdir() if p.suffix in (".c", ".cc", ".cpp", ".h", ".hpp")
    )
    for path in files:
        code = strip_comments_and_strings(path.read_text(encoding="utf-8"))
        for lineno, line in enumerate(code.splitlines(), 1):
            if "= delete" in line or "=delete" in line:
                continue  # C++ deleted functions — не heap-аллокация.
            if FORBIDDEN.search(line):
                rel = path.relative_to(ROOT)
                violations.append(f"{rel}:{lineno}: {line.strip()}")

    if violations:
        print("FAIL: heap-аллокации в N64 HAL запрещены (ADR-014/S41–42)")
        for item in violations:
            print("  " + item)
        return 1

    print(f"PASS: heap-вызовов нет ({len(files)} файлов src/hal/n64)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
