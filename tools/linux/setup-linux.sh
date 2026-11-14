#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# ============================================================================
# setup-linux.sh — установка инструментария RetroPolyEngine на Linux.
#
# Закрывает пробелы docs/BUILDING.md для Debian / Ubuntu / Linux Mint:
#   1. системные пакеты (CMake, Ninja, OpenGL/X11/audio dev, QA/docs);
#   2. инициализация git-подмодулей (SDL3, GoogleTest, PSn00bSDK);
#   3. внешний PSn00bSDK v0.24 (pinned, SHA-256 из VERSION.txt);
#   4. Jo Engine v2026.2 (pinned commit из VERSION.txt);
#   5. переменные окружения PSN00BSDK_* / JO_ENGINE_ROOT.
#
# Скрипт идемпотентен: повторный запуск пропускает уже установленное.
# Запускать от обычного пользователя — sudo для apt запрашивается внутри.
#
# Примеры:
#   tools/linux/setup-linux.sh --check       # только диагностика
#   tools/linux/setup-linux.sh --yes         # установить всё
#   tools/linux/setup-linux.sh --pc-only     # только PC-зависимости
# ============================================================================

set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"

# --- Пины внешних тулчейнов (синхронизированы с VERSION.txt) ----------------
readonly PS1_VERSION="0.24"
readonly PS1_URL="https://github.com/Lameguy64/PSn00bSDK/releases/download/v${PS1_VERSION}/PSn00bSDK-${PS1_VERSION}-Linux.zip"
readonly PS1_SHA256="5ada7c9478d795b22bde96ee23d6869290c14a740bcde3d57c01c342a26ded35"
readonly PS1_GCC_VERSION="12.3.0"

readonly JO_TAG="v2026.2"
readonly JO_COMMIT="5e99944238030821a48e17a40abefd77e81953c2"
readonly JO_URL="https://github.com/johannes-fetz/joengine.git"

readonly CONFIG_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/retro_engine/env.sh"

# Системные пакеты: сборка PC + тесты + документация + Saturn ISO.
readonly -a APT_PACKAGES=(
    build-essential
    git
    curl
    unzip
    pkg-config
    cmake
    ninja-build
    python3
    python3-pip
    python3-venv
    python3-yaml
    libgl1-mesa-dev
    libegl1-mesa-dev
    libx11-dev
    libxext-dev
    libxrandr-dev
    libxcursor-dev
    libxi-dev
    libxfixes-dev
    libxss-dev
    libxtst-dev
    libxkbcommon-dev
    libwayland-dev
    libdecor-0-dev
    libasound2-dev
    libpulse-dev
    libudev-dev
    libdbus-1-dev
    xvfb
    lcov
    doxygen
    graphviz
    clang-format
    genisoimage
)

# --- Опции -------------------------------------------------------------------
ASSUME_YES=0
DRY_RUN=0
CHECK_ONLY=0
SKIP_APT=0
SKIP_PS1=0
SKIP_SATURN=0
NO_SHELL_CONFIG=0
FORCE_PS1=0
APT_MIRROR=""
PS1_DIR="${PSN00BSDK_ROOT:-$HOME/PSn00bSDK}"
JO_DIR="${JO_ENGINE_ROOT:-$HOME/joengine}"

FAILURES=0
WARNINGS=0
TMP_DIR=""
APT_SOURCES_FILE=""
SUDO=()

# --- Вывод -------------------------------------------------------------------
if [[ -t 1 ]]; then
    C_RESET=$'\033[0m'
    C_RED=$'\033[31m'
    C_GREEN=$'\033[32m'
    C_YELLOW=$'\033[33m'
    C_CYAN=$'\033[36m'
else
    C_RESET=""
    C_RED=""
    C_GREEN=""
    C_YELLOW=""
    C_CYAN=""
fi

