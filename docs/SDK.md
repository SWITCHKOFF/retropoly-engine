# SDK RetroPolyEngine

> Спринт 33–34 (недели 65–68), веха v0.2.1. Границы API —
> [`adr/ADR-011-public-sdk-api.md`](adr/ADR-011-public-sdk-api.md).
> Релизная политика — [`RELEASING.md`](RELEASING.md).

SDK — это устанавливаемое дерево движка: публичные заголовки, библиотеки,
CMake package, host-инструмент `asset_converter`, примеры, шаблон проекта,
toolchain-обёртки и контролируемые checksums. Публичный API не тянет SDL3,
OpenGL, PSn00bSDK или Jo Engine в заголовки потребителя.

## 1. Состав пакета

```
sdk/
  include/retro_engine/...
  lib/<platform>/<arch>/...
  lib/cmake/RetroPolyEngine/...
  lib/pkgconfig/retro_engine.pc
  cmake/                       # ps1-toolchain.cmake, saturn-sdk-toolchain.cmake
  tools/asset_converter[.exe]  # PC
  examples/hello_triangle/
  examples/rotating_cube/      # PC
  examples/hello_triangle_ps1/
  examples/hello_triangle_saturn/
  templates/project/
  VERSION.txt
  LEGAL.md
  LICENSE
  README.md
  sdk_manifest.json
  checksums.sha256
```

Архивы: `retro-engine-sdk-<version>-<platform>[-<os>]-<arch>.tar.gz` и
`.zip` (PC: `-pc-linux-x86_64`, PS1: `-ps1-mips`, Saturn: `-saturn-sh2`),
рядом — `.sha256`. Layout детерминирован: фиксированный порядок файлов,
mtime из `SOURCE_DATE_EPOCH`/даты git-коммита.

## 1.1. Пакет SDK v0.2.1 (release candidate, не опубликован)

**Release freeze:** любая публикация релизов отложена до v0.9.x;
GitHub Release v0.2.1 удалён/отозван. Артефакты ниже собираются локально
или в CI:

`retro-engine-sdk-0.2.1-*` (отдельно) или `retro-engine-v0.2.1.tar.gz`
(`sdk/` + `ui/` + optional `demos/`).

| Host | Архив | Target |
|---|---|---|
| Linux x86_64 | `retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz` | PC |
| Windows x86_64 | `retro-engine-sdk-0.2.1-pc-windows-x86_64.zip` | PC |
| macOS arm64 | `retro-engine-sdk-0.2.1-pc-macos-arm64.tar.gz` | PC |
| Linux (cross) | `retro-engine-sdk-0.2.1-ps1-mips.tar.gz` | PS1 (PSn00bSDK v0.24) |
| Linux (cross) | `retro-engine-sdk-0.2.1-saturn-sh2.tar.gz` | Saturn (Jo Engine v2026.2) |

```sh
tar -xzf retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz
cd sdk
sha256sum -c checksums.sha256
cat VERSION.txt sdk_manifest.json

# примеры против установленного SDK
cmake --preset sdk-release
cmake --build --preset sdk-release
```

PS1/Saturn toolchains в архив не входят: установите pinned
PSn00bSDK v0.24 / Jo Engine v2026.2 (см. `docs/BUILDING.md`).

## 2. Сборка SDK из репозитория

```sh
# PC
cmake --preset pc-release
cmake --build --preset pc-release
cmake --build build/pc-release --target sdk_package

# PS1 (PSn00bSDK v0.24)
cmake --preset ps1-release
cmake --build --preset ps1-release
cmake --build build/ps1-release --target sdk_package

# Saturn (Jo Engine v2026.2)
export JO_ENGINE_ROOT="$HOME/joengine"
cmake --preset saturn-release
cmake --build --preset saturn-release
cmake --build build/saturn-release --target sdk_package
```

`scripts/package_sdk.py` также можно запустить вручную:

```sh
python3 scripts/package_sdk.py --build-dir build/pc-release \
  --platform pc --arch x86_64 --version 0.2.1 --output-dir dist
```

Выход: `build/<preset>/sdk-stage-<platform>-<arch>/` (install-tree) и
`dist/retro-engine-sdk-<version>-<platform>-<arch>.{tar.gz,zip}`.
IP-адреса/BIOS/ROM/build-каталоги/секреты в пакет не попадают — проверка
встроена в packager и release gate.

## 3. Потребитель: CMake

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_game LANGUAGES CXX)

find_package(RetroPolyEngine CONFIG REQUIRED)

