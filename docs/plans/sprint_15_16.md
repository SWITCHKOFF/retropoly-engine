# Спринт 15–16 — Hello Triangle на PS1 (недели 29–32)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§5).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 15–16 |
| Недели | 29–32 — 24.05.2027 – 20.06.2027 |
| Веха | M6 — Hello Triangle PS1 |
| Цель | Тот же пример на PS1 из bootable-образа; соблюдение бюджетов |
| Входные критерии | Спринт 13–14 завершён (HAL PS1) ([`sprint_13_14.md`](sprint_13_14.md)) |
| На критическом пути | Да |
| Выходные артефакты | `memory_profiler.h`, PS1-сборка hello_triangle, `benchmarks/reference_environment.yaml` (секция ps1_emulator), `scripts/check_binary_size.sh`, скриншоты PC vs PS1 |
| Следующий спринт | [`sprint_17_18.md`](sprint_17_18.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 50 | 75 | 51.7 | 6.7 | 65.0 | 40–60 ч | ⚠ При 10 ч/нед дефицит 11.7 ч → scope reduction или recovery-неделя после S16 |

## Задачи

**Неделя 29 — Ассеты с CD:**
- [ ] [B] Адаптировать main-путь hello_triangle под PS1 (общий game-код; различия только в HAL)
- [ ] [B] Загрузка `triangle.mesh`/`texture.tex` с ISO9660; дисковая разметка в ps1-disc.xml
- [ ] [B] Тест: ассеты загружаются (лог + видимость)

**Неделя 30 — Рендер-система PS1:**
- [ ] [B] RenderSystem через GTE+OT (общая game-система, backend-бридж)
- [ ] [B] Камера fixed-point; отрисовка треугольника; сверка скриншота PC vs PS1 (перечислить ожидаемые raster-различия)

**Неделя 31 — Управление:**
- [ ] [B] InputSystem: единая абстракция клавиатура(P)/геймпад(PC+PS1); движение камеры с D-pad/стика

**Неделя 32 — Производительность и бюджеты:**
- [ ] [B] MemoryProfiler (`core/memory/memory_profiler.h`): total/used RAM+VRAM, per-allocators; print_stats + check_thresholds (алерт > 90%)
- [ ] [B] Измерить W1-equivalent в DuckStation (pinned build, speed 100%, vsync off, 60 s): ≥ 30 **completed engine frames/s**, скорость эмулятора ≥ 98%
  - [ ] Зафиксировать в `benchmarks/reference_environment.yaml` секцию ps1_emulator (build/commit, renderer/backend, region, host os/cpu, git_revision, asset_set_sha256)
  - [ ] **Формулировка:** это emulator validation gate, НЕ доказательство 30 FPS на реальном железе
- [ ] [B] Проверка peak main RAM ≤ 1 572 864 байт (1.5 MiB), включая runtime/SDK/stack/static/арены; VRAM ≤ 819 200 байт (800 KiB) — методика замера VRAM задокументирована PS1-бэкендом
- [ ] [B] Размер PS-EXE: `scripts/check_binary_size.sh` (лимит 500 KiB / 512000 байт)
- [ ] [B] Скриншоты PC vs PS1 в `screenshots/`
- [ ] [B] CHANGELOG + PR

## Критерии приёмки спринта 15–16

- [ ] hello_triangle на PS1 из .cue/.bin; камера с геймпада; ≥ 30 FPS (эмулятор); RAM/VRAM в бюджете; PS-EXE ≤ 500 KiB
