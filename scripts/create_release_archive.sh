#!/bin/bash
# SPDX-License-Identifier: MIT
# RetroPolyEngine — scripts/create_release_archive.sh
#
# Формирует релизный архив retro-engine-<tag>.tar.gz для GitHub Release.
# Тег передаётся первым аргументом; по умолчанию — текущий git-тег.

set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd -- "$ROOT" || exit 1

TAG="${1:-$(git describe --tags --exact-match 2>/dev/null || echo "v0.1.0")}"
OUT_DIR="$ROOT/dist/retro-engine-$TAG"
ARCHIVE="$ROOT/retro-engine-$TAG.tar.gz"

echo "Creating release archive for $TAG..."

rm -rf "$OUT_DIR" "$ARCHIVE"
mkdir -p "$OUT_DIR/pc" "$OUT_DIR/ps1"

cp -r "$ROOT/build/pc-release/game"/* "$OUT_DIR/pc/"
cp "$ROOT/build/ps1-release/src/hal/ps1/RETRO.bin" \
   "$ROOT/build/ps1-release/src/hal/ps1/RETRO.cue" \
   "$ROOT/build/ps1-release/src/hal/ps1/retro_ps1.exe" \
   "$OUT_DIR/ps1/"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$ROOT/docs/LEGAL.md" "$OUT_DIR/"

tar -czf "$ARCHIVE" -C "$ROOT/dist" "retro-engine-$TAG"
echo "Archive: $ARCHIVE"
