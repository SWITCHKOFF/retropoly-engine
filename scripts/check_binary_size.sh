#!/bin/sh
# SPDX-License-Identifier: MIT
# RetroPolyEngine — scripts/check_binary_size.sh
#
# Гейт размера PS-EXE (Спринт 15–16, неделя 32): лимит 500 KiB / 512000 байт.
# Путь к PS-EXE можно передать аргументом; по умолчанию — артефакт канонического
# пресета ps1-release.

set -e

EXE="${1:-build/ps1-release/src/hal/ps1/retro_ps1.exe}"
LIMIT=512000

if [ ! -f "$EXE" ]; then
    echo "ERROR: PS-EXE не найден: $EXE"
    exit 2
fi

SIZE="$(wc -c < "$EXE")"
if [ "$SIZE" -gt "$LIMIT" ]; then
    echo "FAIL: PS-EXE $SIZE bytes exceeds limit $LIMIT bytes ($EXE)"
    exit 1
fi

echo "OK: PS-EXE $SIZE bytes <= $LIMIT bytes ($EXE)"
