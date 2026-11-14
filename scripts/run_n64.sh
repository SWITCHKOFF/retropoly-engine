#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/run_n64.sh (S39–42, ADR-014)
#
# Запуск boot-demo в pinned эмуляторе gopher64 1.2.1. Эмулятор и ROM
# НЕ вендорятся: gopher64 устанавливается пользователем, ROM собирается
# пресетом n64-release. BIOS не нужен (N64 — картриджная платформа).
#
# Использование:
#   scripts/run_n64.sh [ROM.z64]
#   GOPHER64_BIN=/path/to/gopher64 scripts/run_n64.sh
#
# Ручной gate: 60 с, экран 320×240 с текстом BOOT OK, DFS/memory overlay,
# heartbeat "frame=... time=... reload=1 staging=0" каждые ~5 с; input-лог
# "[n64_input] pad0 press ..."; 10-минутный stress — тот же запуск.
# Выход — закрыть окно/эмулятор.
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROM="${1:-${REPO_ROOT}/build/n64-release/src/hal/n64/hello_n64.z64}"
EMU="${GOPHER64_BIN:-gopher64}"

if [[ ! -f "${ROM}" ]]; then
    echo "run_n64: ROM не найден: ${ROM}" >&2
    echo "  Соберите: cmake --preset n64-release && cmake --build --preset n64-release" >&2
    exit 1
fi

if [[ "${EMU}" == */* ]]; then
    if [[ ! -x "${EMU}" ]]; then
        echo "run_n64: GOPHER64_BIN не исполняемый: ${EMU}" >&2
        exit 1
    fi
elif ! command -v "${EMU}" >/dev/null 2>&1; then
    echo "run_n64: gopher64 не найден в PATH." >&2
    echo "  Установите pinned gopher64 1.2.1 (VERSION.txt) и задайте:" >&2
    echo "    export GOPHER64_BIN=/path/to/gopher64" >&2
    echo "  См. docs/BUILDING.md § Nintendo 64." >&2
    exit 1
fi

# 8 MiB RDRAM (Expansion Pak) — reference environment ADR-014; без overclock.
exec "${EMU}" --disable-expansion-pak false --overclock false "${ROM}"
