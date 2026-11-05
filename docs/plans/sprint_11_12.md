# Спринт 11–12 — Hello Triangle на PC (недели 21–24)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§4).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 11–12 |
| Недели | 21–24 — 29.03.2027 – 25.04.2027 |
| Веха | M4 — Hello Triangle PC |
| Цель | Первый пример: меш из .mesh, текстура из .tex, камера WASD+мышь |
| Входные критерии | Спринт 9–10 завершён (конвертер + загрузчики) ([`sprint_09_10.md`](sprint_09_10.md)) |
| На критическом пути | Да |
| Выходные артефакты | `examples/hello_triangle/`, `game/components.h`, `game/systems/{render_system,input_system}.h`, скриншот-эталон, `benchmarks/reference_environment.yaml` |
| Следующий спринт | [`sprint_13_14.md`](sprint_13_14.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 25 | 38 | 55 | 38.7 | 5.0 | 48.7 | 40–60 ч | Укладывается; самый лёгкий спринт критического пути — источник компенсации для S7–8/S13–14 |

## Задачи

**Неделя 21 — Ассеты примера:**
- [x] [B] `examples/hello_triangle/` + CMakeLists (link только `retro_engine`)
  *(коммит 9bec7f6; PLATFORM_PS1 собирает заглушку — main.cpp отделяет
  PLATFORM_PC-реализацию; PLATFORM_PC=1/HT_ASSET_DIR подаются compile-time,
  пути ассетов не зависят от CWD)*
- [x] [B] `assets/source/triangle.gltf` (Blender: простой треугольник с UV), `assets/source/texture.png` (256×256 паттерн)
  *(генератором `scripts/make_source_assets.py`, не Blender; состав: 3 общие
  вершины + 3 идентичные записи (0,1,2)×3 — норматив index_count % 3 == 0;
  текстура — уже существующий 256×256 PNG)*
- [x] [B] Прогон через конвертер → `assets/build/triangle.mesh`, `texture.tex`
  *(конвертер печатает `ok` для обоих файлов; состав mesh см. выше; golden-кадр
  примера записан hash'ем `e47452481dd56507`)*

**Неделя 22 — main.cpp:**
- [x] [B] Инициализация: Platform_Init → Render_Init(320,240) (ровно один раз), загрузка ассетов через FileSystem_LoadFile + mesh/texture loaders
  *(ht_load_assets в общем `scene.h` — header ЕДИН для main.cpp и
  golden-теста; malformed-вход расходует persistent-память арены ровно
  0 байт — rollback, контракт загрузчиков §1.2.7)*
- [x] [B] `game/components.h` (POD): Position, Rotation, Velocity, MeshRenderer{mesh_id,texture_id}, Camera{fov,near,far}, PlayerControl, Collider
  *(коммит a32ce48; тонкий манифест — включает `core/ecs/registry.h` как
  единственную точку описания ECS-компонентов ядра; дубликаты запрещены)*
- [x] [B] `game/systems/render_system.h`: итерация Registry::view<MeshRenderer,Position,Rotation> → Render_DrawMesh; камера → Render_SetCamera
  *(model = translate·Rz·Ry·Rx; BindTexture по texture_id, out-of-range
  слот — unbind flat-пути; плюс `camera_math.h` — общая формула для
  input и render систем)*
- [x] [B] Entity с треугольником; отрисовка; визуальная проверка
  *(визуальная проверка — golden-эталоном, не вручную; см. неделю 24)*

**Неделя 23 — Камера:**
- [x] [B] `game/systems/input_system.h`: WASD-движение, мышь-взгляд (yaw/pitch), Space/Shift вверх/вниз, ESC-выход
  *(мышь требует расширения HAL: `Input_SetMouseRelativeMode` /
  `Input_GetMouseDelta` (consume, накопление только в relative mode) —
  коммит 0dcacf8; диагонали нормированы; нулевое направление — нулевая
  velocity: контракт vec3_normalize делит на нулевую длину (debug assert),
  в InputSystem добавлена защитная ветка)*
- [x] [B] Fixed-point камера: perspective + look_at из math core
  *(сенса 327 fixed = 0.005 rad/px; pitch кламп pi/2−1/32 исключает
  вырождение look_at)*
- [x] [H] FPS-счётчик (через Logger / overlay)
  *(через Logger, в bench-режиме: лог в file-sink (`retro_engine.log`) —
  fps по секундным окнам + итог (кадры/мс/среднее); экранных наложений
  в интерактиве нет)*

**Неделя 24 — Стабилизация:**
- [x] [B] Главный цикл по контракту: fixed accumulator 60 Hz (`accumulator_60hz`, SIM_DT = FIXED_ONE/60); wall-clock только планирует тики; рендер каждый кадр
  - [x] Внутри тика: InputSystem → GameUpdate → CollisionSystem → MovementSystem
    *(GameUpdate — фаза зарезервирована §10: сцена без собственной
    механики)*
  - [x] После: Render_BeginFrame → Clear → RenderSystem → Render_EndFrame → frame_stack.reset_frame()
- [x] [B] Замер W1-equivalent: ≥ 60 FPS на зафиксированной reference PC среде; заполнить `benchmarks/reference_environment.yaml` (ос/цпу/гпу/драйвер/компилятор/build/resolution/vsync/measurement 60 s/git_revision/asset sha)
  *(фактический путь: `docs/benchmarks/reference_environment.yaml`;
  pc-release, 60 с: среднее 482.79 fps (28996 кадров/60058 мс), окна
  287–1160 (окно 1 ~1160 — прогрев Mesa) — полка ≥ 60 FPS с запасом ≈ ×8)*
- [x] [B] `examples/hello_triangle/README.md` (управление, сборка, что видно)
- [x] [B] Скриншот-эталон в `tests/visual/reference/` + unit визуальной регрессии (readback-hash)
  *(эталон hello_triangle_default.bin (+px-партнёр), хэш e47452481dd56507,
  бюджет 128 px — тот же класс tie-breaking силуэтных рёбер, что у room12;
  тест `HelloTriangleGolden.DefaultSceneView` строит сцену общим scene.h →
  game::RenderSystem — golden НЕ дублирует константы сцены; readback-протокол
  вынесен в tests/visual/golden_protocol.{h,cpp}, W0-эталоны
  не регрессировали)*
- [x] [B] CHANGELOG + PR
  *(CHANGELOG обновлён; PR-повестка локальна — gh CLI в песочнице нет,
  merge в `develop` коммитами, как в S3–6/S9–10)*

## Критерии приёмки спринта 11–12

- [x] Треугольник из .mesh + текстура из .tex корректно отображается
  *(golden-эталон дефолт-кадра HelloTriangleGolden.DefaultSceneView:
  submitted == index_count, reject'ов нет, draw_calls == index_count+1)*
- [x] Камера WASD+мышь; FPS ≥ 60 (W1-equivalent, reference env записан)
  *(управление протестировано интерактивной сессией; замер pc-release —
  среднее 482.79 fps; снапшот окружения 2026-11-05 — в
  `docs/benchmarks/reference_environment.yaml`)*
- [x] Screenshot сравнён с эталоном; README примера готов
  *(эталон через readback-FNV + px-фолбэк; README — управление/сборка/
  bench-протокол/golden-регрессия)*
