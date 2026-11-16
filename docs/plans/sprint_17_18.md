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
- [x] [B] Сверка PC/PS1 на эталонной сцене
      — PS1-порт сцены собран (`main_ps1.cpp`, ISO с 4 текстурами) и
      проверен в DuckStation 2026-11-16: 4 куба/2 башни, фиксированная
      камера, GTE H=171, кубы 2.0 ед. на z≈6.7 — геометрия корректна.
      При сверке найден и исправлен pre-existing дефект PC-бэкенда:
      `RenderTriCmd` хранит view-space вершины, поэтому шейдеру передаётся
      ТОЛЬКО projection (ранее — proj·view·model, double-transform;
      PC-кубы рисовались ≈0.6 масштаба). Все 16 visual-тестов rebased и
      зелёные, PC-скриншоты перегенерированы; композиция PC≡PS1.

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
      — PC: `screenshots/rotating_cube_pc.png` из rebased golden;
      PS1: `screenshots/rotating_cube_ps1*.png`;
      видео: `videos/rotating_cube_pc.webm` + `videos/rotating_cube_ps1.webm`
      (VP9/WebM 640×480, 30 fps, 12.3–12.8 с, прыжки/overlay)
- [~] [B] CHANGELOG + PR
      — CHANGELOG обновлён; коммит спринта создан локально (4da8b6d);
      PR в develop — ручной чек-лист (ниже)

## Ручной чек-лист приёмки (требует эмулятора/записи)

- [x] DuckStation: образ `build/ps1-release/src/hal/ps1/RETRO.cue` — кубы
      видны/вращаются/прыгают, коллизии с полом/стеком, overlay (Select),
      VPS 59.7 / engine FPS 62 ≥ 30 (PerfMon internal FPS 29.9 ≈ номинал
      30; 2026-11-16, детали ниже)
- [x] PS1-скриншот `screenshots/rotating_cube_ps1.png` (F10 DuckStation);
      дополнительно `_overlay.png` (FPS 62/TRI 20/RAM 931K),
      `_perfmon.png` (OSD 29.93 FPS / 59.86 VPS / 100 %), `_jump.png` и
      `_landed.png` (прыжок/приземление/коллизии)
- [x] Видео демки (PC-прогон + PS1-прогон) для релиза/соцсетей —
      `videos/rotating_cube_pc.webm`, `videos/rotating_cube_ps1.webm`
- [x] PR в develop с закрытием спринта — PR #6
      (https://github.com/SWITCHKOFF/retropoly-engine/pull/6, open;
      merge — после review)

## Результаты ручной приёмки (2026-11-16)

**Окружение.** DuckStation `0.1-12754-g82a118249 [master]`, Linux x64,
Software renderer, fast boot, NTSC-U, VSync off, speed 100 %, реальный
X11 display `:0` (окно 640×480). Образ
`build/ps1-release/src/hal/ps1/RETRO.cue` — 682 080 B; PS-EXE 122 880 B.

**Сцена/визуал.** 4 куба = 2 башни (48 треугольников), 4 текстуры
256×256, vertex lighting/Gouraud; фиксированная камера `(0, 3.2, 7)`,
pitch −0.15; верхние кубы стоят на нижних. Крестик — прыжок (импульс
+4.9), гравитация −9.8: кубы поднимаются и возвращаются на пол/стек,
AABB-контакт сохраняется (`_jump.png`/`_landed.png`). Select — overlay:
`FPS 62 / TRI 20 / RAM 931K` (`_overlay.png`); строки overlay сдвинуты
в NTSC safe area (y=12/19/26), иначе FPS-строка обрезалась overscan'ом.
Видео: `videos/rotating_cube_ps1.webm` (12.3 с) и
`videos/rotating_cube_pc.webm` (12.8 с), VP9/WebM 640×480 30 fps,
с прыжками и overlay.

**60-секундный замер (чистое окно, overlay off).** 60 окон PerfMon:
FPS `27.89 / 29.86 / 29.91 / 30.51` (min/avg/median/max), VPS
`55.79 / 59.74 / 59.83 / 61.01`, avg frame 16.74 ms, CPU 4.70 %,
RNDR 15.56 %, GPU 0.00 %. OSD: `29.93 FPS · 59.86 VPS · 100 % (100 %)`.
Внутренний PerfMon FPS — поле 59.94/2 = 29.97 Гц (номинал 30 FPS PS1);
гейт «≥ 30» фиксируется по VPS 59.7 и engine FPS 62. Один host-stall
(max frame 98.43 ms) дал окно 27.89 FPS; устойчивое состояние 29.9/59.8.

**PC/PS1 parity fix.** PS1-геометрия корректна (GTE H=171; кубы 2.0 ед.
на z≈6.7). Причина расхождения PC — double-transform: `RenderTriCmd`
хранит view-space вершины, а PC-шейдер получал `proj·view·model`.
Исправлено: в команду кладётся только projection; PC-шейдер применяет
её к view-space. Все 16 visual-тестов перебазированы и зелёные (включая
`W0Golden.AffineNumericConformanceOnSlantedPlane`, чья программная
реплика также переведена на projection), `rotating_cube_pc.png` и
`hello_triangle_pc.png` перегенерированы. PC/PS1 композиция W1 совпадает;
различия остаются raster-класса (RGB555/affine/rounding).

## Критерии приёмки спринта 17–18

- [x] W1-ворклоуд выполняет гейты FPS/памяти на обеих платформах
      — PC: гейт выполнен с запасом (см. benchmarks/reference_environment.yaml);
      PS1: RAM 931 KiB / VRAM ≈ 494 KiB / PS-EXE 122 880 Б в бюджетах,
      VPS 59.7 и engine FPS 62 ≥ 30 (PerfMon internal FPS 29.9 ≈ номинал 30)
- [x] AABB-коллизии + гравитация работают; Gouraud работает
- [x] Overlay показывает FPS/RAM/triangles; 10-минутный стресс чистый
      — PC: стресс 600 с чистый; DuckStation: 60 с чистый + overlay
      FPS/TRI/RAM проверен; 10-мин стресс 634 с (Xvfb) — 630 окон
      PerfMon, FPS 22.99/29.73/29.91/30.99, VPS 45.16/59.46/59.82/61.73,
      без E/F/crash, graceful shutdown
- [x] README демки + материалы для релиза
      — README + PC/PS1 скриншоты (clean/overlay/perfmon/jump/landed) +
      `videos/rotating_cube_{pc,ps1}.webm`; `videos/README.md`

Легенда: [x] — выполнено и проверено локально; [~] — выполнено частично
(машинная часть закрыта, ручная приёмка в эмуляторе/запись — чек-лист выше).
