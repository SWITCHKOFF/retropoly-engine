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
- [ ] [B] `examples/hello_triangle/` + CMakeLists (link только `retro_engine`)
- [ ] [B] `assets/source/triangle.gltf` (Blender: простой треугольник с UV), `assets/source/texture.png` (256×256 паттерн)
- [ ] [B] Прогон через конвертер → `assets/build/triangle.mesh`, `texture.tex`

**Неделя 22 — main.cpp:**
- [ ] [B] Инициализация: Platform_Init → Render_Init(320,240) (ровно один раз), загрузка ассетов через FileSystem_LoadFile + mesh/texture loaders
- [ ] [B] `game/components.h` (POD): Position, Rotation, Velocity, MeshRenderer{mesh_id,texture_id}, Camera{fov,near,far}, PlayerControl, Collider
- [ ] [B] `game/systems/render_system.h`: итерация Registry::view<MeshRenderer,Position,Rotation> → Render_DrawMesh; камера → Render_SetCamera
- [ ] [B] Entity с треугольником; отрисовка; визуальная проверка

**Неделя 23 — Камера:**
- [ ] [B] `game/systems/input_system.h`: WASD-движение, мышь-взгляд (yaw/pitch), Space/Shift вверх/вниз, ESC-выход
- [ ] [B] Fixed-point камера: perspective + look_at из math core
- [ ] [H] FPS-счётчик (через Logger / overlay)

**Неделя 24 — Стабилизация:**
- [ ] [B] Главный цикл по контракту: fixed accumulator 60 Hz (`accumulator_60hz`, SIM_DT = FIXED_ONE/60); wall-clock только планирует тики; рендер каждый кадр
  - [ ] Внутри тика: InputSystem → GameUpdate → CollisionSystem → MovementSystem
  - [ ] После: Render_BeginFrame → Clear → RenderSystem → Render_EndFrame → frame_stack.reset_frame()
- [ ] [B] Замер W1-equivalent: ≥ 60 FPS на зафиксированной reference PC среде; заполнить `benchmarks/reference_environment.yaml` (ос/цпу/гпу/драйвер/компилятор/build/resolution/vsync/measurement 60 s/git_revision/asset sha)
- [ ] [B] `examples/hello_triangle/README.md` (управление, сборка, что видно)
- [ ] [B] Скриншот-эталон в `tests/visual/reference/` + unit визуальной регрессии (readback-hash)
- [ ] [B] CHANGELOG + PR

## Критерии приёмки спринта 11–12

- [ ] Треугольник из .mesh + текстура из .tex корректно отображается
- [ ] Камера WASD+мышь; FPS ≥ 60 (W1-equivalent, reference env записан)
- [ ] Screenshot сравнён с эталоном; README примера готов
