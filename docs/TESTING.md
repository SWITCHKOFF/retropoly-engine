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

- `tests.yml` — pc-debug + pc-debug-render (xvfb) + `saturn_hal_tests`
  (host, `PLATFORM_SATURN=1`, fake C-мост, без BIOS/SH-2);
- `build-pc.yml` — pc-release на ubuntu/windows/macos;
- `build-ps1.yml` — ps1-release на ubuntu;
- `build-saturn.yml` — saturn-release + structural checks ISO/ELF + наличие
  и байтовое совпадение `.mesh`/`.tex` в образе;
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

### Saturn (Mednafen 1.32.1, S23–26)

- [x] `retro_saturn.cue` стартует с легальным Japanese BIOS 1.01;
- [x] тёмно-синий clear 320×224 (`0x9000`), строка 27:
      `[INFO][asset] CD mesh=1 tex=1 heap=65560` (932 KiB heap; CUBE.MSH
      остаётся в persistent-арене для workload);
- [x] 6-button pad: D-pad, A/B/C, X/Y/Z, L/R, Start мапятся в `INPUT_PAD_*`;
- [x] 5 минут без краша/зависания/сброса (`timeout 300` → exit 124);
- [x] скриншот [`../screenshots/saturn_clear_s23_24.png`](../screenshots/saturn_clear_s23_24.png);
- [x] `retro_saturn.map`: `__jo_work_end < 0x060FBC00`;
- [x] S25–26 dual: benchmark `1=23838 2=16113 g=32 h=1`, кадры
      `f=1500c=1503w=4007h=af890f`
      ([benchmark](../screenshots/saturn_dual_cpu_s25_26.png),
      [metrics](../screenshots/saturn_dual_metrics_s25_26.png));
- [x] S25–26 single (`-DRPE_SATURN_DUAL_CPU=OFF`): `1=... 2=n/a h=1`,
      Master продолжает fallback
      ([скриншот](../screenshots/saturn_single_cpu_s25_26.png));
- [x] S25–26 hang (`-DRPE_SATURN_FAULT_MODE=hang`): `slave failed: wait
      timeout`, кадры продолжаются
      ([скриншот](../screenshots/saturn_fault_hang_s25_26.png));
- [x] S25–26 flood (`-DRPE_SATURN_FAULT_MODE=flood`): `flood overflows=1`;
- [x] 10 минут dual-CPU stress: exit 124, без deadlock/hang; середина —
      `f=12300c=12303w=4035h=af890f`, hash стабилен
      ([скриншот](../screenshots/saturn_stress_s25_26.png));
- [x] host-тесты `saturn_hal_tests` — 63/63 в PC-debug.
