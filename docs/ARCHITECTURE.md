# Архитектура RetroPolyEngine

> Нормативные источники:
> [`RetroEngine_Engineering_Documentation_v1.43.md`](RetroEngine_Engineering_Documentation_v1.43.md)
> и [`DEVELOPMENT_PLAN.md`](DEVELOPMENT_PLAN.md). При расхождении приоритет у
> нормативных секций инженерной документации.

## 1. Высокоуровневая структура

Движок построен из четырёх слоёв, каждый следующий зависит только от
предыдущих:

```
┌─────────────────────────────────────────────┐
│  examples/        hello_triangle, rotating_cube│
│  game/            systems + scene helpers     │
├─────────────────────────────────────────────┤
│  src/render/      ядро рендера                │
│  src/assets/      форматы .mesh/.tex v1       │
├─────────────────────────────────────────────┤
│  src/hal/         Hardware Abstraction Layer  │
│    pc/            SDL3 + OpenGL 3.3           │
│    ps1/           PSn00bSDK + GTE + GPU + OT  │
│    saturn/        Jo Engine + VDP (v0.2.0)    │
├─────────────────────────────────────────────┤
│  src/core/        математика, память, ECS, log│
└─────────────────────────────────────────────┘
```

Правила зависимостей:

- `examples/` и `game/` ссылаются только на `src/core/*`, `src/assets/*`,
  `src/render/*` и `src/hal/platform.h`. Прямых вызовов SDL3/OpenGL/PSn00bSDK
  в прикладном коде нет.
- `src/render/` и `src/assets/` не знают о бэкенде: они используют
  `hal/render_backend.h` и `hal/platform.h`.
- `src/hal/` — единственное место, где живут платформенные зависимости.
- `src/core/` не зависит от HAL, рендера и assets.

## 2. HAL-подход

Вместо runtime virtual dispatch в горячих путях используется **compile-time
backend selection** через CMake:

- Ровно один из `PLATFORM_PC` / `PLATFORM_PS1` / `PLATFORM_SATURN` включён
  (guard в корневом `CMakeLists.txt`).
- `src/CMakeLists.txt` подключает `src/hal/pc/`, `src/hal/ps1/` или
  `src/hal/saturn/` (v0.2.0; ADR-008).
- Единый публичный заголовок [`src/hal/platform.h`](../src/hal/platform.h)
  объявляет весь наружный API; реализации — в `.cpp` бэкенда. Saturn не
  протаскивает SH-2/Jo Engine в публичный заголовок: контакт с SDK
  изолирован в `src/hal/saturn/jo_saturn_bridge.c`.
- Saturn S25–26: Master + Slave SH-2 взаимодействуют через внутренние
  `scu_sync`/`slave_worker` (SPSC-очередь, cache-through shared Work RAM-L).
  Slave выполняет только compute-job'ы и не трогает VDP/VRAM/периферию;
  отказ Slave → single-CPU fallback (ADR-009).

Это исключает виртуальные вызовы в кадре и даёт компилятору полную
видимость для inline/оптимизаций.

## 3. Слой Core

### 3.1 Fixed-point математика (`src/core/math/`)

- `fixed_t` = `int32_t`, формат Q16.16.
- saturating add/sub/mul/div; `fixed_div(0)` — детерминированный контракт.
- Векторы/матрицы: `Vec2/Vec3/Vec4`, `Mat3/Mat4` column-major.
- Детерминированная тригонометрия `sin_fixed`/`cos_fixed` — целочисленный
  полином Тейлора, без float.

### 3.2 Память (`src/core/memory/`)

- `LinearAllocator` — bump-арена с overflow-safe mark/rollback.
- `PoolAllocator<T,N>` — фиксированный пул с free-list.
- `StackAllocator` — вложенные фреймы.
- `MemoryProfiler` — учёт RAM/VRAM по зарегистрированным источникам.

В runtime запрещены `new`/`delete`/`malloc`/`free`; все аллокации — через
ядро.

### 3.3 ECS (`src/core/ecs/`)

- `Entity` — 24-bit index + 8-bit generation; `INVALID_ENTITY = 0`.
- `Registry` — 7 MVP-пулов с фиксированными ёмкостями (Position/Rotation/
  Velocity 1024, MeshRenderer/Collider 256, Camera/PlayerControl 8).
- `ComponentPool<T,N>` — dense-массив + sparse-индекс, swap-back удаление.
- `View<Components...>` — итерация по dense-массиву первого компонента с
  кэшированием указателей на остальные; без sparse-lookup в теле цикла.

