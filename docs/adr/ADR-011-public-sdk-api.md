# ADR-011: границы публичного SDK API и install/export

- **Статус:** Accepted
- **Дата:** 2026-11-13
- **Спринт:** 33–34 (недели 65–68), веха v0.2.1
- **Связанные документы:** [`../plans/sprint_33_34.md`](../plans/sprint_33_34.md),
  [`../SDK.md`](../SDK.md), [`../RELEASING.md`](../RELEASING.md),
  [`../../include/retro_engine/retro_engine.h`](../../include/retro_engine/retro_engine.h)

## Контекст

До S33–34 движок собирался только как in-tree CMake-проект: заголовки лежали
в `src/`, internal- и public-контракты не различались, а `find_package` не
существовал. Релизная политика v0.2.1 требует готовый SDK: публичные
заголовки, библиотеки, CMake package, примеры и consumer smoke.

Нужно зафиксировать границу API так, чтобы:

1. публичные заголовки не тянули SDL3/OpenGL/PSn00bSDK/Jo Engine;
2. internal-заголовки не устанавливались;
3. in-tree сборка, тесты и три бэкенда не ломались;
4. SDK собирался как из build-tree, так и из install-tree;
5. PC/PS1/Saturn пакеты имели один и тот же публичный API.

## Решения

### 1. Канонический public-корень — `include/retro_engine/`

Публичные заголовки перенесены в:

```
include/retro_engine/retro_engine.h      # umbrella (единственная точка входа)
include/retro_engine/version.h           # generated: версия/платформа
include/retro_engine/core/**             # types, math, memory, ecs, log
include/retro_engine/hal/platform.h      # Platform/Render/Input/Audio/FS
include/retro_engine/assets/assets.h     # .mesh/.tex v1 + loaders
include/retro_engine/game/**             # components + systems
```

Потребитель включает `<retro_engine/retro_engine.h>` или конкретный
`<retro_engine/...>`. Source-style `"core/math/fixed.h"` остаётся доступен
как build-interface совместимость для in-tree кода и тестов, но не является
документированным способом подключения SDK.

### 2. Internal-заголовки не устанавливаются

Не входят в SDK и не включаются в umbrella:

- `src/render/*` (`renderer.h`, `command_buffer.h`, `texture.h`, `sort_key.h`,
  `overlay_text.h`);
- `src/hal/render_backend.h` (контракт бэкенда);
- `src/hal/pc|ps1|saturn/*` (включая `hal_*_internal.h`, `jo_saturn_bridge.h`,
  `saturn_*`).

Исключение для Saturn: заголовки pinned Jo Engine устанавливаются как
зависимость `RetroPolyEngine::jo_engine` (в `include/retro_engine/saturn/`),
но не входят в публичный API движка. Аналогично PC SDK содержит SDL3/glad
как link-зависимости, а не как часть public headers.

Правило проверяется автоматически:
`verify_release_contract.py::sdk_headers` + CI-тест «public headers compile
standalone».

### 3. CMake targets и package

Библиотеки экспортируются с namespaced-именами:

| Target | Назначение |
|---|---|
| `RetroPolyEngine::retro_engine` | ядро (math/ECS/render/assets API) |
| `RetroPolyEngine::hal` | платформенный HAL (ровно один бэкенд) |
| `RetroPolyEngine::log` | диагностика |

`RetroPolyEngineConfig.cmake` + `RetroPolyEngineConfigVersion.cmake`
поддерживают:

- install-tree: `find_package(RetroPolyEngine CONFIG REQUIRED)`;
- build-tree: `-DRetroPolyEngine_DIR=<build-dir>`.

PC-пакет несёт SDL3 (static) и glad; PS1/Saturn — кросс-библиотеки. Версия
пакета — `0.2.1`, `SameMajorVersion`.

### 4. Layout SDK

```
sdk/
  include/retro_engine/...
  lib/<platform>/<arch>/...
  lib/cmake/RetroPolyEngine/...
  lib/pkgconfig/retro_engine.pc
  cmake/                     # ps1/saturn toolchain wrappers
  tools/asset_converter      # PC
  examples/                  # standalone CMake, find_package
  templates/project/
  VERSION.txt
  LEGAL.md / LICENSE
  sdk_manifest.json
  checksums.sha256
```

Архив: `retro-engine-sdk-<version>-<platform>-<arch>.{tar.gz,zip}` с
детерминированными mtime/порядком файлов (`scripts/package_sdk.py`).

### 5. Consumer smoke — часть release gate

`tests/sdk/consumer/` — отдельный CMake-проект, который не видит дерево
исходников: только `find_package`, установленные заголовки/библиотеки и
`examples/hello_triangle`. Для PS1/Saturn тот же smoke собирает
`examples/hello_triangle_ps1|saturn` из пакета.

`verify_release_contract.py` падает, если SDK-архив, manifest, checksums,
public headers или consumer smoke отсутствуют/повреждены.

## Последствия

- Внутренние заголовки можно менять без обещаний ABI/API.
- Публичные заголовки обязаны компилироваться без третьих сторон; нарушение
  ловится release gate.
- Для новых public-заголовков нужно добавить их в `include/retro_engine/`,
  umbrella и `ci/normative_contracts.yaml` (при необходимости).
- Полноценный package manager (зависимости, версии, lock-файлы) в v0.2.1 не
  входит — только headers/libs/config/examples/checksums.
