# Сборка RetroPolyEngine

> Нормативные источники:
> [`RetroEngine_Engineering_Documentation_v1.43.md`](RetroEngine_Engineering_Documentation_v1.43.md),
> [`DEVELOPMENT_PLAN.md`](DEVELOPMENT_PLAN.md) §6, §11 и
> [`plans/sprint_00.md`](plans/sprint_00.md). Единственная точка входа —
> [`CMakePresets.json`](../CMakePresets.json); ручные `cmake -B` с
> нестандартными путями не поддерживаются.

## Требования

### Все платформы

- Git 2.39+
- CMake 3.25+
- Ninja 1.11+
- Python 3.10+ (только для служебных скриптов)

### PC

- Компилятор C/C++: GCC 12+, Clang 16+, MSVC 2022 (CL 19.38+)
- OpenGL 3.3 Core драйвер / Mesa
- SDL3 собирается из vendored submodule (`src/third_party/sdl3`)

### Linux / Ubuntu

```sh
sudo apt-get update
sudo apt-get install -y git cmake ninja-build libgl1-mesa-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
  libxfixes-dev libxss-dev libxtst-dev libxkbcommon-dev
```

Автоматическая установка (Debian / Ubuntu / Linux Mint) — пакеты, PS1/Saturn
тулчейны и переменные окружения:

```sh
tools/linux/setup-linux.sh --check   # только диагностика, без изменений
tools/linux/setup-linux.sh --yes     # установить всё недостающее
tools/linux/setup-linux.sh --pc-only # только зависимости PC-сборки
```

Скрипт идемпотентен; внешние тулчейны ставятся по пинам из `VERSION.txt`
(PSn00bSDK v0.24 с проверкой SHA-256, Jo Engine v2026.2 с проверкой commit).

Если DNS не разрешает `archive.ubuntu.com` / `security.ubuntu.com`, укажите
доступное Ubuntu-зеркало — системные `sources.list` не изменяются:

```sh
tools/linux/setup-linux.sh --yes --apt-mirror http://mirror.yandex.ru/ubuntu
```

### Windows

Можно использовать автоматический скрипт:

```powershell
.\tools\windows\setup-windows.ps1
```

Или вручную установить:

- Git for Windows
- CMake + Ninja
- Visual Studio 2022 Build Tools (C++ workload)
- MSYS2 (для Unix-утилит, используемых в CI)
- DuckStation / PCSX-Redux (для ручной приёмки PS1)

### macOS

```sh
brew install git cmake ninja
```

### PS1