add_executable(my_game src/main.cpp)
target_compile_features(my_game PRIVATE cxx_std_14)
target_link_libraries(my_game PRIVATE RetroPolyEngine::retro_engine)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build
```

Доступные цели:

| Target | Содержимое |
|---|---|
| `RetroPolyEngine::retro_engine` | fixed/ECS/alloc/render/assets/игровые системы |
| `RetroPolyEngine::hal` | выбранный бэкенд (PC/PS1/Saturn) |
| `RetroPolyEngine::log` | диагностика |

`find_package` работает и из build-tree:

```sh
cmake -S . -B build -DRetroPolyEngine_DIR=/path/to/engine/build/pc-release
```

pkg-config (PC):

```sh
export PKG_CONFIG_PATH=/path/to/sdk/lib/pkgconfig
pkg-config --cflags --libs retro_engine
```

## 4. Публичный API

```cpp
#include <retro_engine/retro_engine.h>

int main()
{
    if (!Platform_Init()) return 1;
    if (!Render_Init(RENDER_FB_WIDTH, RENDER_FB_HEIGHT)) return 1;

    while (Platform_PumpEvents()) {
        Render_BeginFrame();
        Render_Clear(8, 16, 32);
        Render_EndFrame();
        Platform_SwapBuffers();
    }

    Render_Shutdown();
    Platform_Shutdown();
    return 0;
}
```

Публичные группы:

- `core/types.h`, `core/math/*` — Q16.16 fixed-point, векторы/матрицы;
- `core/memory/*` — linear/pool/stack allocators + profiler;
- `core/ecs/*` — Registry, Entity, компоненты;
- `core/log/log.h` — логирование;
- `hal/platform.h` — Platform/Render/Input/Audio/FileSystem + диагностика;
- `assets/assets.h` — `.mesh`/`.tex` v1 и безопасные загрузчики;
- `game/**` — компоненты и системы слоя игры.

Версия доступна без запуска платформы:

```cpp
#include <retro_engine/version.h>
printf("%s (%s)\n", RETRO_ENGINE_VERSION_STRING, RETRO_ENGINE_PLATFORM_NAME);
```

Internal-заголовки (`render/*`, `hal/*/...`) в SDK не устанавливаются.

## 5. Примеры из SDK

```sh
cd /path/to/sdk
cmake --preset sdk-release
cmake --build --preset sdk-release
./build/examples/hello_triangle/hello_triangle --version
./build/examples/rotating_cube/rotating_cube --version
```

Или standalone:

```sh
cmake -S examples/hello_triangle -B build-ht \
  -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build-ht
./build-ht/hello_triangle --version
```

## 6. PS1 SDK

```sh
export PSN00BSDK_LIBS="$HOME/PSn00bSDK/lib/libpsn00b"
cmake -S examples/hello_triangle_ps1 -B build-ps1 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/sdk/cmake/ps1-toolchain.cmake \
  -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build-ps1
```

Результат — `hello_triangle_ps1.elf/.exe`. Для CD-образа используйте
`mkpsxiso` из PSn00bSDK (см. `docs/BUILDING.md` §PS1).

## 7. Saturn SDK

```sh
export JO_ENGINE_ROOT="$HOME/joengine"
cmake -S examples/hello_triangle_saturn -B build-sat -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/sdk/cmake/saturn-sdk-toolchain.cmake \
  -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build-sat
```

Пакет содержит `lib/saturn/sh2/libjo_engine.a`, SH-2 библиотеки движка и
публичные заголовки Jo Engine. Запуск — в Mednafen (BIOS пользователя, не
вендорится).

## 8. Манифест и checksums

`sdk_manifest.json` содержит:

- `version`, `git_commit`, `git_dirty`, `commit_date`;
- `platform`, `arch`, `build_type`, `compiler`, `abi`;
- `targets`, `components`, `examples`;
- `artifacts[]` — путь, размер, SHA-256 каждого файла.

`checksums.sha256` покрывает все файлы SDK (кроме самого файла):

```sh
cd sdk
sha256sum -c checksums.sha256
```

Release gate:

```sh
python3 scripts/verify_release_contract.py \
  --sdk-archive dist/retro-engine-sdk-0.2.1-pc-x86_64.tar.gz
```

## 9. Clean consumer smoke

```sh
python3 scripts/sdk_consumer_smoke.py \
  --sdk-dir build/pc-release/sdk-stage-pc-x86_64
```

Проверяется: `find_package`, сборка `sdk_probe` + установленных примеров,
запуск `--version` без дисплея. Для PS1/Saturn:

```sh
python3 scripts/sdk_consumer_smoke.py --sdk-dir /path/to/sdk --platform ps1
python3 scripts/sdk_consumer_smoke.py --sdk-dir /path/to/sdk --platform saturn
```

## 10. Ограничения v0.2.1

- Нет dependency manager/version solver: SDK поставляет ровно один набор
  headers/libs/config/checksums.
- PC SDK содержит static SDL3/glad; системным остаётся только OpenGL.
- PS1/Saturn требуют внешний pinned toolchain (PSn00bSDK/Jo Engine).
- ABI-совместимость гарантируется в пределах major-версии пакета
  (`SameMajorVersion`); C++ API без бинарного обещания между компиляторами.
