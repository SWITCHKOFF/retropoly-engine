# Спринт 45–46 — N64: hello_triangle, бюджеты и релиз v0.3.0 (недели 89–92)

> Детальный план спринта. Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.3).
> Источник требований: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md)
> §2.3 (N64), §4 (итерация 4); [`../RELEASING.md`](../RELEASING.md) — релиз
> обязан содержать SDK + UI client.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-14):** план; зависит от S43–44 (RSP/RDP). Любая
> публикация релизов проекта отложена до v0.9.x; v0.3.0 — **только draft**
> ([`../adr/ADR-013-release-freeze.md`](../adr/ADR-013-release-freeze.md)).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 45–46 |
| Недели | 89–92 — 03.05.2027 – 30.05.2027 (ориентир) |
| Веха | v0.3.0 — Nintendo 64 (итерация 4) |
| Цель | Полный hello_triangle/rotating_cube на N64, бюджеты W0/W1, SDK/UI-таргет N64 и draft-релиз v0.3.0 |
| Входные критерии | S43–44: RSP/RDP renderer, Z-буфер, TMEM, W0-golden |
| На критическом пути | Да (закрытие N64-итерации) |
| Выходные артефакты | N64-цели `hello_triangle`/`rotating_cube`, `benchmarks/n64_*`, скриншоты, N64 SDK-архив, UI target N64, release payload v0.3.0 (draft), docs/N64.md, CHANGELOG |
| Следующий спринт | Jaguar v0.4.0 (S47+) — по отдельному плану; любая публикация релизов отложена до v0.9.x |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 55 | 85 | 56.7 | 8.3 | 73.3 | 40–60 ч | ⚠ Интеграция/упаковка; при просадке режем UI N64-таргет до S47, но SDK/ROM/бюджеты не режем |

## Задачи

**Неделя 89 — hello_triangle и ассеты:**

- [ ] [B] N64-цель `hello_triangle`: те же `.mesh`/`.tex` v1, камера/ввод, 320×240, цикл 60 Hz
- [ ] [B] Ассет-пайплайн: `.mesh`/`.tex` → N64 runtime layout (CI8/RGBA16, тайлы, CLUT); конвертация — host-only, MIPS-тулчейн не участвует
- [ ] [B] Детерминированная конвертация/тайлинг; `.mesh`/`.tex` v1 не меняются; converter поддерживает `--platform n64`
- [ ] [B] Camera/input loop; `Platform_GetTimeMs`; стабильный кадр без дрейфа
- [ ] [H] Debug overlay: FPS, frame time, RDRAM peak, TMEM, RSP/RDP counters
- [ ] [M] Сравнить цвет/dither с PC/PS1/Saturn; задокументировать отличия

**Неделя 90 — бюджеты, W0/W1 и визуальный gate:**

- [ ] [B] Бюджеты: peak RDRAM ≤ 3.5 MiB (reserve 512 KiB), ROM/DFS ≤ 32 MiB, stack ≤ 64 KiB, TMEM ≤ 4 KiB, frame < 16.6 ms (target 60 Hz)
- [ ] [B] W1 N64: 4 видимых куба / 48 треугольников, CI8/256, Gouraud, transparency off, фиксированная камера; ≥ 30 FPS стабильно 60 с (target 60 FPS)
- [ ] [B] W0 N64: 5 кейсов, framebuffer-hash; стабильность между 3 запусками и повторными сборками
- [ ] [B] 10-минутный stress; MemoryProfiler; без утечек/деградации
- [ ] [B] `benchmarks/n64_hello_triangle_v0_3_0.yaml` + reference environment metadata (G12)
- [ ] [H] Скриншоты `screenshots/n64_hello_triangle.png`, `screenshots/n64_rotating_cube.png`; сравнение с PC/PS1/Saturn

**Неделя 91 — SDK, UI и release payload:**