info() { printf '%s\n' "$*"; }
step() { printf '\n%s== %s ==%s\n' "$C_CYAN" "$*" "$C_RESET"; }
ok() { printf '  %s[OK]%s   %s\n' "$C_GREEN" "$C_RESET" "$*"; }
warn() {
    WARNINGS=$((WARNINGS + 1))
    printf '  %s[WARN]%s %s\n' "$C_YELLOW" "$C_RESET" "$*"
}
fail() {
    FAILURES=$((FAILURES + 1))
    printf '  %s[FAIL]%s %s\n' "$C_RED" "$C_RESET" "$*"
}
skip() { printf '  [SKIP] %s\n' "$*"; }
die() { printf 'Ошибка: %s\n' "$*" >&2; exit 1; }

cleanup() {
    if [[ -n "$TMP_DIR" && -d "$TMP_DIR" ]]; then
        rm -rf -- "$TMP_DIR"
    fi
    if [[ -n "$APT_SOURCES_FILE" && -f "$APT_SOURCES_FILE" ]]; then
        rm -f -- "$APT_SOURCES_FILE"
    fi
}
trap cleanup EXIT

usage() {
    cat <<'EOF'
Использование:
  tools/linux/setup-linux.sh [опции]

Опции:
  --check             только проверить окружение, ничего не устанавливать
  --dry-run, -n       показать план, не вносить изменения
  --yes, -y           не спрашивать подтверждение
  --pc-only           пропустить PS1 и Saturn
  --skip-ps1          не устанавливать PSn00bSDK
  --skip-saturn       не устанавливать Jo Engine
  --skip-apt          не устанавливать системные пакеты
  --no-shell-config   не подключать env к ~/.bashrc / ~/.zshrc
  --force-ps1         переустановить PSn00bSDK (старый каталог — в .bak)
  --apt-mirror URL    временный Ubuntu-зеркало для apt (например,
                      http://mirror.yandex.ru/ubuntu) — системные sources
                      не изменяются
  --ps1-dir DIR       каталог PSn00bSDK (по умолчанию $HOME/PSn00bSDK)
  --joengine-dir DIR  каталог Jo Engine (по умолчанию $HOME/joengine)
  -h, --help          эта справка

Переменные PSN00BSDK_ROOT и JO_ENGINE_ROOT учитываются как значения
по умолчанию. Пресеты и пины описаны в docs/BUILDING.md и VERSION.txt.
EOF
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1
}

run() {
    if (( DRY_RUN )); then
        printf '  DRY-RUN:'
        printf ' %q' "$@"
        printf '\n'
    else
        "$@"
    fi
}

first_version() {
    local out=""
    out="$("$@" 2>/dev/null | head -n 1)" || true
    if [[ "$out" =~ ([0-9]+(\.[0-9]+)+) ]]; then
        printf '%s\n' "${BASH_REMATCH[1]}"
    fi
}

version_ge() {
    local have="$1" need="$2"
    [[ "$(printf '%s\n%s\n' "$need" "$have" | sort -V | head -n 1)" == "$need" ]]
}

parse_args() {
    while (( $# > 0 )); do
        case "$1" in
            --check) CHECK_ONLY=1 ;;
            --dry-run|-n) DRY_RUN=1 ;;
            --yes|-y) ASSUME_YES=1 ;;
            --pc-only) SKIP_PS1=1; SKIP_SATURN=1 ;;
            --skip-ps1) SKIP_PS1=1 ;;
            --skip-saturn) SKIP_SATURN=1 ;;
            --skip-apt) SKIP_APT=1 ;;
            --no-shell-config) NO_SHELL_CONFIG=1 ;;
            --force-ps1) FORCE_PS1=1 ;;
            --apt-mirror)
                [[ $# -ge 2 ]] || die "--apt-mirror требует аргумент"
                APT_MIRROR="${2%/}"
                shift
                ;;
            --ps1-dir)
                [[ $# -ge 2 ]] || die "--ps1-dir требует аргумент"
                PS1_DIR="$2"
                shift
                ;;
            --joengine-dir)
                [[ $# -ge 2 ]] || die "--joengine-dir требует аргумент"
                JO_DIR="$2"
                shift
                ;;
            -h|--help) usage; exit 0 ;;
            *) die "неизвестный аргумент: $1 (см. --help)" ;;
        esac
        shift
    done
}

confirm() {
    (( ASSUME_YES )) && return 0
    [[ -t 0 ]] || die "нет TTY для подтверждения; используйте --yes"
    local answer=""
    printf 'Продолжить? [y/N] '
    read -r answer || true
    [[ "$answer" == [yY] ]]
}

detect_debian() {
    [[ -r /etc/os-release ]] \
        || die "/etc/os-release не найден — поддерживаются Debian/Ubuntu/Mint"
    # shellcheck disable=SC1091
    . /etc/os-release
    case " ${ID:-} ${ID_LIKE:-} " in
        *ubuntu*|*debian*) ;;
        *) die "поддерживаются Debian/Ubuntu/Mint; сейчас: ${PRETTY_NAME:-unknown}" ;;
    esac
    require_cmd apt-get || die "apt-get не найден"
    require_cmd dpkg-query || die "dpkg-query не найден"
}

