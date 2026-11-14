#!/bin/bash
# SPDX-License-Identifier: MIT
# ============================================================================
# RetroPolyEngine — scripts/create_release_archive.sh (v2, Спринт 37–38)
#
# Собирает публичный payload v0.2.1 через scripts/build_release_bundle.py:
#   dist/retro-engine-v<ver>/     sdk/ + ui/ + checksums + report + docs
#   dist/retro-engine-v<ver>.tar.gz (+ .sha256)
#
# Demo-образы допустимы ТОЛЬКО как optional demos/ (--demos-dir). Без
# SDK/UI-архивов скрипт завершается ошибкой: demo-архив не является релизом
# (docs/RELEASING.md).
#
# Использование:
#   ./scripts/create_release_archive.sh v0.2.1
#   ./scripts/create_release_archive.sh v0.2.1 \
#       --sdk-archive dist/retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz \
#       --ui-archive  dist/retro-engine-ui-0.2.1-linux-x86_64.tar.gz \
#       --demos-dir   dist/demos
# ============================================================================

set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd -- "$ROOT" || exit 1

TAG="${1:-$(git describe --tags --exact-match 2>/dev/null || echo "v0.2.1")}"
shift || true
VERSION="${TAG#v}"

args=(--version "$VERSION")
if [ -d "$ROOT/dist/demos" ]; then
    args+=(--demos-dir "$ROOT/dist/demos")
fi

echo "Building SDK+UI release payload for $TAG..."
exec python3 "$ROOT/scripts/build_release_bundle.py" "${args[@]}" "$@"
