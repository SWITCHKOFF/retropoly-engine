# Спринт 37–38 — Release gate v2 и публикация v0.2.1 (недели 73–76)

> Детальный план спринта. Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.2).
> Источник требований: [`RELEASING.md`](../RELEASING.md) — SDK + UI client,
> checksums, consumer smoke.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-14):** S37–38 технически завершён: release gate v2
> (`release_payload`, `build_release_bundle.py`, negative-tests, verifier
> `--payload-only`), payload `sdk/`+`ui/`+optional `demos/`,
> `create_release_archive.sh` v2, reusable SDK/UI workflows и `release.yml`
> «gate → **draft** only». Remote CI на `3e816d1` — 12/12 green; release
> workflow 43577691540 создал GitHub Release, но он **удалён/отозван**:
> публичные релизы заморожены до **~v0.9.x** (`release freeze`). Тег
> `v0.2.1` — техническая метка; публикация — вручную после совместной
> приёмки maintainer + агент.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 37–38 |
| Недели | 73–76 — 11.01.2027 – 07.02.2027 |
| Веха | v0.2.1 — release candidate (SDK + UI); публикация отложена до ~v0.9.x |
| Цель | Собрать SDK+UI payload и release gate v2; публикация отложена/заморожена |
| Входные критерии | S33–34 (SDK) и S35–36 (UI) закрыты; v0.2.0 отозван |
| На критическом пути | Да (release gate v2) |
| Выходные артефакты | `sdk/` + `ui/` + `checksums.sha256` + `release_contract_report.json` + `payload_verification_report.json`; технический тег `v0.2.1`; публикация — после ~v0.9.x |
| Следующий спринт | v0.3.0 N64 (S39+) — разработка; public release заморожен до ~v0.9.x |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 30 | 50 | 70 | 50.0 | 6.7 | 63.3 | 40–60 ч | ⚠ Интеграция/упаковка; при просадке переносим анонс, но не SDK/UI-гейт |

## Задачи

**Неделя 73 — release payload и валидатор:**

- [x] [B] `scripts/build_release_bundle.py` (или CMake target): собирает `dist/retro-engine-v<ver>/` из SDK-архивов, UI-пакетов и optional demo-образов
- [x] [B] Layout: `sdk/`, `ui/`, `checksums.sha256`, `release_contract_report.json`, `README.md`, `LICENSE`, `LEGAL.md`; demo-образы — только `demos/` и явно optional
- [x] [B] `verify_release_contract.py`: asserts SDK manifest/headers/libs/CMake package, UI package/version, checksums, clean consumer smoke, UI headless smoke
- [x] [B] `ci/normative_contracts.yaml`: зафиксировать `release.payload = sdk+ui`, версии SDK/UI, supported hosts/targets, обязательные checksums; `release_freeze` до ~v0.9.x, `v0_2_1_release_status: withdrawn`
- [x] [B] `release.yml`: build/consume SDK+UI artifacts, consumer/UI smoke на чистой распаковке, **draft-only** при `all_ok: true`; public publish — вручную
- [x] [B] `create_release_archive.sh` v2: содержит `sdk/` + `ui/`, а demo-образы не могут подменять SDK
- [x] [B] Проверка отсутствия BIOS/ROM/ключей/build-каталогов в payload; отдельный negative-test

**Неделя 74 — документация и версия:**

- [x] [B] `docs/RELEASING.md`: обновить статус — v0.2.0 withdrawn, v0.2.1 release checklist; таблица «что считается релизом»
- [x] [B] `docs/SDK.md`, `docs/UI_CLIENT.md`: версия v0.2.1, установка, quickstart, supported hosts/targets
- [x] [B] README: download/quickstart SDK+UI, скриншоты, supported matrix; убрать demo-only как основной путь
- [x] [B] CHANGELOG `[0.2.1]`: SDK packaging, UI client, release gate v2, v0.2.0 withdrawn
- [x] [B] `VERSION.txt`/`sdk_manifest.json`/UI version = `0.2.1`; commit/ABI/checklists
- [x] [B] Обновить `docs/DEVELOPMENT_PLAN.md` roadmap: v0.2.1 release, N64 S39+
- [x] [H] Release notes: что внутри SDK/UI, supported platforms, known limitations, как проверить checksums

