# Спринт 43–44 — N64: RSP-микрокод, RDP и аппаратный Z-буфер (недели 85–88)

> Детальный план спринта. Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.3).
> Источник требований: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md)
> §2.3 (N64): RCP (RSP+RDP), 18-bit Z, perspective-correct texturing,
> тайловые текстуры и TMEM 4 KiB.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-14):** план; зависит от S41–42 (HAL/DFS/память).
> Любая публикация релизов проекта отложена до v0.9.x (ADR-013).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 43–44 |
| Недели | 85–88 — 05.04.2027 – 02.05.2027 (ориентир) |
| Веха | v0.3.0 — Nintendo 64: renderer (часть 3) |
| Цель | RSP-микрокод для трансформации/освещения + RDP-конвейер с аппаратным Z-буфером, perspective-correct текстурами и TMEM-бюджетом |
| Входные критерии | S41–42: HAL, DFS-ассеты, memory ledger, command buffer skeleton |
| На критическом пути | Да (самый рискованный спринт N64) |
| Выходные артефакты | `src/hal/n64/rsp_*`, `rdp_*`, render backend, W0-golden, `benchmarks/n64_render_s43_44.yaml`, скриншоты, ADR-015 |
| Следующий спринт | [`sprint_45_46.md`](sprint_45_46.md) — hello_triangle/rotating_cube, бюджеты и v0.3.0 |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 45 | 65 | 100 | 67.5 | 9.2 | 85.8 | 40–60 ч | ⚠ Максимальный риск: RSP-ucode + TMEM; baseline libdragon `t3d`/`rdpq`, CPU-fallback; custom ucode режется первым |

## Задачи

**Неделя 85 — RSP-микрокод и вершинный конвейер:**

- [ ] [B] ADR-015: стратегия RSP — baseline libdragon `t3d`/`rdpq` vs собственный ucode; матрицы, трансформация, освещение, клиппинг; CPU-fallback и границы v0.3.0
- [ ] [B] Интеграция ucode: загрузка/запуск, лимиты IMEM/DMEM (4 KiB каждый), overlay-планирование, детерминизм
- [ ] [B] Формат вершины N64: position/normal/UV/color, alignment, fixed 16.16 vs float (R4300 FPU) — решение и тесты
- [ ] [B] Transform/lighting: model→world→view→clip, perspective divide; Gouraud ambient+directional; W0-векторы
- [ ] [B] Matrix stack и переполнение — явная диагностика; никакого per-frame malloc
- [ ] [H] Бенчмарк CPU vs RSP: 48 и 256 треугольников, µs/кадр; зафиксировать в `benchmarks/`
- [ ] [M] Сравнить результат с PC/PS1/Saturn fixed-point путём; задокументировать расхождения

**Неделя 86 — RDP-конвейер, Z-буфер и текстуры:**

- [ ] [B] RDP command stream: triangle primitives (shade/texture), bounded buffer, sync по DP completion
- [ ] [B] Аппаратный Z-буфер: 320×240 depth в RDRAM, clear, depth test; проверить 18-bit internal representation и политику хранения
- [ ] [B] Perspective-correct texturing: s/t/w на вершину, без affine-fallback; проверить на наклонной плоскости
- [ ] [B] Текстурный путь: CI8/RGBA16, тайлы 64×64/32×64, TMEM 4 KiB, загрузка через RDP tile load, CLUT/палитры
- [ ] [B] Bilinear filtering (hardware) — политика N64; отличие от PS1 nearest зафиксировать в golden/docs
- [ ] [H] RSP→RDP интеграция: вывод ucode сразу в команды, синхронизация без гонок
- [ ] [M] 16-bit output/dither; проверить, что VI не делает неожиданный scale

**Неделя 87 — backend, W0-golden и тесты:**

- [ ] [B] `Render_*` → N64 backend: BeginFrame/Clear/DrawTriangle/BindTexture/EndFrame; overflow → `false` + диагностика
- [ ] [B] Texture cache/TMEM slots: загрузка, eviction, бюджет 4 KiB; ни одного silent drop
- [ ] [B] W0-golden N64: 5 кейсов (visible/backface-culled/near-cross/far-cross/invalid-depth), app-side FNV-hash 320×240 framebuffer; стабильность между запусками
- [ ] [B] Host-тесты: генерация RDP-команд, Z-encoding, tiling, UV/perspective-математика, overflow
- [ ] [B] `build-n64.yml` собирает render-demo; artifacts: `.z64`, map, ROM size
- [ ] [H] 10-минутный stress render-loop в pinned эмуляторе; без краша/утечек
- [ ] [M] Скриншот `screenshots/n64_render_s43_44.png`

**Неделя 88 — производительность, документы и приёмка:**

- [ ] [B] W1-подобный замер: 4 куба / 48 треугольников, 320×240, 60 Hz VI; frame time, FPS, RSP/RDP-загрузка; `benchmarks/n64_render_s43_44.yaml` + метаданные окружения (G12)
- [ ] [B] `docs/N64.md`: RSP/RDP, Z, текстуры/TMEM, отличия от PS1/Saturn
- [ ] [B] ADR-015 accepted (ucode/Z/texture-форматы/fallback)
- [ ] [B] Приёмка: текстурированный Gouraud-куб в pinned эмуляторе; W0-hash стабилен; host-тесты зелёные
- [ ] [M] Сравнить скриншоты PC/PS1/Saturn/N64, перечислить ожидаемые различия

## Критерии приёмки спринта 43–44

- [ ] RSP реально используется для трансформации/освещения; RDP растеризует; CPU-fallback — только аварийный режим с диагностикой
- [ ] Аппаратный Z-буфер работает; перекрывающиеся треугольники сортируются корректно без Painter's-хака
- [ ] Perspective-correct текстурирование подтверждено на наклонной геометрии; TMEM ≤ 4 KiB
- [ ] W0: 5 обязательных кейсов детерминированы; framebuffer-hash стабилен между запусками
- [ ] Переполнение command buffer/TMEM/арены → явный `false` + диагностика, без silent drop
- [ ] 48 треугольников укладываются в кадр 60 Hz (target) или ≥ 30 FPS (gate) в pinned эмуляторе
- [ ] 10-минутный stress PASS; `docs/N64.md`, ADR-015, benchmark согласованы

## Ручной чек-лист приёмки

- [ ] Запустить render-demo в pinned эмуляторе: куб вращается, текстура видна, Z-перекрытия корректны
- [ ] Проверить наклонную плоскость/UV — нет affine-warp
- [ ] Снять скриншот и сравнить с PC/PS1/Saturn; зафиксировать отличия (bilinear, 16-bit, Z)
- [ ] Включить CPU-fallback и сравнить кадр/hash с RSP-путём
- [ ] Посмотреть лог TMEM/command buffer/арены — нет переполнений
- [ ] 10-минутный stress без краша/деградации

## Риски

- **RSP-микрокод сложнее, чем кажется.** Митигация: baseline libdragon `t3d`/`rdpq`, собственный ucode — только если baseline не проходит gate; CPU-fallback.
- **TMEM 4 KiB вызывает thrash.** Митигация: маленькие текстуры/CI8, тайлы 64×64/32×64, явный budget и eviction-политика.
- **Z-буфер/порядок отличается от PS1/Saturn.** Митигация: per-platform golden, документированные различия; не тащить Painter's на N64.
- **Эмуляторная точность RDP/RSP.** Митигация: pinned emulator, app-side hash, повторяемые настройки, ручной gate.
