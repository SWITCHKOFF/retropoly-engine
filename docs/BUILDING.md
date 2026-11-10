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
| `pc-asan` | PC | ON | OFF | AddressSanitizer |
| `pc-bench` | PC | OFF | OFF | google/benchmark |
| `pc-coverage` | PC | ON | OFF | lcov покрытие |
| `ps1-release` | PS1 | OFF | OFF | Кросс-сборка PS-EXE + CD-образ |
| `saturn-release` | Saturn | OFF | OFF | SH-2 ELF + bootable ISO/CUE (Jo Engine) |

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

## Сборка Saturn

```sh
export JO_ENGINE_ROOT="$HOME/joengine"   # Jo Engine v2026.2
cmake --preset saturn-release
cmake --build --preset saturn-release
```

Артефакты:

- `build/saturn-release/src/hal/saturn/retro_saturn.elf` (SH-2 ELF)
- `build/saturn-release/src/hal/saturn/retro_saturn.bin` (raw)
- `build/saturn-release/src/hal/saturn/retro_saturn.iso` (ISO9660)
- `build/saturn-release/src/hal/saturn/retro_saturn.cue` (`MODE1/2048`)
- `build/saturn-release/src/hal/saturn/IP.BIN`
- `build/saturn-release/src/hal/saturn/retro_saturn.map`

CI: `.github/workflows/build-saturn.yml` — pinned commit, sha256
компилятора, build, структурная валидация ISO/ELF, artifact
`retro-saturn-disc`. Эмулятор в CI не запускается (BIOS не вендорится).

### Запуск Saturn-образа в Mednafen 1.32.1

```sh
mednafen -ss.bios_na_eu /path/to/mpr-17933.bin \
  build/saturn-release/src/hal/saturn/retro_saturn.cue
```

BIOS (`mpr-17933.bin` NA/EU или `sega_101.bin` JP) пользователь
предоставляет локально; в репозиторий/CI он не попадает. Smoke-тест:
backdrop 320×224, 60 секунд без краша. Полный чек-лист и настройки —
[`SATURN.md`](SATURN.md) §9.

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
