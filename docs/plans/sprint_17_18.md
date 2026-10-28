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
- [ ] [B] `examples/rotating_cube/` + CMakeLists + README
- [ ] [B] Ассеты: несколько мешей/текстур (cube.gltf ×N, texture ×N) → `assets/build/`
- [ ] [B] W1-сцена: 4 видимых куба = 48 текстурированных треугольников, CI8/256, Gouraud on, transparency off, ≤ 1 текстура 256×256 на куб, фиксированная камера (эталонная расстановка)
- [ ] [B] Загрузка нескольких мешей/текстур; RotationSystem (fixed 90°/сек yaw)

**Неделя 34 — Коллизии/физика (demo-level):**
- [ ] [B] `CollisionSystem` (AABB): intersect-тесты, response (выталкивание), не проходит сквозь
- [ ] [B] Простая гравитация/прыжок на fixed-point (НЕ полноценная физика)
- [ ] [B] Тесты: пересечение/разделение AABB; детерминизм при 60 Гц-реплей

**Неделя 35 — Освещение:**
- [ ] [B] Vertex lighting: ambient + directional (fixed)
- [ ] [B] Gouraud: per-vertex RGB → интерполяция (PC: varying; PS1: GT3 c r/g/b per vertex)
- [ ] [B] Сверка PC/PS1 на эталонной сцене

**Неделя 36 — Полировка:**
- [ ] [B] Debug overlay: FPS, RAM usage, triangle count, (опц. VRAM)
- [ ] [B] Стресс: 10 минут работы без краша (PC + DuckStation), лог FPS-мин/макс/средн
- [ ] [B] Прогон полной цепочки тестов: unit + integration (pc-debug-render под xvfb) + asset malformed
- [ ] [B] W1-метрики: PC ≥ 60 FPS, PS1-эмулятор ≥ 30 FPS; обновить benchmarks/reference_environment.yaml
- [ ] [B] Скриншоты + видео демки (для релиза и соцсетей)
- [ ] [B] CHANGELOG + PR

## Критерии приёмки спринта 17–18

- [ ] W1-ворклоуд выполняет гейты FPS/памяти на обеих платформах
- [ ] AABB-коллизии + гравитация работают; Gouraud работает
- [ ] Overlay показывает FPS/RAM/triangles; 10-минутный стресс чистый
- [ ] README демки + материалы для релиза