**Неделя 75 — полный release gate:**

- [x] [B] `build-pc`, `build-ps1`, `build-saturn`, `tests`, `sdk-package`, `ui-client` — success на release-коммите (3e816d1, 12/12 push+PR)
- [x] [B] `verify_release_contract.py --w1-seconds 60` — `all_ok: true`, включая SDK/UI asserts (локально 2026-11-14; W1 225 FPS)
- [x] [B] Clean-room: распаковать `retro-engine-v0.2.1` в пустой каталог; consumer hello_triangle собирается через `find_package`
- [x] [B] Clean-room UI: создать проект, импортировать ассет, собрать PC target, запустить (`ui_e2e.py` PASS)
- [x] [B] PS1/Saturn SDK examples собираются из пакета; ручной запуск `.cue` в pinned эмуляторе (consumer smoke PS1/Saturn PASS; ручной gate — S35–36)
- [x] [B] Negative-tests: отсутствующий SDK/UI/checksum → gate fail; BIOS в payload → gate fail (13/13 PASS)
- [x] [B] Подготовить `v0.2.1` release branch/tag; v0.2.0 tag оставить историческим

**Неделя 76 — публикация (заморожена):**

- [x] [B] Тег `v0.2.1` (annotated) — создан как техническая метка
- [ ] [B] GitHub Release — **удалён/отозван**; публикация отложена до ~v0.9.x
- [x] [B] Проверить скачанные assets: checksums, размеры, отсутствие BIOS/ROM (до отзыва)
- [x] [B] Обновить README/CHANGELOG/RELEASING статус «withdrawn / release freeze»
- [ ] [B] Анонс — запрещён до снятия release freeze
- [ ] [M] Собрать метрики релиза — после публикации v0.9.x

## Критерии приёмки спринта 37–38

- [x] Payload содержит **SDK** (headers/libs/CMake package/toolchain/examples) и **UI client**; demo-образы только optional
- [x] `verify_release_contract.py` падает без SDK/UI и проходит на v0.2.1 (negative-tests 13/13 PASS)
- [x] Clean consumer строит hello_triangle через `find_package`; UI создаёт проект/собирает/запускает
- [x] Checksums проверяются; нет BIOS/ROM/секретов/build-каталогов
- [ ] GitHub Release не публикуется до ~v0.9.x; v0.2.1 Release удалён/отозван, README/CHANGELOG/RELEASING согласованы
- [x] v0.2.0 остаётся отозванным; ссылки на него не выдаются за SDK-релиз

## Ручной чек-лист приёмки

- [x] Скачать release assets с GitHub, распаковать в чистые каталоги (до отзыва Release)
- [x] Проверить `sha256sum -c checksums.sha256` (локальный payload и скачанные assets)
- [x] Собрать hello_triangle из SDK; запустить UI client (clean consumer + ui_e2e)
- [x] Создать проект, импортировать `cube.gltf`/`texture_0.png`, Build PC, Run
- [x] Проверить UI package на Windows/Linux/macOS (или доступных host) — CI matrix S35–36 + Linux E2E
- [x] Проверить, что BIOS/ROM не попали в release (payload negative-tests)

## Риски

- **Сборка UI/SDK в release workflow тяжёлая.** Митигация: использовать CI artifacts с commit’а, не пересобирать всё в одном job.
- **Checksums/манифест расходятся с assets.** Митигация: генерировать checksums после упаковки, проверять на чистой распаковке.
- **UI нестабилен на одном host.** Митигация: matrix smoke, known limitations в release notes; не блокировать другие hosts.
- **Соблазн опубликовать demo-архив как релиз.** Митигация: gate v2 hard-fail без `sdk/`+`ui/`; README/RELEASING явно запрещают.