- PSn00bSDK v0.24 (pinned): скачать
  [`PSn00bSDK-0.24-Linux.zip`](https://github.com/Lameguy64/PSn00bSDK/releases/tag/v0.24)
  или Windows-эквивалент.
- Распаковать в `~/PSn00bSDK` (Linux/macOS) или `C:\PSn00bSDK` (Windows).
- Установить переменные среды:

```bash
# Linux/macOS
export PSN00BSDK_ROOT=$HOME/PSn00bSDK
export PSN00BSDK_LIBS=$PSN00BSDK_ROOT/lib/libpsn00b
export PATH=$PSN00BSDK_ROOT/bin:$PATH
```

```powershell
# Windows (cmd)
set PSN00BSDK_ROOT=C:\PSn00bSDK
set PSN00BSDK_LIBS=%PSN00BSDK_ROOT%\lib\libpsn00b
set PATH=%PSN00BSDK_ROOT%\bin;%PATH%
```

### Sega Saturn

- Jo Engine v2026.2 (pinned commit `5e99944`) — full checkout, не sparse
  без `Compiler/` и `jo_engine/`.
- Bundled `sh-none-elf-gcc` 8.2.0 идёт внутри Jo Engine
  (`Compiler/LINUX`); отдельный Saturn toolchain не нужен.
- `mkisofs`/`genisoimage` (пакет `genisoimage`) — для bootable ISO.
- Версии/SHA-256: [`../VERSION.txt`](../VERSION.txt) и
  [`../ci/joengine-v2026.2.sha256`](../ci/joengine-v2026.2.sha256).
- Подробности платформы/эмулятора: [`SATURN.md`](SATURN.md).

```bash
git clone --depth 1 --branch v2026.2 \
  https://github.com/johannes-fetz/joengine.git "$HOME/joengine"
export JO_ENGINE_ROOT="$HOME/joengine"
```

```powershell
# Windows (experimental; bundled sh-elf-gcc.exe)
git clone --depth 1 --branch v2026.2 https://github.com/johannes-fetz/joengine.git C:\joengine
set JO_ENGINE_ROOT=C:\joengine
```

### Nintendo 64

- libdragon-toolchain: GNU GCC 16.2.0 для `mips64-elf` (Linux x86_64 deb),
  SHA-256 — [`../ci/n64-toolchain.sha256`](../ci/n64-toolchain.sha256).
- libdragon (MIT), pinned commit `e356bf3` — env-install, не vendored
  ([`../ci/n64-libdragon.commit`](../ci/n64-libdragon.commit)).
- `N64_INST` — install-prefix (bin/, `mips64-elf/{include,lib}`);
  `LIBDRAGON_ROOT` — git-checkout для проверки commit.
- Эмулятор ручного gate — gopher64 1.2.1 (не vendored, BIOS не нужен).
- Версии/SHA-256: [`../VERSION.txt`](../VERSION.txt); платформа —
  [`N64.md`](N64.md); решение — ADR-014.

```bash
mkdir -p "$HOME/n64-sdk" && cd "$HOME/n64-sdk"
curl -L -o gcc-toolchain-mips64-x86_64.deb \
  https://github.com/DragonMinded/libdragon/releases/download/toolchain-continuous-prerelease/gcc-toolchain-mips64-x86_64.deb
sha256sum -c /path/to/retro-engine/ci/n64-toolchain.sha256
dpkg-deb -x gcc-toolchain-mips64-x86_64.deb toolchain
export N64_INST="$HOME/n64-sdk/toolchain/opt/libdragon"

git clone https://github.com/DragonMinded/libdragon.git
git -C libdragon checkout e356bf3f56f7afbf7e5246329562f145965cfdfc
make -C libdragon -j"$(nproc)"
make -C libdragon install
make -C libdragon tools-install
export LIBDRAGON_ROOT="$HOME/n64-sdk/libdragon"
```

## Клонирование

```sh
git clone --recursive https://github.com/RetroPolyEngine/retro-engine.git
cd retro-engine
```

Если репозиторий уже склонирован без submodules:

```sh
git submodule update --init --recursive
```

## Канонические пресеты

| Пресет | Бэкенд | Тесты | Примеры | Назначение |
|---|---|---|---|---|
| `pc-debug` | PC | ON | OFF | Разработка, unit-тесты |
| `pc-release` | PC | OFF | ON | Оптимизированные примеры |
| `pc-debug-render` | PC | ON | OFF | W0-golden + visual regression |
| `ui-debug` | PC | ON | OFF | UI client Debug + core-тесты (S35–36) |
| `ui-release` | PC | OFF | OFF | UI client Release + пакет |
| `pc-asan` | PC | ON | OFF | AddressSanitizer |
| `pc-bench` | PC | OFF | OFF | google/benchmark |
| `pc-coverage` | PC | ON | OFF | lcov покрытие |
| `ps1-release` | PS1 | OFF | OFF | Кросс-сборка PS-EXE + CD-образ |
| `saturn-release` | Saturn | OFF | OFF | SH-2 ELF + bootable ISO/CUE (Jo Engine), dual-CPU |
| `saturn-single` | Saturn | OFF | OFF | S25–26 single-CPU fallback (`RPE_SATURN_DUAL_CPU=OFF`) |
| `saturn-hang` | Saturn | OFF | OFF | S25–26 fault injection: Slave зависает |
| `saturn-flood` | Saturn | OFF | OFF | S25–26 fault injection: переполнение очереди |
| `n64-release` | N64 | OFF | OFF | `.z64` + DFS (`mkdfs`) + map + sha256 (libdragon) |

## SDK: install/export и пакет

SDK собирается из любого платформенного пресета и упаковывается в
детерминированный архив:

```sh
# PC
cmake --preset pc-release
cmake --build --preset pc-release
cmake --build build/pc-release --target sdk_package

# PS1 / Saturn — аналогично своим пресетам
cmake --build build/ps1-release --target sdk_package
cmake --build build/saturn-release --target sdk_package
```

Результат: `build/<preset>/sdk-stage-<platform>-<arch>/` и
`dist/retro-engine-sdk-<version>-<platform>-<arch>.{tar.gz,zip}`.
Проверка consumer-проекта:

```sh
python3 scripts/sdk_consumer_smoke.py --sdk-dir build/pc-release/sdk-stage-pc-x86_64
```

Установка в произвольный prefix без архива:

```sh
cmake --install build/pc-release --prefix /opt/retro-engine-sdk
```

`find_package(RetroPolyEngine CONFIG REQUIRED)` и цели
`RetroPolyEngine::{retro_engine,hal,log}` — см. [`SDK.md`](SDK.md).

## UI client (S35–36)

UI client — отдельный host-таргет `retro_engine_ui` (SDL3 + Dear ImGui),
не линкует примеры/движок и работает с установленным SDK:

```sh
# Debug + unit-тесты ядра
cmake --preset ui-debug
cmake --build --preset ui-debug --target retro_engine_ui_tests
./build/ui-debug/tools/ui_client/retro_engine_ui_tests

# Release + запуск
cmake --preset ui-release
cmake --build --preset ui-release --target retro_engine_ui
./build/ui-release/ui/retro_engine_ui

# Пакет ui/ + checksums
python3 scripts/package_ui.py --build-dir build/ui-release \
  --version 0.2.1 --output-dir dist
```

Требования: Dear ImGui submodule (`git submodule update --init --recursive`),
OpenGL 3.3; headless smoke — `xvfb-run`. Руководство —
[`UI_CLIENT.md`](UI_CLIENT.md).

## Сборка PC

```sh
# Debug + тесты
cmake --preset pc-debug
cmake --build --preset pc-debug
ctest --preset pc-debug --output-on-failure

# Release + примеры
cmake --preset pc-release
cmake --build --preset pc-release

# Запуск примера
./build/pc-release/game/hello_triangle
./build/pc-release/game/rotating_cube
```

## Сборка PS1

```sh
# Linux / macOS
cmake --preset ps1-release
cmake --build --preset ps1-release

# Windows (cmd)
cmake --preset ps1-release
cmake --build --preset ps1-release
```

Артефакты:

- `build/ps1-release/src/hal/ps1/retro_ps1.elf`
- `build/ps1-release/src/hal/ps1/retro_ps1.exe`
- `build/ps1-release/src/hal/ps1/RETRO.bin`
- `build/ps1-release/src/hal/ps1/RETRO.cue`

### Запуск PS1-образа в DuckStation

```sh
# Linux
scripts/run_ps1.bat
```

Или открыть `build/ps1-release/src/hal/ps1/RETRO.cue` в DuckStation
(NTSC-U, fast boot). PS1-запуск — ручная приёмка; в CI проверяется только
кросс-сборка и наличие артефактов.

### Проверка размера PS-EXE

```sh
sh scripts/check_binary_size.sh
```

Лимит: 512 000 байт.

> Публичным релизом считается только SDK + UI client по
> [`RELEASING.md`](RELEASING.md); demo-образы `hello_triangle.*` — примеры,
> не релиз. Любая публикация релизов отложена до v0.9.x (release freeze).

## Сборка Saturn

```sh
export JO_ENGINE_ROOT="$HOME/joengine"   # Jo Engine v2026.2
cmake --preset saturn-release
cmake --build --preset saturn-release
```

Артефакты `hello_triangle` (S31–32; байт-идентичны legacy `retro_saturn.*`):

- `build/saturn-release/src/hal/saturn/hello_triangle.elf` (SH-2 ELF)
- `build/saturn-release/src/hal/saturn/hello_triangle.bin` (raw)
- `build/saturn-release/src/hal/saturn/hello_triangle.iso` (ISO9660)
- `build/saturn-release/src/hal/saturn/hello_triangle.cue` (`MODE1/2048`)
- `build/saturn-release/src/hal/saturn/hello_triangle.map`
- `build/saturn-release/src/hal/saturn/IP.BIN`

В `disc/` автоматически копируются CD-ассеты под bare 8.3-именами:
`CUBE.MSH`, `TRIANGLE.MSH`, `TEXTURE.TEX`, `TEX0.TEX`…`TEX3.TEX`. HAL
принимает только такие имена без `;1` (`src/hal/saturn/saturn_fs_path.h`).

CI: `.github/workflows/build-saturn.yml` — pinned commit, sha256
компилятора, build, структурная валидация ISO/ELF + наличие и байтовое
совпадение `.mesh`/`.tex`, artifact `retro-saturn-disc`. Эмулятор в CI не
запускается (BIOS не вендорится).

Saturn HAL host-тесты (без BIOS/эмулятора):

```sh
cmake --preset pc-debug
cmake --build build/pc-debug --target saturn_hal_tests
ctest --test-dir build/pc-debug -R 'Saturn|AssetHeap' --output-on-failure
```

Цель компилирует `src/hal/saturn/*` host-компилятором с `PLATFORM_SATURN=1`
и fake C-мостом (`tests/saturn/fake_saturn_bridge.cpp`); SH-2/Jo Engine не
нужны.

### Запуск Saturn-образа в Mednafen 1.32.1

```sh
mednafen -video.driver softfb -ss.bios_jp /path/to/sega_101.bin \
  -ss.region_autodetect 0 -ss.region_default jp \
  build/saturn-release/src/hal/saturn/hello_triangle.cue
```

BIOS (`mpr-17933.bin` NA/EU или `sega_101.bin` JP) пользователь
предоставляет локально; в репозиторий/CI он не попадает. Smoke-тест:
тёмно-синий clear 320×224 (`0x9000`), строка 27 с логом
`[INFO][asset] CD mesh=1 tex=1 heap=65560`, 5 минут S23–24 и 10 минут
S25–26 без краша. 6-button pad (D-pad, A/B/C, X/Y/Z, L/R, Start) мапится
в `INPUT_PAD_*`; SMPC-порт настраивается `ss.input.port1=gamepad`,
mapping/latch/disconnect покрыты host-тестами. Управление
`hello_triangle`: D-pad — поворот объекта, A/B — дистанция камеры,
C/X — высота, Y/Z — орбита, Start — сброс. Полный чек-лист и
настройки — [`SATURN.md`](SATURN.md) §9, §11, §13.

### Варианты Saturn-сборки (S25–26)

Канонический `saturn-release` — dual-CPU. Опции:

| Опция | Значения | Назначение |
|---|---|---|
| `RPE_SATURN_DUAL_CPU` | `ON`/`OFF` | `OFF` — single-CPU fallback (нет `slave.c`/`JO_COMPILE_WITH_DUAL_CPU_SUPPORT`) |
| `RPE_SATURN_FAULT_MODE` | `none`/`hang`/`flood` | fault injection для ручной приёмки |

```sh
# single-CPU fallback
cmake --preset saturn-single
cmake --build --preset saturn-single

# Slave зависает на следующем job
cmake --preset saturn-hang
cmake --build --preset saturn-hang

# переполнение очереди (boot-проба)
cmake --preset saturn-flood
cmake --build --preset saturn-flood
```

Метрики S25–26, порог и ручной чек-лист — [`SATURN.md`](SATURN.md) §11,
нормативное решение — [`adr/ADR-009-master-slave.md`](adr/ADR-009-master-slave.md).

## Сборка Nintendo 64

```sh
export N64_INST="$HOME/n64-sdk/toolchain/opt/libdragon"
export LIBDRAGON_ROOT="$HOME/n64-sdk/libdragon"
cmake --preset n64-release
cmake --build --preset n64-release
```

Артефакты `build/n64-release/src/hal/n64/`:

- `hello_n64.z64` — bootable `.z64` (canonical demo);
- `hello_n64.map` — linker map;
- `hello_n64.z64.sha256` — SHA-256 (`sha256sum -c`);
- `retro_n64.z64` — те же байты под engine-именем;
- `retro_n64.elf` + `retro_n64.sym` — ELF и символы;
- `retro_n64.dfs` + `retro_n64.dfs.txt` — read-only DFS (`CUBE.MSH`,
  `TEX0.TEX`, `TRIANGLE.MSH`) и listing.

Проверка ROM без эмулятора:

```sh
python3 scripts/verify_n64_rom.py \
  build/n64-release/src/hal/n64/hello_n64.z64
python3 scripts/verify_n64_no_heap.py
"$N64_INST/bin/dumpdfs" -l build/n64-release/src/hal/n64/hello_n64.z64
```

Ожидается `RESULT: PASS`, `entry 0x80000400`,
`crc_policy=libdragon_ipl3_header_crc_unused` (libdragon IPL3 не читает
header CRC — policy ADR-014).

Ручной запуск в pinned gopher64 1.2.1:

```sh
curl -L -o gopher64 \
  https://github.com/gopher64/gopher64/releases/download/v1.2.1/gopher64-linux-x86_64
chmod +x gopher64
GOPHER64_BIN=$PWD/gopher64 scripts/run_n64.sh

# headless
xvfb-run -a -s "-screen 0 800x600x24" \
  env GOPHER64_BIN=$PWD/gopher64 scripts/run_n64.sh
```

Gate: 60 с, тёмно-синий экран `(0,0,32)`, строки `BOOT OK / S41-42`,
`DFS OK MESH 600 TEX 66068`, memory overlay, heartbeat
`frame=… time=…` каждые ~5 с, input-лог `[n64_input] pad0 press …`,
без RSP-crash; 10-минутный stress — ручной. mupen64plus не использовать:
libdragon IPL3 r5+ не загружается (mupen64plus-core#1185). CI
(`.github/workflows/build-n64.yml`) собирает ROM, проверяет
header/ELF/sha256/DFS и запрет heap, но эмулятор не запускает. Полный
чек-лист — [`N64.md`](N64.md) §9.

N64 HAL host-тесты (без libdragon):

```sh
cmake --preset pc-debug
cmake --build build/pc-debug --target n64_hal_tests
ctest --test-dir build/pc-debug -R N64 --output-on-failure
```

Цель компилирует `src/hal/n64/*` host-компилятором с `PLATFORM_N64=1`
и fake C-мостом (`tests/n64/fake_n64_bridge.cpp`). ASan-прогон:

```sh
cmake --preset pc-asan
cmake --build build/pc-asan --target n64_hal_tests
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 ./build/pc-asan/tests/n64_hal_tests
```

## Частые проблемы

### `SDL3 not found` / submodule пуст

Сообщение:

```
src/third_party/sdl3 пуст (submodule не инициализирован)
```

Решение:

```sh
git submodule update --init --recursive
```

### `PS1 build → PATH/env`

Сообщение:

```
CMake Error: Could not find toolchain file: .../sdk.cmake
```

Решение: проверить `PSN00BSDK_LIBS` и `PATH`; см. раздел PS1 выше.

### `Saturn build → JO_ENGINE_ROOT`

Сообщение:

```
JO_ENGINE_ROOT не задан ...
```

Решение: скачать Jo Engine v2026.2 (полный checkout) и экспортировать
`JO_ENGINE_ROOT`; см. раздел Saturn выше. `JO_ENGINE_ROOT` должен
содержать `Compiler/LINUX`, `Compiler/COMMON`, `jo_engine`.

### `saturn-release → mkisofs not found`

Предпочтительно: установить `genisoimage` (Linux) или `mkisofs`
(macOS/Windows). Без него bootable ISO — обязательный артефакт gate не
закрывается.

Fallback (изолированные окружения, например Flatpak): сборка может
использовать `tools/saturn_mkiso.py`, если в PATH доступен Python 3 с
пакетом `pycdlib`:

```sh
python3 -m venv /tmp/saturn-iso-tools
source /tmp/saturn-iso-tools/bin/activate
python3 -m pip install pycdlib
cmake --preset saturn-release
cmake --build --preset saturn-release
```

### `n64-release → N64_INST не задан`

Сообщение:

```
N64_INST не задан. Установите pinned mips64-elf toolchain + libdragon
```

Решение: выполнить установку из раздела Nintendo 64 выше; проверить
`test -x "$N64_INST/bin/n64tool"` и
`test -f "$N64_INST/mips64-elf/lib/libdragon.a"`.

### `n64-release → LIBDRAGON_ROOT HEAD=…`

Toolchain file требует pinned commit `e356bf3` при наличии `.git`.
Решение: `git -C "$LIBDRAGON_ROOT" checkout
e356bf3f56f7afbf7e5246329562f145965cfdfc`; обновление libdragon —
отдельная задача с ADR-014.

### `n64-release → mips64-elf-gcc … не N64 cross-toolchain`

`N64_INST` указывает не на libdragon-toolchain (в `bin/` лежит host gcc
или другой target). Переустановить pinned deb из `VERSION.txt`.

### N64 ROM чёрный в mupen64plus

Не использовать mupen64plus: libdragon IPL3 r5+ не загружается
(mupen64plus-core#1185). Pinned baseline — gopher64 1.2.1
(`scripts/run_n64.sh`); ares также загружает ROM, но Linux-бинаря
upstream нет.

### Visual Studio: ошибки линковки SDL3-static

Убедитесь, что используете `cmake --preset pc-release` (или `pc-debug`), а не
генерируете solution вручную. `CMakePresets.json` фиксирует Ninja и
необходимые переменные.

### `pc-debug-render` падает локально без дисплея

Используйте `xvfb-run` (Linux):

```sh
xvfb-run -a ctest --preset pc-debug-render --output-on-failure -R integration
```

## Проверка вендоринга

```sh
sh ci/verify_vendored.sh
```

Проверяет пины submodules и SHA-256 одиночных файлов против
[`VERSION.txt`](../VERSION.txt).