## 4. Ядро рендера (`src/render/`)

### 4.1 Pipeline

Конвейер кадра (§7.1 инженерной документации):

```
TRANSFORM → CLIP → CULL → SORT → PACK → SUBMIT
```

1. **TRANSFORM**: вершины меша преобразуются матрицей `model·view·proj`
   (fixed 16.16).
2. **CLIP**: треугольник с вершиной вне `[near..far]` отбрасывается целиком.
3. **CULL**: PS1-NCLIP-эмуляция — знаковая площадь в экранных координатах
   (y вниз); A < 0 — фронт.
4. **SORT**: `depth_key` (far→near) + tie-breaker `submission_index`;
   вставочная сортировка `SortRef`.
5. **PACK**: каждый треугольник упаковывается в командный буфер кадра
   (64 KiB).
6. **SUBMIT**: бэкенд отрисовывает команды в reference framebuffer 320×240,
   затем выкладывает nearest на окно.

### 4.2 Контракты рендера

- Reference resolution: 320×240 (PC и PS1). Saturn v0.2.0 отображает
  центральные 224 строки (native 320×224, policy crop; ADR-008 §4).
  `hello_triangle` (S31–32) совмещает VDP1-полигоны/текстуры с VDP2 NBG1
  bitmap-фоном; VDP1 framebuffer стирается в transparent, scroll VDP2
  коммитится в VBlank, Master+Slave получают лёгкий workload каждый кадр
  (60 Гц).
- Цвет: RGB555+STP для текстур, Gouraud-интерполяция вершинного цвета.
- Depth: `depth_key_from_camera_z` без modulo-aliasing; `DEPTH_REJECT`
  за пределами `[depth_min..depth_max]`.
- Безопасность: command-buffer/OT/arena overflow возвращает `false` +
  диагностический код; silent drop запрещён.

### 4.3 W0-конформанс

Корректность рендера верифицируется readback framebuffer + FNV-1a 64
хэш, сжатый к 15-битному RGB (устойчив к LSB-джиттеру драйверов).
Эталоны хранятся в `tests/visual/reference/*.bin`.

## 5. Ассет-пайплайн (`src/assets/`, `tools/asset_converter/`)

```
glTF/PNG (source) ──asset_converter──▶ .mesh/.tex v1 ──▶ загрузчик движка
```

- Конвертер — host-only, не собирается MIPS-тулчейном.
- PS1-сборка потребляет заранее сгенерированные `assets/build/*`.
- Форматы: little-endian, побайтовая сериализация, без struct-cast.
- Загрузчики выполняют 7-шаговую валидацию ДО аллокации; malformed-вход
  расходует 0 байт persistent-памяти (rollback арены).

См. подробности в [`ASSETS.md`](ASSETS.md).

## 6. Платформенные оптимизации

### 6.1 PC

- SDL3: окно 1280×960, GL 3.3 Core, статическая линковка SDL3-static.
- OpenGL: reference FBO 320×240 RGBA8 (без depth-attachment);
  `glDisable(GL_DEPTH_TEST/BLEND/CULL_FACE)` — куллинг и сортировка в ядре.
- Ввод: SDL_Gamepad API + клавиатурная эмуляция цифрового PS1-пада.

### 6.2 PS1

- PSn00bSDK v0.24 (pinned), toolchain `sdk.cmake`.
- GTE для трансформаций вершин; GPU+Ordering Table (1024 bucket).
- VRAM: texture pages 256×256 (5 слотов), CLUT в слотах y=240+slot.
- CD-ROM ФС: `CdSearchFile` → `CdRead`/`CdReadSync`.
- Двойной framebuffer 320×240 16bpp.

## 7. Слой игры (`game/`)

- `components.h` — единая точка включения компонентов.
- `game/systems/` — header-only системы: Camera, Input, Movement, Render,
  Collision, Rotation, Lighting.
- Примеры `examples/hello_triangle/` и `examples/rotating_cube/` показывают
  полный цикл: инициализация → загрузка ассетов → ECS-сцена → кадр.

## 8. Сборка и тестирование

Канонические пресеты — единственная точка входа:

```sh
cmake --preset pc-debug && cmake --build --preset pc-debug
ctest --preset pc-debug --output-on-failure
```

Подробнее — [`BUILDING.md`](BUILDING.md) и [`TESTING.md`](TESTING.md).
