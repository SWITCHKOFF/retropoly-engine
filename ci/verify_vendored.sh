#!/bin/sh
# ============================================================================
# verify_vendored.sh — CI-проверка вендоренных зависимостей (Спринт 0 §3.6:
# «записать точный тег/commit и SHA-256 в VERSION.txt; CI проверяет хэши»).
#
# Проверяет: (1) submodule-пины против VERSION.txt, (2) SHA-256 одиночных
# вендоренных файлов против VERSION.txt. POSIX sh.
# Правило: обновление зависимости = отдельная задача с ADR; после pin локальные
# pull/update запрещены — при несовпадении фикс.
# ============================================================================

set -u

BINDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$BINDIR/.." && pwd)
cd -- "$ROOT" || { echo 'verify_vendored: FAIL: нет корня репозитория' >&2; exit 1; }

field() {
    sed -n "/^\[$1\]/,/^\[/p" VERSION.txt \
      | grep -E "^ *$2 *=" | head -n 1 \
      | sed "s/^ *$2 *= *//" | cut -d ';' -f1 | tr -d ' \t\r'
}

die() {
    printf 'verify_vendored: FAIL: %s\n' "$1" >&2
    exit 1
}

# --- 1) Submodules: pin по commit из VERSION.txt -----------------------------
for _s in sdl3 googletest psn00bsdk; do
    _expected=$(field "$_s" commit)
    [ -n "$_expected" ] || die "нет 'commit' в секции [$_s] VERSION.txt"
    _actual=$(git -C "src/third_party/$_s" rev-parse HEAD 2>/dev/null) \
        || die "$_s: submodule не инициализирован (git submodule update --init)"
    [ "$_actual" = "$_expected" ] \
        || die "$_s: pin сместился (VERSION.txt: $_expected; фактически: $_actual)"
    printf 'verify_vendored: OK  %s  pinned %s\n' "$_s" "$_actual"
done

# --- 2) Одиночные файлы: SHA-256 из VERSION.txt --------------------------------
check_file() {
    _section="$1"; _path_key="$2"; _sha_key="$3"
    _path=$(field "$_section" "$_path_key")
    _expected=$(field "$_section" "$_sha_key")
    [ -n "$_path" ] || die "[$_section]: нет '$_path_key' в VERSION.txt"
    [ -n "$_expected" ] || die "[$_section]: нет '$_sha_key' в VERSION.txt"
    [ -f "$_path" ] || die "$_path: файл отсутствует"
    _actual=$(sha256sum "$_path" | cut -d ' ' -f 1)
    [ "$_actual" = "$_expected" ] \
        || die "$_path: sha256 не совпал (ожидалось $_expected; фактически $_actual)"
    printf 'verify_vendored: OK  %s  sha256\n' "$_path"
}

check_file cgltf  path             sha256
check_file stb    path_image       sha256_image
check_file stb    path_image_write sha256_image_write
check_file glad   path_gl_h        sha256_gl_h
check_file glad   path_gl_c        sha256_gl_c
check_file glad   path_khrplatform_h sha256_khrplatform_h

printf 'verify_vendored: все пины и хэши совпадают с VERSION.txt\n'