- [ ] [B] N64 SDK package: headers/libs/CMake package/toolchain wrapper/examples/checksums; `sdk_manifest.json` с target `n64`
- [ ] [B] UI client target N64: обнаружение SDK, build/run в pinned эмуляторе; `.rpeproject` target `n64`; docs
- [ ] [B] Release payload v0.3.0: `sdk/` включает N64, `ui/`, optional `demos/n64`, `checksums.sha256`; BIOS/ROM/ключи запрещены
- [ ] [B] `verify_release_contract.py`: N64 asserts; negative-tests (нет N64 SDK, нет ROM, BIOS в payload)
- [ ] [B] `release.yml`: reusable N64 build → gate → **draft only** (ADR-013); public publish — не ранее v0.9.x, вручную
- [ ] [B] `docs/N64.md` финал; README supported matrix; CHANGELOG `[0.3.0]`; `VERSION.txt` 0.3.0
- [ ] [H] Release notes: что внутри SDK/UI, supported platforms, известные ограничения, checksums

**Неделя 92 — E2E и приёмка:**

- [ ] [B] Clean-room: распаковать N64 SDK → consumer `hello_triangle` собирается через `find_package` → `.z64` запускается в pinned эмуляторе
- [ ] [B] UI E2E: New Project → target N64 → import assets → Build → Run в эмуляторе
- [ ] [B] CI green: `build-n64`, `tests`, `sdk-package`, `ui-client`, release gate; negative-tests PASS
- [ ] [B] Payload audit: нет BIOS/ROM/ключей/build-каталогов; checksums сходятся
- [ ] [B] Приёмка v0.3.0: gate §13.3; релиз остаётся draft/frozen
- [ ] [M] Метрики/анонс — после v0.9.x

## Критерии приёмки спринта 45–46

- [ ] `hello_triangle` и `rotating_cube` N64 работают в pinned эмуляторе; 60 с и 10-минутный stress PASS
- [ ] W1 ≥ 30 FPS (target 60), frame < 33 ms (gate) / < 16.6 ms (target); бюджеты RDRAM/TMEM/ROM соблюдены
- [ ] W0: 5 кейсов детерминированы; hash стабилен; отличия от PS1/Saturn задокументированы
- [ ] N64 SDK-архив проходит clean `find_package`; UI собирает и запускает N64 target
- [ ] Release payload содержит `sdk/` + `ui/` + checksums; negative-tests падают без N64 SDK/ROM
- [ ] Нет BIOS/ROM/ключей/build-каталогов; v0.2.x остаются withdrawn
- [ ] CHANGELOG/VERSION/README/docs/N64.md/RELEASING согласованы

## Ручной чек-лист приёмки

- [ ] Запустить `hello_triangle` и `rotating_cube` в pinned эмуляторе; проверить ввод/камеру/кадр
- [ ] Снять скриншоты, проверить overlay (FPS, RDRAM, TMEM, frame time)
- [ ] Clean-room SDK: `find_package` → сборка → запуск `.z64`
- [ ] UI: New Project → N64 → import → Build → Run
- [ ] Проверить `sha256sum -c checksums.sha256`; отсутствие BIOS/ROM/ключей
- [ ] 10-минутный стресс без краша; повторный запуск/сброс эмулятора

## Риски

- **Производительность N64 ниже плана.** Митигация: 320×240, CI8, меньше overdraw; gate ≥ 30 FPS, target 60; scope reduction эффектов.
- **N64 SDK/тулчейн тяжёлый для упаковки.** Митигация: env-install/toolchain wrapper, checksums, notices; не вендорить BIOS/ROM.
- **Ассет-тайлинг расходится с PC/PS1.** Митигация: host-only конвертация, golden per platform, `.mesh`/`.tex` v1 не ломать.
- **Давление «опубликовать v0.3.0».** Митигация: ADR-013 hard rule, `release.yml` draft-only; публикация — не ранее v0.9.x.
