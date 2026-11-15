# Reference-план 51–52 — Jaguar: renderer, W0/W1, SDK/UI и draft v0.4.0 (reference-недели 101–104)

> **Статус (2026-11-15): REFERENCE ONLY — DEFERRED «до лучших времён» +
> LEGAL HOLD.** План **не активируется** и не является backlog к
> исполнению; Jaguar-код/toolchain/ROM/CD/payload не создаются.
> **Активация — только по явному личному разрешению maintainer начать
> Jaguar** и после приёмки S49–50. Снятие hold само по себе не активирует
> план. До разрешения чек-боксы ниже не выполняются, а «Неделя N» —
> reference-разбиение, не календарь.
> Решение — [`../adr/ADR-016-jaguar-deferred.md`](../adr/ADR-016-jaguar-deferred.md);
> условия возобновления — [`jaguar_legal_hold.md`](jaguar_legal_hold.md).
>
> Reference-план (не активен). Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.4).
> Источник требований: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md)
> §2.4 (Atari Jaguar): программируемый растеризатор Tom, Blitter с Z/Gouraud,
> программируемый texture mapping, отсутствие готового 3D API; §4 — v0.4.0.
> [`../RELEASING.md`](../RELEASING.md) — релиз обязан содержать SDK + UI client.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> Все значения производительности и бюджеты — целевые; подтверждаются
> pinned-эмулятором и reference-метаданными. Любая публикация релизов
> отложена до v0.9.x
> ([`../adr/ADR-013-release-freeze.md`](../adr/ADR-013-release-freeze.md)),
> поэтому при возобновлении v0.4.0 готовится как **draft**.

## Активация (обязательный gate)

- [ ] **Maintainer лично и явно разрешил начать Jaguar** (отдельное
      решение, не равное снятию LEGAL HOLD).
- [ ] S49–50 активирован и принят; его критерии выполнены.
- [ ] Reference-даты заменены на реальные сроки/ресурсы; план принят
      maintainer к исполнению.
- [ ] Clean-room и BIOS-политика остаются в силе.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 51–52 |
| Недели | **не назначены**; reference: 101–104 (26.07–22.08.2027) — не commitment |
| Веха | v0.4.0 — **reference**, не текущая веха |
| Цель | Reference-цель (при активации): полный hello_triangle/rotating_cube на Jaguar, бюджеты W0/W1, SDK/UI-таргет Jaguar и draft-релиз v0.4.0 |
| Входные критерии | **Личное разрешение maintainer начать Jaguar**; S49–50 принят; HAL, ROM-ассеты, memory ledger, Tom video/Blitter skeleton готовы |
| На критическом пути | **Нет.** Вне текущего плана; после активации — да (закрытие Jaguar-итерации) |
| Выходные артефакты | Jaguar-цели `hello_triangle`/`rotating_cube`, `docs/benchmarks/jaguar_*`, скриншоты, Jaguar SDK-архив, UI target Jaguar, release payload v0.4.0 (draft), `docs/JAGUAR.md` (final), CHANGELOG |
| Следующий спринт | **Не активен.** 3DO исключён из roadmap; следующий этап вне Jaguar — аудио/ФС (v0.6.0–v0.8.0); публикация — не ранее v0.9.x |

## PERT-оценка (reference, не commitment)

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 55 | 85 | 56.7 | 8.3 | 73.3 | 40–60 ч | ⚠ Интеграция/упаковка; при просадке режем CRY/DSP-путь и UI Jaguar-таргет до следующего этапа, но ROM/бюджеты/SDK не режем |

## Задачи (reference; не выполнять до активации)

**Reference-этап 1 (неделя 101) — vertex pipeline и растризация:**

- [ ] [B] Формат вершины Jaguar: position/normal/UV/color, Q16.16, alignment; model→world→view→clip, perspective divide, backface cull, near/far
- [ ] [B] Triangle setup и edge walking на GPU/68000 в fixed-point; bounded span-очередь; никакого per-frame malloc
- [ ] [B] Blitter span path: 16-bit Z compare/write, Gouraud-интерполяция, RGB16; texture path через GPU с 64×64 RGB16 (CRY — опция), без fixed-function sampler
- [ ] [B] `Render_*` → Jaguar backend: BeginFrame/Clear/DrawTriangle/BindTexture/EndFrame; object list; command buffer 64 KiB; overflow → `false` + диагностика
- [ ] [H] Распределение нагрузки: 68000 — host/control, GPU — T&L/raster, DSP — вспомогательная математика/простой; замер и фиксация в benchmark
- [ ] [M] Сравнить fixed-point путь с PC/PS1/Saturn/N64; задокументировать расхождения (Z, RGB16, отсутствие FPU)

**Reference-этап 2 (неделя 102) — бюджеты, W0/W1 и визуальный gate:**

- [ ] [B] W0 Jaguar: 5 кейсов (visible/backface-culled/near-cross/far-cross/invalid-depth), 320×240 RGB16, FNV-hash; стабильность между 3 запусками и повторными сборками
- [ ] [B] W1 Jaguar: 4 видимых куба / 48 треугольников, текстура 64×64, Gouraud, Z, RGB16, фиксированная камера; display 60 Гц, render gate ≥ 20 FPS (frame < 50 мс), target 30 FPS (< 33.3 мс), стабильно 60 с
- [ ] [B] Бюджеты: DRAM ≤ 2 MiB (ledger S49–50), ROM/asset pack ≤ 4 MiB, stack ≤ 64 KiB, GPU local ≤ 4 KiB, Z 16-bit; `verify_jaguar_budgets.py` PASS; heap в engine-коде отсутствует
- [ ] [B] 10-минутный stress; MemoryProfiler; без утечек/деградации; повторный запуск/сброс эмулятора
- [ ] [B] `docs/benchmarks/jaguar_hello_triangle_v0_4_0.yaml` + reference environment metadata (G12)
- [ ] [H] Скриншоты `screenshots/jaguar_hello_triangle.png`, `screenshots/jaguar_rotating_cube.png`; сравнение с PC/PS1/Saturn/N64
- [ ] [M] Проверить PAL-режим/letterbox; v0.4.0 gate — NTSC 320×240, PAL — известное ограничение

