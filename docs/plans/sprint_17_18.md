# Спринт 17–18 — Rotating Cube — демо MVP (недели 33–36)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§5).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 17–18 |
| Недели | 33–36 — 21.06.2027 – 18.07.2027 |
| Веха | M7 — Rotating Cube |
| Цель | Финальная демка: W1-сцена (4 куба = 48 треугольников), коллизии, освещение, overlay |
| Входные критерии | Спринт 15–16 завершён (hello_triangle PS1 + бюджеты) ([`sprint_15_16.md`](sprint_15_16.md)) |
| На критическом пути | Да |
| Выходные артефакты | `examples/rotating_cube/`, CollisionSystem/гравитация, vertex lighting/Gouraud, debug overlay, скриншоты + видео демки |
| Следующий спринт | [`sprint_19_20.md`](sprint_19_20.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 30 | 45 | 65 | 45.8 | 5.8 | 57.5 | 40–60 ч | Укладывается |

## Задачи

**Неделя 33 — Сцена:**
- [x] [B] `examples/rotating_cube/` + CMakeLists + README
- [x] [B] Ассеты: несколько мешей/текстур (cube.gltf ×N, texture ×N) → `assets/build/`
      — меш ОДИН (cube.mesh переиспользуется 4 кубами через `mesh_id 0`;
      W1-геометрия однородна, RAM-бюджет), текстур ЧЕТЫРЕ варианта
      (texture_0..3.png → .tex; по одной 256×256 на куб — стресс Median Cut)
- [x] [B] W1-сцена: 4 видимых куба = 48 текстурированных треугольников, CI8/256, Gouraud on, transparency off, ≤ 1 текстура 256×256 на куб, фиксированная камера (эталонная расстановка)
      — расстановка: ДВЕ БАШНИ по 2 куба (контакт куб-куб с первого кадра),
      камера (0, 3.2, 7), pitch −0.15, FOV 70°; инвентарь 20 submitted /
      28 backface (невидимые грани замкнутого корпуса — норма куллинга)
- [x] [B] Загрузка нескольких мешей/текстур; RotationSystem (fixed 90°/сек yaw)
      — `game/systems/rotation_system.h` (квантование fixed-accumulate)

**Неделя 34 — Коллизии/физика (demo-level):**
- [x] [B] `CollisionSystem` (AABB): intersect-тесты, response (выталкивание), не проходит сквозь
      — замкнутые интервалы (касание = контакт) — сходимость к точному
      покою; ось минимального проникновения (ничья X→Y→Z); статика/динамика
- [x] [B] Простая гравитация/прыжок на fixed-point (НЕ полноценная физика)
      — −9.8/+4.9 (demo-level; `rc_gravity_and_jump`, фаза GameUpdate)
- [x] [B] Тесты: пересечение/разделение AABB; детерминизм при 60 Гц-реплей
      — `tests/ecs/collision_test.cpp` (9 кейсов, включая реплей демо-сцены
      с совпадением хэшей и инвариантом «не ниже пола»)

**Неделя 35 — Освещение:**
- [x] [B] Vertex lighting: ambient + directional (fixed)
      — `game/systems/lighting_system.h`, точка канала =
      clamp255(ambient + light·max(0, dot(R·n, L)))
- [x] [B] Gouraud: per-vertex RGB → интерполяция (PC: varying; PS1: GT3 c r/g/b per vertex)
      — конвейер ядра уже интерполирует varying/POLY_GT3; lit-копии вершин
      поданы через RenderBanks::lights (PC+PS1 один и тот же банк W1)
- [ ] [B] Сверка PC/PS1 на эталонной сцене
      — PC-эталон: golden `rotating_cube_default.bin` + скриншот; PS1-порт
      сцены собран (`main_ps1.cpp`, ISO с 4 текстурами), ручной прогон
      DuckStation/сверка — в ручном чек-листе ниже

**Неделя 36 — Полировка:**
- [x] [B] Debug overlay: FPS, RAM usage, triangle count, (опц. VRAM)
      — `Render_OverlayText` (ядро) + шрифт 3×5 + backend-путь
      (PC: identity-MVP NDC; PS1: POLY_G3 в OT bucket 0); TAB/Select;
      golden `overlay_text_default.bin`
- [x] [B] Стресс: 10 минут работы без краша (PC + DuckStation), лог FPS-мин/макс/средн
      — PC: `rotating_cube 600` (см. docs/benchmarks/reference_environment.yaml);
      DuckStation — ручной чек-лист (ниже)
- [x] [B] Прогон полной цепочки тестов: unit + integration (pc-debug-render под xvfb) + asset malformed
      — 177/177 unit+integration, 16/16 visual (W0/hello_triangle/
      rotating_cube/overlay), malformed-набор без изменений зелёный
- [x] [B] W1-метрики: PC ≥ 60 FPS, PS1-эмулятор ≥ 30 FPS; обновить benchmarks/reference_environment.yaml
      — PC-полка зафиксирована; PS1 ≥ 30 FPS — ручной замер в DuckStation
- [x] [B] Скриншоты + видео демки (для релиза и соцсетей)
      — PC: `screenshots/rotating_cube_pc.png` из golden; PS1-скриншот и
      видео — ручной чек-лист
- [~] [B] CHANGELOG + PR
      — CHANGELOG обновлён; коммит спринта создан локально (4da8b6d);
      PR в develop — ручной чек-лист (ниже)

## Ручной чек-лист приёмки (требует эмулятора/записи)

- [ ] DuckStation: образ `build/ps1-release/src/hal/ps1/RETRO.cue` — кубы
      видны/вращаются/прыгают, коллизии с полом/стеком, overlay (Select),
      10-минутный стресс без краша, PerfMon FPS/VPS ≥ 30
- [ ] PS1-скриншот `screenshots/rotating_cube_ps1.png` (F10 DuckStation)
- [ ] Видео демки (PC-прогон + PS1-прогон) для релиза/соцсетей
- [ ] PR в develop с закрытием спринта

## Критерии приёмки спринта 17–18

- [~] W1-ворклоуд выполняет гейты FPS/памяти на обеих платформах
      — PC: гейт выполнен с запасом (см. benchmarks/reference_environment.yaml);
      PS1: RAM/VRAM/PS-EXE-бюджеты статически сходятся (пул 256 KiB W1,
      PS-EXE 110 592 Б), FPS ≥ 30 — ручной замер DuckStation (чек-лист выше)
- [x] AABB-коллизии + гравитация работают; Gouraud работает
- [~] Overlay показывает FPS/RAM/triangles; 10-минутный стресс чистый
      — PC: стресс 600 с чистый; DuckStation-стресс — чек-лист выше
- [x] README демки + материалы для релиза
      — README + PC-скриншот; PS1-скриншот/видео — чек-лист выше

Легенда: [x] — выполнено и проверено локально; [~] — выполнено частично
(машинная часть закрыта, ручная приёмка в эмуляторе/запись — чек-лист выше).
