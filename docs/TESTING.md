# Тестирование RetroPolyEngine

> Нормативные источники:
> [`DEVELOPMENT_PLAN.md`](DEVELOPMENT_PLAN.md) §10,
> [`plans/sprint_19_20.md`](plans/sprint_19_20.md) (неделя 39) и
> [`RetroEngine_Engineering_Documentation_v1.43.md`](RetroEngine_Engineering_Documentation_v1.43.md).

## Пирамида тестов

### Unit-тесты (70%)

Запуск:

```sh
cmake --preset pc-debug && cmake --build --preset pc-debug
ctest --preset pc-debug --output-on-failure
```

Основные модули:

- `Fixed`, `Vec2/3/4`, `Trig`, `Mat4`, `Mat3` — математика;
- `LinearAllocator`, `PoolAllocator`, `StackAllocator`,
  `AllocatorsIntegration` — память;
- `Entity`, `RegistryTest` — ECS;
- `TextureRgb555`, `TexturePoolTest` — текстуры;
- `CommandBufferLayout`, `CommandBufferApi`, `CommandBufferOverflow` —
  командный буфер;
- `DepthKeyNormative`, `SortKeyContract` — рендер-контракты;
- `Hal`, `HalMouse`, `HalFileSystem` — HAL PC;
- `MeshLoader`, `TexContract`, `TextureLoader` — ассеты.

### Integration / visual тесты (25%)

```sh
cmake --preset pc-debug-render && cmake --build --preset pc-debug-render
xvfb-run -a ctest --preset pc-debug-render --output-on-failure
```

- `W0Golden` — framebuffer-конформанс;
- `RenderLifecycle` — контракты Init/BeginFrame/EndFrame;
- `HelloTriangleGolden`, `RotatingCubeGolden` — эталонные кадры примеров.

### E2E / smoke (5%)

```sh
cmake --preset pc-release && cmake --build --preset pc-release
./build/pc-release/game/hello_triangle 1
./build/pc-release/game/rotating_cube 1
```

PS1:

```sh
cmake --preset ps1-release && cmake --build --preset ps1-release
# Ручной запуск RETRO.cue в DuckStation
```

## Release gate

Перед тегом `v0.1.0` должен быть зелёным:

```sh
python3 scripts/verify_release_contract.py
```

Скрипт проверяет:

- канонические пресеты;
- размер PS-EXE ≤ 512 000 байт;
- RAM/VRAM бюджеты PS1;
- W0 framebuffer hash;
- W1 FPS PC/PS1;
- валидацию ассетов;
- отсутствие silent renderer overflow.

## CI

- `tests.yml` — pc-debug + pc-debug-render (xvfb);
- `build-pc.yml` — pc-release на ubuntu/windows/macos;
- `build-ps1.yml` — ps1-release на ubuntu;
- `release.yml` — по тегу `v*` (см. [`plans/sprint_19_20.md`](plans/sprint_19_20.md)).

## Ручные QA-чек-листы

### PC

- [ ] `ctest` pc-debug/pc-release зелёный;
- [ ] `hello_triangle` и `rotating_cube` запускаются без краша;
- [ ] overlay (TAB/Select) работает;
- [ ] выход по ESC/START не падает.

### PS1 (DuckStation)

- [ ] `RETRO.cue` загружается;
- [ ] куб/треугольник виден и вращается;
- [ ] FPS/VPS ≥ 30 в течение 60 с;
- [ ] RAM ≈ 815 KiB / 1.5 MiB, VRAM ≤ 800 KiB;
- [ ] PS-EXE ≤ 512 000 байт.
