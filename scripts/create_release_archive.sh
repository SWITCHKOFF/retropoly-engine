#!/bin/bash
# SPDX-License-Identifier: MIT
# RetroPolyEngine — scripts/create_release_archive.sh
#
# Формирует legacy demo-архив retro-engine-<tag>.tar.gz.
# ВНИМАНИЕ: по docs/RELEASING.md это НЕ публичный SDK-релиз; для релиза
# обязательны sdk/ (headers/libs/CMake package/toolchain/examples) и ui/.
# Тег передаётся первым аргументом; по умолчанию — текущий git-тег.

set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd -- "$ROOT" || exit 1

TAG="${1:-$(git describe --tags --exact-match 2>/dev/null || echo "v0.2.0")}"
OUT_DIR="$ROOT/dist/retro-engine-$TAG"
ARCHIVE="$ROOT/retro-engine-$TAG.tar.gz"

echo "Creating release archive for $TAG..."

rm -rf "$OUT_DIR" "$ARCHIVE"
mkdir -p "$OUT_DIR/pc" "$OUT_DIR/ps1" "$OUT_DIR/saturn"

cp -r "$ROOT/build/pc-release/game"/* "$OUT_DIR/pc/"
cp "$ROOT/build/ps1-release/src/hal/ps1/RETRO.bin" \
   "$ROOT/build/ps1-release/src/hal/ps1/RETRO.cue" \
   "$ROOT/build/ps1-release/src/hal/ps1/retro_ps1.exe" \
   "$OUT_DIR/ps1/"
cp "$ROOT/build/saturn-release/src/hal/saturn/hello_triangle.elf" \
   "$ROOT/build/saturn-release/src/hal/saturn/hello_triangle.bin" \
   "$ROOT/build/saturn-release/src/hal/saturn/hello_triangle.iso" \
   "$ROOT/build/saturn-release/src/hal/saturn/hello_triangle.cue" \
   "$ROOT/build/saturn-release/src/hal/saturn/hello_triangle.map" \
   "$OUT_DIR/saturn/"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$ROOT/docs/LEGAL.md" "$OUT_DIR/"

tar -czf "$ARCHIVE" -C "$ROOT/dist" "retro-engine-$TAG"
echo "Archive: $ARCHIVE"
