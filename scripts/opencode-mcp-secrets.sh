#!/usr/bin/env bash
# Управление ключами API для MCP-серверов OpenCode (Context7, TestSprite).
#
# Ключи хранятся ВНЕ git-репозитория:
#   ~/.config/opencode/secrets/<name>
# Каталог: 0700, файлы: 0600, владелец — текущий пользователь.
# В opencode.json попадают только ссылки {file:...}, а не сами ключи.
#
# Ввод ключа скрыт (read -s), поэтому он не попадает в историю shell,
# в аргументы процессов и в глобальное окружение пользователя.

set -euo pipefail

readonly CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
readonly SECRETS_DIR="$CONFIG_HOME/opencode/secrets"

# name|описание|обязательность
readonly -a SECRET_SPECS=(
  "context7_api_key|Context7 API key (необязателен; Enter — пропустить)|optional"
  "testsprite_api_key|TestSprite API key (обязателен для testsprite)|required"
)

umask 077

CLEANUP_FILE=""
cleanup() {
  if [[ -n "${CLEANUP_FILE:-}" ]]; then
    rm -f -- "$CLEANUP_FILE"
  fi
}
trap cleanup EXIT

die() { printf 'Ошибка: %s\n' "$*" >&2; exit 1; }
info() { printf '%s\n' "$*"; }

usage() {
  cat <<'EOF'
Использование:
  scripts/opencode-mcp-secrets.sh set [context7|testsprite|all]
  scripts/opencode-mcp-secrets.sh status
  scripts/opencode-mcp-secrets.sh clear [context7|testsprite|all]

Команды:
  set     безопасно записать ключ(и); ввод скрыт и не попадает в history
  status  показать, какие ключи сохранены, их права и размер (без значений)
  clear   удалить сохранённые ключи

Ключи лежат в ~/.config/opencode/secrets/ (0700/0600) и не попадают в git.
EOF
}

spec_for() {
  local spec name label required
  for spec in "${SECRET_SPECS[@]}"; do
    IFS='|' read -r name label required <<<"$spec"
    if [[ "$name" == "$1" ]]; then
      printf '%s|%s' "$label" "$required"
      return 0
    fi
  done
  return 1
}

