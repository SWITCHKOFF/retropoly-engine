# plans/ — Детальные планы спринтов RetroPolyEngine

Каждый спринт (пара спринтов) MVP v0.1.0 имеет собственный файл: полный чек-лист атомарных действий, PERT-оценка, критерии приёмки, артефакты и риски. Пост-MVP v0.2.0 (Sega Saturn) детализирован отдельной таблицей ниже.

Родительский документ: [`../DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) — вехи, критический путь, PERT-сводка (§2.4), сквозные треки, гейты G1–G13, бюджеты, релизный чек-лист. Пост-MVP-план — §13 того же документа.

## Указатель

| Файл | Спринты | Недели | Даты (ориентир) | Веха | Тема | PERT E |
|---|---|---|---|---|---|---|
| [sprint_00.md](sprint_00.md) | 0 | 0 | 02.11–08.11.2026 | M0 | Среда, тулчейн, репозиторий, CMake, вендоринг, CI | 19.0 ч |
| [sprint_01_02.md](sprint_01_02.md) | 1–2 | 1–4 | 09.11–06.12.2026 | M1 | Fixed-point математика Q16.16 | 46.7 ч |
| [sprint_03_04.md](sprint_03_04.md) | 3–4 | 5–8 | 07.12.2026–03.01.2027 | M1 | Аллокаторы + ECS Registry | 46.7 ч |
| [sprint_05_06.md](sprint_05_06.md) | 5–6 | 9–12 | 04.01–31.01.2027 | M2 | HAL PC (SDL3 + OpenGL) | 43.5 ч |
| [sprint_07_08.md](sprint_07_08.md) | 7–8 | 13–16 | 01.02–28.02.2027 | M2 | Рендерер с эмуляцией PS1 + W0-golden | 52.5 ч |
| [sprint_09_10.md](sprint_09_10.md) | 9–10 | 17–20 | 01.03–28.03.2027 | M3 | asset_converter (.mesh/.tex v1) | 43.0 ч |
| [sprint_11_12.md](sprint_11_12.md) | 11–12 | 21–24 | 29.03–25.04.2027 | M4 | hello_triangle на PC | 38.7 ч |
| [sprint_13_14.md](sprint_13_14.md) | 13–14 | 25–28 | 26.04–23.05.2027 | M5 | HAL PS1 (PSn00bSDK, GTE, OT, CD) | 57.5 ч |
| [sprint_15_16.md](sprint_15_16.md) | 15–16 | 29–32 | 24.05–20.06.2027 | M6 | hello_triangle на PS1 + бюджеты | 51.7 ч |
| [sprint_17_18.md](sprint_17_18.md) | 17–18 | 33–36 | 21.06–18.07.2027 | M7 | rotating_cube (демо MVP) | 45.8 ч |
| [sprint_19_20.md](sprint_19_20.md) | 19–20 | 37–40 | 19.07–15.08.2027 | M8 | Документация, релиз, анонс | 46.7 ч |

## Post-MVP: v0.2.0 — Sega Saturn (итерация 3)

Детализация §4 «Будущие итерации» инженерной документации: 6 пар спринтов, 24 недели. Перед стартом reference-факты Saturn перепроверить по первоисточникам (требование §2.2); это ориентиры, а не MVP-контракты.

| Файл | Спринты | Недели | Даты (ориентир) | Веха | Тема | PERT E |
|---|---|---|---|---|---|---|
| [sprint_21_22.md](sprint_21_22.md) | 21–22 | 41–44 | 16.08–12.09.2027 | v0.2.0 | Saturn: изучение, toolchain, skeleton, CI | 51.7 ч |
| [sprint_23_24.md](sprint_23_24.md) | 23–24 | 45–48 | 13.09–10.10.2027 | v0.2.0 | HAL Saturn на Master SH-2 | 55.8 ч |
| [sprint_25_26.md](sprint_25_26.md) | 25–26 | 49–52 | 11.10–07.11.2027 | v0.2.0 | Параллелизм Master + Slave SH-2 | 57.5 ч |
| [sprint_27_28.md](sprint_27_28.md) | 27–28 | 53–56 | 08.11–05.12.2027 | v0.2.0 | VDP1: полигоны, текстуры, triangle→quad | 61.7 ч |
| [sprint_29_30.md](sprint_29_30.md) | 29–30 | 57–60 | 06.12.2027–02.01.2028 | v0.2.0 | VDP2: фоны, композитинг, синхронизация | 55.8 ч |
| [sprint_31_32.md](sprint_31_32.md) | 31–32 | 61–64 | 03.01–30.01.2028 | v0.2.0 | Hello Triangle на Saturn + релиз | 52.5 ч |

> Статус S21–30 (2026-11-11): S21–22 (toolchain/preset/HAL skeleton/CI/docs),
> S23–24 (HAL Master SH-2), S25–26 (Master+Slave), S27–28 (VDP1: полигоны,
> текстуры, CLUT, triangle→quad, golden) закрыты. S27–28 visual gate выполнен
> в Mednafen 1.32.1 (Japanese BIOS 1.01); 5-минутный stress — PASS;
> screenshot `screenshots/saturn_vdp1_s27_28.png`. S29–30 — VDP2 NBG1
> bitmap 1024×256 (палитра 7, 256 KiB VRAM), VDP1 поверх VDP2, VBlank scroll,
> golden `screenshots/saturn_vdp2_s29_30.png` / `saturn_vdp2_scroll_s29_30.png`;
> host `saturn_hal_tests` — 112/112; ADR-010. 10-минутный stress S29–30 —
> PASS (f=17760, exit 124); remote CI — `build-saturn` run 43085389214,
> `build-pc`/`build-ps1`/`tests` success. **S31–32** — hello_triangle
> (`hello_triangle.*`, 60 Гц, Master+Slave 16/16), release contract Saturn,
> `release.yml` payload `saturn/`; 10-мин stress PASS, remote CI green.
> **S31–32 — реализация закрыта; GitHub Release v0.2.0 отозван**:
> payload не содержит SDK и UI-клиента. Release gate v2 —
> [`../RELEASING.md`](../RELEASING.md).

## Stage 2.5: v0.2.1 — SDK + UI client (release gate v2)

Следующий этап после отзыва v0.2.0: публичный релиз обязан содержать готовый
SDK и UI-клиент ([`../RELEASING.md`](../RELEASING.md)). N64 (v0.3.0) стартует
после приёмки этого этапа — S39+.

| Файл | Спринты | Недели | Даты (ориентир) | Веха | Тема | PERT E |
|---|---|---|---|---|---|---|
| [sprint_33_34.md](sprint_33_34.md) | 33–34 | 65–68 | 16.11–13.12.2026 | v0.2.1 | SDK install/export, CMake package, consumer smoke | 55.8 ч |
| [sprint_35_36.md](sprint_35_36.md) | 35–36 | 69–72 | 14.12.2026–10.01.2027 | v0.2.1 | UI client MVP: project/target/import/build/run | 60.8 ч |
| [sprint_37_38.md](sprint_37_38.md) | 37–38 | 73–76 | 11.01–07.02.2027 | v0.2.1 | Release gate v2, SDK+UI payload, публикация | 50.0 ч |

> **Статус (2026-11-14):** S33–34 (SDK) реализован; S35–36 (UI client MVP)
> реализован: core+GUI, `.rpeproject`, import/build/run, unit-тесты,
> headless smoke и clean E2E PC, `ui-client.yml`, `scripts/package_ui.py`,
> `docs/UI_CLIENT.md`, ADR-012. Ручной PS1/Saturn gate пройден
> (DuckStation/Mednafen, `.cue` из UI-проекта); первый полный зелёный CI
> push `79ce6f4` (`build-pc/ps1/saturn`, `tests`, `sdk-package`,
> `ui-client`). S37–38 (release gate v2) завершён: payload
> `sdk/`+`ui/`, `scripts/build_release_bundle.py`, verifier
> `release_payload`, negative-tests, `create_release_archive.sh` v2 и
> `release.yml` (reusable SDK/UI → gate → publish). Remote CI `3e816d1` —
> 12/12 green; release workflow 43577691540 — success; annotated-тег
> **v0.2.1** и GitHub Release опубликованы; скачанные assets прошли
> checksums и проверку BIOS/ROM. v0.2.0 остаётся отозванным историческим
> тегом. Следующий этап — N64 **v0.3.0** (S39+); метрики релиза — через
> 1–2 недели.

Следующие итерации (N64 v0.3.0, Jaguar v0.4.0, 3DO v0.5.0, аудио/ФС v0.6.0–v0.8.0) в спецификации заданы диапазонами 6–8 / 4–6 / 4–6 / 8–12 спринтов. N64 начинается с S39 после приёмки v0.2.1 (SDK+UI release gate v2), когда будут известны фактические темп и ограничения Saturn-toolchain.

## Соглашения

- Легенда приоритетов: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
- PERT: E = (O + 4M + P) / 6; σ = (P − O) / 6; 95%-сценарий = E + 2σ. Пересчёт — на каждом sprint review (правила в §2.4 родительского плана).
- Даты привязаны к понедельникам; recovery/buffer-недели в графики не входят.
- Чек-боксы обновляются только в этих файлах; `DEVELOPMENT_PLAN.md` — своды и нормативные разделы.