**Reference-этап 3 (неделя 103) — SDK, UI и release payload:**

- [ ] [B] Jaguar SDK package: headers/libs/CMake package/toolchain wrapper/examples/checksums; `sdk_manifest.json` с target `jaguar`
- [ ] [B] UI client target Jaguar: обнаружение SDK, build/run в pinned эмуляторе; `.rpeproject` target `jaguar`; `--smoke-platform jaguar`; docs
- [ ] [B] Release payload v0.4.0: `sdk/` включает Jaguar, `ui/`, optional `demos/jaguar`, `checksums.sha256`; чужие BIOS/ROM/ключи запрещены
- [ ] [B] `verify_release_contract.py`: Jaguar asserts; negative-tests (нет Jaguar SDK, нет ROM, BIOS в payload)
- [ ] [B] `release.yml`: reusable Jaguar build → gate → **draft only** (ADR-013); public publish — не ранее v0.9.x, вручную
- [ ] [B] `docs/JAGUAR.md` финал; README supported matrix; CHANGELOG `[0.4.0]`; `VERSION.txt` 0.4.0
- [ ] [H] Release notes v0.4.0 (draft): что внутри SDK/UI, supported platforms, известные ограничения, checksums

**Reference-этап 4 (неделя 104) — E2E и приёмка:**

- [ ] [B] Clean-room: распаковать Jaguar SDK → consumer `hello_triangle` собирается через `find_package` → `.j64` запускается в pinned эмуляторе
- [ ] [B] UI E2E: New Project → target Jaguar → import assets → Build → Run в эмуляторе
- [ ] [B] CI green: `build-jaguar`, `tests`, `sdk-package`, `ui-client`, release gate; negative-tests PASS
- [ ] [B] Payload audit: нет чужих BIOS/ROM/ключей/build-каталогов; checksums сходятся
- [ ] [B] Приёмка v0.4.0: gate §13.4; релиз остаётся draft/frozen
- [ ] [M] Метрики/анонс — после v0.9.x (осознанно не выполняется до снятия freeze)

## Критерии приёмки спринта 51–52 (если активирован)

- [ ] `hello_triangle` и `rotating_cube` Jaguar работают в pinned эмуляторе; 60 с и 10-минутный stress PASS
- [ ] W1 ≥ 20 FPS (target 30), frame < 50 мс (gate) / < 33.3 мс (target); бюджеты DRAM/ROM/stack/GPU local соблюдены
- [ ] W0: 5 кейсов детерминированы; hash стабилен; отличия от PC/PS1/Saturn/N64 задокументированы
- [ ] Tom Object Processor/Blitter реально используются; Z/Gouraud/текстуры подтверждены; silent drop отсутствует
- [ ] Jaguar SDK-архив проходит clean `find_package`; UI собирает и запускает Jaguar target
- [ ] Release payload содержит `sdk/` + `ui/` + checksums; negative-tests падают без Jaguar SDK/ROM
- [ ] Нет чужих BIOS/ROM/ключей/build-каталогов; v0.2.x остаются withdrawn, v0.3.0 — draft
- [ ] CHANGELOG/VERSION/README/docs/JAGUAR.md/RELEASING согласованы

## Ручной чек-лист приёмки (если активирован)

- [ ] Запустить `hello_triangle` и `rotating_cube` в libretro Virtual Jaguar (RetroArch/standalone, GPL-3.0) или ином утверждённом внешнем эмуляторе; проверить ввод/камеру/кадр
- [ ] Снять скриншоты, проверить overlay (FPS, frame time, DRAM, GPU/DSP load, object list, Z)
- [ ] Clean-room SDK: `find_package` → сборка → запуск `.j64`
- [ ] UI: New Project → Jaguar → import → Build → Run
- [ ] Проверить `sha256sum -c checksums.sha256`; отсутствие чужих BIOS/ROM/ключей
- [ ] 10-минутный стресс без краша; повторный запуск/сброс эмулятора
- [ ] Если разрешён legal review, проверить в BigPEmu 1.221 (опционально, локально, не в payload); зафиксировать отличия

## Риски

- **Производительность Jaguar ниже плана.** Митигация: 320×240 RGB16, меньше overdraw, Z-буфер только для W1, gate ≥ 20 FPS/target 30; scope reduction текстур/CRY, но не бюджеты и не ROM.
- **68000/Blitter/RISC bus contention и hardware bugs.** Митигация: GPU/Blitter для тяжёлой работы, long-only доступ, документированные workaround'ы, app-side счётчики; libretro Virtual Jaguar (GPL-3.0) как внешний baseline; Virtual Jaguar Rx fallback; BigPEmu — optional local; позже железо.
- **Texture mapping на Jaguar полностью программный.** Митигация: 64×64 RGB16, ограниченный number of textured spans, flat/Gouraud fallback; W1 остаётся валидным без texture-хака.
- **SDK/тулчейн тяжёлый для упаковки.** Митигация: env-install/toolchain wrapper, checksums, notices; не вендорить чужие BIOS/ROM.
- **Эмуляторная точность.** Митигация: pinned baseline, app-side hash/метрики, ручной gate; эмулятор не является заявлением о реальном железе.
- **Давление «опубликовать v0.4.0».** Митигация: ADR-013 hard rule, `release.yml` draft-only; публикация — не ранее v0.9.x.