ensure_dir() {
  local repo_root real_dir
  if [[ -L "$SECRETS_DIR" ]]; then
    die "$SECRETS_DIR — символическая ссылка; отказ (защита от подмены пути)."
  fi
  if [[ ! -e "$SECRETS_DIR" ]]; then
    mkdir -p -- "$SECRETS_DIR"
  fi
  [[ -d "$SECRETS_DIR" ]] || die "$SECRETS_DIR существует, но это не каталог."
  chmod 700 -- "$SECRETS_DIR"
  [[ "$(stat -c '%u' -- "$SECRETS_DIR")" == "$(id -u)" ]] \
    || die "$SECRETS_DIR принадлежит не текущему пользователю."

  # Страховка: каталог секретов ни при каких условиях не должен оказаться в репозитории.
  repo_root="$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel 2>/dev/null || true)"
  real_dir="$(realpath -m -- "$SECRETS_DIR")"
  if [[ -n "$repo_root" && "$real_dir" == "$repo_root"/* ]]; then
    die "Каталог секретов ($real_dir) находится внутри git-репозитория ($repo_root)."
  fi
}

trim() {
  local value="$1"
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  printf '%s' "$value"
}

write_secret() {
  local name="$1" label="$2" value tmp target last_byte

  printf '\n%s\n' "$label"
  printf 'Вставьте ключ (ввод скрыт, пусто — пропустить): '
  IFS= read -r -s value || true
  printf '\n'

  value="$(trim "$value")"
  if [[ -z "$value" ]]; then
    info "Пропущено: $name"
    return 0
  fi
  if printf '%s' "$value" | LC_ALL=C grep -q '[[:cntrl:]]'; then
    die "Ключ содержит управляющие символы (перевод строки, табуляцию и т.п.). Вставьте одной строкой."
  fi

  ensure_dir
  target="$SECRETS_DIR/$name"
  if [[ -L "$target" ]]; then
    die "$target — символическая ссылка; отказ."
  fi

  # Атомарная запись: временный файл в том же каталоге, затем rename.
  tmp="$(mktemp -- "$SECRETS_DIR/.$name.XXXXXX")"
  CLEANUP_FILE="$tmp"
  chmod 600 -- "$tmp"
  printf '%s' "$value" >"$tmp"

  # Контроль до публикации: файл не должен заканчиваться переводом строки/пробелом.
  last_byte="$(tail -c 1 -- "$tmp" | od -An -tu1 | tr -d '[:space:]')"
  if [[ "$last_byte" == "10" || "$last_byte" == "13" || "$last_byte" == "32" || "$last_byte" == "9" ]]; then
    die "В конце ключа обнаружен пробельный символ. Ключ не сохранён."
  fi

  mv -f -- "$tmp" "$target"
  CLEANUP_FILE=""
  chmod 600 -- "$target"

  info "Сохранено: $target (0600, вне git)"
}

show_status() {
  local name label required path perms owner size state last_byte
  info "Каталог секретов: $SECRETS_DIR"
  if [[ -L "$SECRETS_DIR" ]]; then
    info "  ВНИМАНИЕ: каталог — символическая ссылка."
  elif [[ -d "$SECRETS_DIR" ]]; then
    info "  права=$(stat -c '%a' -- "$SECRETS_DIR") владелец=$(stat -c '%U' -- "$SECRETS_DIR")"
  else
    info "  каталог ещё не создан."
  fi
  info ""

  for name in context7_api_key testsprite_api_key; do
    IFS='|' read -r label required <<<"$(spec_for "$name")"
    path="$SECRETS_DIR/$name"
    state="OK"
    if [[ -L "$path" ]]; then
      state="ССЫЛКА (небезопасно)"
    elif [[ ! -e "$path" ]]; then
      state="НЕ СОЗДАН"
    elif [[ ! -f "$path" ]]; then
      state="НЕ ОБЫЧНЫЙ ФАЙЛ"
    else
      perms="$(stat -c '%a' -- "$path")"
      owner="$(stat -c '%U' -- "$path")"
      size="$(stat -c '%s' -- "$path")"
      if [[ "$owner" != "$(id -un)" ]]; then
        state="ВЛАДЕЛЕЦ НЕ ВЫ"
      elif (( (8#$perms & 8#077) != 0 )); then
        state="ПРАВА ШИРЕ 0600"
      elif (( size == 0 )); then
        state="ПУСТО (ключ не вставлен)"
      else
        last_byte="$(tail -c 1 -- "$path" | od -An -tu1 | tr -d '[:space:]')"
        if [[ "$last_byte" == "10" || "$last_byte" == "13" || "$last_byte" == "32" || "$last_byte" == "9" ]]; then
          state="ЕСТЬ ПРОБЕЛ/NEWLINE В КОНЦЕ"
        fi
      fi
      printf '  %-22s %-28s права=%s владелец=%s размер=%s Б\n' \
        "$name" "$state" "$perms" "$owner" "$size"
      continue
    fi
    if [[ "$required" == required ]]; then
      printf '  %-22s %-28s (обязателен)\n' "$name" "$state"
    else
      printf '  %-22s %-28s (необязателен)\n' "$name" "$state"
    fi
  done
  info ""
  info "Значения ключей намеренно не выводятся."
}

clear_secret() {
  local name="$1" path="$SECRETS_DIR/$name"
  if [[ -e "$path" || -L "$path" ]]; then
    rm -f -- "$path"
    info "Удалено: $path"
  else
    info "Нет файла: $path"
  fi
}

main() {
  local cmd="${1:-}" target="${2:-all}" name label required answer
  local -a names=()
  case "$target" in
    context7) names=(context7_api_key) ;;
    testsprite) names=(testsprite_api_key) ;;
    all) names=(context7_api_key testsprite_api_key) ;;
    *) die "Неизвестный сервер: $target (допустимо: context7, testsprite, all)" ;;
  esac
  case "$cmd" in
    set)
      ensure_dir
      for name in "${names[@]}"; do
        IFS='|' read -r label required <<<"$(spec_for "$name")"
        write_secret "$name" "$label"
      done
      info ""
      info "Готово. Проверка: $0 status"
      info "Затем в OpenCode выполните /reload (или перезапустите opencode), чтобы MCP перечитали ключи."
      ;;
    status)
      show_status
      ;;
    clear)
      printf 'Удалить ключи (%s)? [y/N] ' "$target"
      read -r answer || true
      [[ "$answer" == [yY] ]] || { info "Отменено."; exit 0; }
      for name in "${names[@]}"; do
        clear_secret "$name"
      done
      ;;
    ""|-h|--help|help)
      usage
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
}

main "$@"