setup_sudo() {
    if [[ "$(id -u)" -eq 0 ]]; then
        die "запустите скрипт от обычного пользователя: sudo для apt вызовется сам"
    fi
    require_cmd sudo || die "sudo не найден, а установка пакетов требует root"
    SUDO=(sudo)
}

install_apt_packages() {
    step "1/5. Системные пакеты (apt)"

    if (( SKIP_APT )); then
        skip "пропущено (--skip-apt)"
        return 0
    fi

    detect_debian

    local -a missing=()
    local pkg
    for pkg in "${APT_PACKAGES[@]}"; do
        if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null \
                | grep -q 'ok installed'; then
            missing+=("$pkg")
        fi
    done

    if (( ${#missing[@]} == 0 )); then
        ok "все пакеты уже установлены"
        return 0
    fi

    info "Не хватает пакетов: ${missing[*]}"

    local -a apt_opts=()
    if [[ -n "$APT_MIRROR" ]]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        local codename="${UBUNTU_CODENAME:-${VERSION_CODENAME:-}}"
        [[ -n "$codename" ]] \
            || die "не удалось определить Ubuntu codename для --apt-mirror"
        APT_SOURCES_FILE="$(mktemp "${TMPDIR:-/tmp}/rpe-apt.XXXXXX.list")"
        {
            printf 'deb %s %s main restricted universe multiverse\n' \
                "$APT_MIRROR" "$codename"
            printf 'deb %s %s-updates main restricted universe multiverse\n' \
                "$APT_MIRROR" "$codename"
            printf 'deb %s %s-backports main restricted universe multiverse\n' \
                "$APT_MIRROR" "$codename"
            printf 'deb %s %s-security main restricted universe multiverse\n' \
                "$APT_MIRROR" "$codename"
        } >"$APT_SOURCES_FILE"
        apt_opts=(
            -o "Dir::Etc::sourcelist=$APT_SOURCES_FILE"
            -o "Dir::Etc::sourceparts=-"
            -o "APT::Get::List-Cleanup=0"
        )
        info "apt-зеркало: $APT_MIRROR ($codename), системные sources не меняются"
    fi

    run "${SUDO[@]}" apt-get "${apt_opts[@]}" update
    run "${SUDO[@]}" apt-get "${apt_opts[@]}" install -y "${missing[@]}"

    if (( DRY_RUN )); then
        skip "dry-run: установка не выполнялась"
        return 0
    fi

    hash -r
    ok "системные пакеты установлены"
}

ensure_submodules() {
    step "2/5. Git-подмодули"

    if ! git -C "$REPO_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        warn "репозиторий не является git-checkout — submodules не проверяются"
        return 0
    fi

    # Только верхний уровень: вложенные submodules PSn00bSDK по VERSION.txt
    # намеренно не инициализируются (pin psn00bsdk-tree).
    local uninit=0
    uninit="$(git -C "$REPO_ROOT" submodule status 2>/dev/null \
        | grep -c '^-' || true)"
    if [[ "$uninit" == "0" ]]; then
        ok "все верхнеуровневые submodules инициализированы"
        return 0
    fi

    info "Неинициализированных submodules: $uninit"
    run git -C "$REPO_ROOT" submodule update --init
    if (( DRY_RUN )); then
        skip "dry-run: submodules не обновлялись"
        return 0
    fi
    ok "submodules инициализированы"
}

install_ps1() {
    step "3/5. PSn00bSDK ${PS1_VERSION}"

    if (( SKIP_PS1 )); then
        skip "пропущено (--skip-ps1)"
        return 0
    fi

    if [[ -x "$PS1_DIR/bin/mipsel-none-elf-gcc" \
            && -f "$PS1_DIR/lib/libpsn00b/cmake/sdk.cmake" ]]; then
        if (( ! FORCE_PS1 )); then
            ok "уже установлен: $PS1_DIR"
            return 0
        fi
        info "--force-ps1: переустановка $PS1_DIR"
    fi

    if (( DRY_RUN )); then
        info "DRY-RUN: скачать $PS1_URL"
        info "DRY-RUN: проверить SHA-256 ($PS1_SHA256)"
        info "DRY-RUN: распаковать в $PS1_DIR"
        return 0
    fi

    TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/rpe-ps1.XXXXXX")"
    local archive="$TMP_DIR/psn00bsdk.zip"
    local extract_dir="$TMP_DIR/extract"
    local sdk_lib sdk_root backup

    info "Скачивание PSn00bSDK ${PS1_VERSION} (~76 MiB)..."
    curl -fL --retry 3 --retry-delay 2 --connect-timeout 20 \
        -o "$archive" "$PS1_URL"

    info "Проверка SHA-256..."
    printf '%s  %s\n' "$PS1_SHA256" "$archive" | sha256sum -c -

    info "Распаковка..."
    mkdir -p -- "$extract_dir"
    unzip -q "$archive" -d "$extract_dir"

    sdk_lib="$(find "$extract_dir" -type d -path '*/lib/libpsn00b' -print -quit)"
    [[ -n "$sdk_lib" ]] \
        || die "в архиве PSn00bSDK не найден каталог lib/libpsn00b"
    sdk_root="$(dirname -- "$(dirname -- "$sdk_lib")")"
    [[ -x "$sdk_root/bin/mipsel-none-elf-gcc" ]] \
        || die "в архиве PSn00bSDK не найден bin/mipsel-none-elf-gcc"

    if [[ -e "$PS1_DIR" ]]; then
        backup="${PS1_DIR}.bak.$(date +%Y%m%d-%H%M%S).$$"
        info "Существующий каталог сохранён: $backup"
        mv -- "$PS1_DIR" "$backup"
    fi
    mkdir -p -- "$(dirname -- "$PS1_DIR")"
    mv -- "$sdk_root" "$PS1_DIR"

    local gcc_ver
    gcc_ver="$(first_version "$PS1_DIR/bin/mipsel-none-elf-gcc" --version)"
    if [[ "$gcc_ver" == "$PS1_GCC_VERSION" ]]; then
        ok "PSn00bSDK установлен: $PS1_DIR (GCC $gcc_ver)"
    else
        warn "PSn00bSDK установлен, но GCC $gcc_ver != $PS1_GCC_VERSION"
    fi
}

install_saturn() {
    step "4/5. Jo Engine ${JO_TAG}"

    if (( SKIP_SATURN )); then
        skip "пропущено (--skip-saturn)"
        return 0
    fi

    if [[ -d "$JO_DIR/.git" ]]; then
        local current
        current="$(git -C "$JO_DIR" rev-parse HEAD 2>/dev/null || true)"
        if [[ "$current" == "$JO_COMMIT" ]]; then
            ok "уже установлен: $JO_DIR @ ${current:0:7}"
        else
            warn "в $JO_DIR commit ${current:0:7} != ${JO_COMMIT:0:7}"
            warn "каталог не изменяю: переключитесь на пин вручную"
            return 0
        fi
    elif [[ -e "$JO_DIR" ]]; then
        die "$JO_DIR существует, но не является git-репозиторием; укажите --joengine-dir"
    else
        if (( DRY_RUN )); then
            info "DRY-RUN: git clone --depth 1 --branch $JO_TAG $JO_URL $JO_DIR"
            return 0
        fi
        mkdir -p -- "$(dirname -- "$JO_DIR")"
        info "Клонирование Jo Engine ${JO_TAG}..."
        git clone --depth 1 --branch "$JO_TAG" "$JO_URL" "$JO_DIR"

        local current
        current="$(git -C "$JO_DIR" rev-parse HEAD)"
        [[ "$current" == "$JO_COMMIT" ]] \
            || die "Jo Engine: commit $current != $JO_COMMIT (пин из VERSION.txt)"
        ok "Jo Engine установлен: $JO_DIR @ ${current:0:7}"
    fi

    [[ -x "$JO_DIR/Compiler/LINUX/bin/sh-none-elf-gcc-8.2.0" ]] \
        || die "Jo Engine: нет Compiler/LINUX/bin/sh-none-elf-gcc-8.2.0"
    [[ -f "$JO_DIR/Compiler/COMMON/jo.linker" ]] \
        || die "Jo Engine: нет Compiler/COMMON/jo.linker"
    [[ -d "$JO_DIR/jo_engine" ]] \
        || die "Jo Engine: нет каталога jo_engine"
}

configure_shell() {
    step "5/5. Переменные окружения"

    if (( NO_SHELL_CONFIG )); then
        skip "пропущено (--no-shell-config)"
        return 0
    fi

    if (( DRY_RUN )); then
        info "DRY-RUN: записать $CONFIG_FILE"
        info "DRY-RUN: подключить его в ~/.bashrc и ~/.zshrc"
        return 0
    fi

    mkdir -p -- "$(dirname -- "$CONFIG_FILE")"
    {
        printf '# Сгенерировано %s/tools/linux/setup-linux.sh\n' "$REPO_ROOT"
        printf 'export PSN00BSDK_ROOT=%q\n' "$PS1_DIR"
        printf 'export PSN00BSDK_LIBS="$PSN00BSDK_ROOT/lib/libpsn00b"\n'
        printf 'export JO_ENGINE_ROOT=%q\n' "$JO_DIR"
        printf 'case ":$PATH:" in\n'
        printf '    *":$PSN00BSDK_ROOT/bin:"*) ;;\n'
        printf '    *) export PATH="$PSN00BSDK_ROOT/bin:$PATH" ;;\n'
        printf 'esac\n'
    } >"$CONFIG_FILE"
    ok "записан: $CONFIG_FILE"

    local rc marker_begin="# >>> RetroPolyEngine toolchain >>>"
    local marker_end="# <<< RetroPolyEngine toolchain <<<"
    for rc in "$HOME/.bashrc" "$HOME/.zshrc"; do
        [[ -f "$rc" ]] || continue
        if grep -qF -- "$marker_begin" "$rc"; then
            ok "уже подключено: $rc"
            continue
        fi
        {
            printf '\n%s\n' "$marker_begin"
            printf 'if [ -f %q ]; then . %q; fi\n' "$CONFIG_FILE" "$CONFIG_FILE"
            printf '%s\n' "$marker_end"
        } >>"$rc"
        ok "подключено: $rc"
    done
    info "В текущем терминале: source ~/.bashrc (или новый терминал)"
}

check_tool_version() {
    local label="$1" cmd="$2" need="$3"
    shift 3
    if ! require_cmd "$cmd"; then
        if (( SKIP_APT )); then
            skip "$label: не установлен (--skip-apt)"
        else
            fail "$label не найден"
        fi
        return 0
    fi
    local ver
    ver="$(first_version "$@" || true)"
    if [[ -z "$ver" ]]; then
        fail "$label: не удалось определить версию"
    elif version_ge "$ver" "$need"; then
        ok "$label $ver (>= $need)"
    else
        fail "$label $ver < $need"
    fi
}

run_checks() {
    step "Проверка окружения"

    check_tool_version "CMake" cmake 3.25 cmake --version
    check_tool_version "Ninja" ninja 1.11 ninja --version
    check_tool_version "GCC" gcc 12 gcc --version
    check_tool_version "G++" g++ 12 g++ --version
    check_tool_version "Python" python3 3.10 python3 --version

    if require_cmd pkg-config; then
        local -a missing_pc=()
        local pc
        for pc in gl x11 xext xrandr xcursor xi xfixes xscrnsaver xtst xkbcommon \
                  alsa libpulse libudev dbus-1 wayland-client egl; do
            if ! pkg-config --exists "$pc" 2>/dev/null; then
                missing_pc+=("$pc")
            fi
        done
        if (( ${#missing_pc[@]} == 0 )); then
            ok "OpenGL/X11/audio dev: pkg-config модули найдены"
        elif (( SKIP_APT )); then
            skip "pkg-config модули не установлены (--skip-apt): ${missing_pc[*]}"
        else
            fail "нет pkg-config модулей: ${missing_pc[*]}"
        fi
    elif (( SKIP_APT )); then
        skip "pkg-config не установлен (--skip-apt)"
    else
        fail "pkg-config не найден"
    fi

    # Опциональные инструменты: не блокируют сборку, но нужны для QA/docs.
    local tool
    for tool in xvfb-run lcov doxygen dot clang-format; do
        if require_cmd "$tool"; then
            ok "$tool: $(command -v "$tool")"
        else
            warn "$tool не найден (опционально)"
        fi
    done

    if (( ! SKIP_SATURN )); then
        if require_cmd genisoimage || require_cmd mkisofs; then
            ok "ISO-инструмент: $(command -v genisoimage 2>/dev/null || command -v mkisofs)"
        elif (( SKIP_APT )); then
            skip "genisoimage/mkisofs не установлен (--skip-apt)"
        else
            fail "genisoimage/mkisofs не найден (нужен для Saturn ISO)"
        fi
    fi

    if (( SKIP_PS1 )); then
        skip "PS1: пропущено (--skip-ps1)"
    elif [[ -x "$PS1_DIR/bin/mipsel-none-elf-gcc" ]]; then
        local ps1_gcc
        ps1_gcc="$(first_version "$PS1_DIR/bin/mipsel-none-elf-gcc" --version)"
        if [[ "$ps1_gcc" == "$PS1_GCC_VERSION" ]]; then
            ok "PSn00bSDK: mipsel-none-elf-gcc $ps1_gcc"
        else
            fail "PSn00bSDK: GCC $ps1_gcc != $PS1_GCC_VERSION"
        fi
        if [[ -f "$PS1_DIR/lib/libpsn00b/cmake/sdk.cmake" ]]; then
            ok "PSn00bSDK: sdk.cmake найден"
        else
            fail "PSn00bSDK: нет lib/libpsn00b/cmake/sdk.cmake"
        fi
        if [[ -x "$PS1_DIR/bin/mkpsxiso" ]]; then
            ok "PSn00bSDK: mkpsxiso найден"
        else
            warn "PSn00bSDK: mkpsxiso не найден"
        fi
    else
        fail "PSn00bSDK не установлен: $PS1_DIR"
    fi

    if (( SKIP_SATURN )); then
        skip "Saturn: пропущено (--skip-saturn)"
    elif [[ -d "$JO_DIR/.git" ]]; then
        local jo_head
        jo_head="$(git -C "$JO_DIR" rev-parse HEAD 2>/dev/null || true)"
        if [[ "$jo_head" == "$JO_COMMIT" ]]; then
            ok "Jo Engine: ${jo_head:0:7}"
        else
            fail "Jo Engine: commit ${jo_head:0:7} != ${JO_COMMIT:0:7}"
        fi
        if [[ -x "$JO_DIR/Compiler/LINUX/bin/sh-none-elf-gcc-8.2.0" \
                && -f "$JO_DIR/Compiler/COMMON/jo.linker" \
                && -d "$JO_DIR/jo_engine" ]]; then
            ok "Jo Engine: компилятор и linker-скрипт найдены"
        else
            fail "Jo Engine: неполный checkout ($JO_DIR)"
        fi
    else
        fail "Jo Engine не установлен: $JO_DIR"
    fi

    if [[ -f "$REPO_ROOT/ci/verify_vendored.sh" ]]; then
        if (cd "$REPO_ROOT" && sh ci/verify_vendored.sh >/dev/null 2>&1); then
            ok "вендоренные зависимости: пины и SHA-256 совпадают"
        else
            fail "вендоренные зависимости: проверка не прошла (sh ci/verify_vendored.sh)"
        fi
    fi

    if [[ -f "$REPO_ROOT/src/third_party/benchmark/CMakeLists.txt" ]]; then
        ok "google/benchmark вендорен"
    else
        warn "google/benchmark не вендорен: pc-bench соберёт memory_benchmark без math_benchmark"
    fi
}

print_summary() {
    step "Итог"
    if (( FAILURES == 0 )); then
        info "${C_GREEN}Все обязательные компоненты готовы.${C_RESET}"
        if (( WARNINGS > 0 )); then
            info "Предупреждений: $WARNINGS (опциональные компоненты)."
        fi
        info ""
        if (( NO_SHELL_CONFIG )); then
            info "Env не подключён (--no-shell-config): экспортируйте"
            info "PSN00BSDK_ROOT/PSN00BSDK_LIBS/JO_ENGINE_ROOT вручную."
        else
            info "Дальше:"
            info "  source ~/.bashrc   # или новый терминал"
        fi
        info "  cmake --preset pc-debug"
        info "  cmake --build --preset pc-debug"
        info "  ctest --preset pc-debug --output-on-failure"
        return 0
    fi
    info "${C_RED}Неудачных проверок: $FAILURES${C_RESET}"
    info "Исправьте [FAIL] выше и запустите --check повторно."
    return 1
}

main() {
    parse_args "$@"

    if (( CHECK_ONLY )); then
        run_checks
        print_summary
        return
    fi

    if (( ! SKIP_APT )); then
        setup_sudo
    fi

    step "RetroPolyEngine: установка инструментария"
    info "Репозиторий: $REPO_ROOT"
    if (( SKIP_APT )); then
        info "Системные пакеты: пропуск (--skip-apt)"
    else
        info "Системные пакеты: ${#APT_PACKAGES[@]} шт. (недостающие)"
    fi
    if (( SKIP_PS1 )); then
        info "PSn00bSDK: пропуск"
    else
        info "PSn00bSDK ${PS1_VERSION}: $PS1_DIR"
    fi
    if (( SKIP_SATURN )); then
        info "Jo Engine: пропуск"
    else
        info "Jo Engine ${JO_TAG}: $JO_DIR"
    fi

    if (( ! DRY_RUN )) && ! confirm; then
        info "Отменено."
        return 0
    fi

    install_apt_packages
    ensure_submodules
    install_ps1
    install_saturn
    configure_shell

    if (( DRY_RUN )); then
        info ""
        info "DRY-RUN: изменения не вносились."
        return 0
    fi

    run_checks
    print_summary
}

main "$@"
