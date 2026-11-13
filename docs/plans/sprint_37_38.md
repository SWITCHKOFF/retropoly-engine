# Спринт 37–38 — Release gate v2 и публикация v0.2.1 (недели 73–76)

> Детальный план спринта. Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.2).
> Источник требований: [`RELEASING.md`](../RELEASING.md) — SDK + UI client,
> checksums, consumer smoke.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-13):** план не начат; старт после S35–36.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 37–38 |
| Недели | 73–76 — 11.01.2027 – 07.02.2027 |
| Веха | v0.2.1 — первый действующий публичный релиз (SDK + UI) |
| Цель | Собрать SDK+UI payload, включить release gate v2, опубликовать v0.2.1 |
| Входные критерии | S33–34 (SDK) и S35–36 (UI) закрыты; v0.2.0 отозван |
| На критическом пути | Да (release gate v2) |
| Выходные артефакты | `sdk/` + `ui/` + `checksums.sha256` + `release_contract_report.json`; тег `v0.2.1`; GitHub Release |
| Следующий спринт | v0.3.0 N64 (S39+) после приёмки v0.2.1 |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 30 | 50 | 70 | 50.0 | 6.7 | 63.3 | 40–60 ч | ⚠ Интеграция/упаковка; при просадке переносим анонс, но не SDK/UI-гейт |

## Задачи

**Неделя 73 — release payload и валидатор:**

- [ ] [B] `scripts/build_release_bundle.py` (или CMake target): собирает `dist/retro-engine-v<ver>/` из SDK-архивов, UI-пакетов и optional demo-образов
- [ ] [B] Layout: `sdk/`, `ui/`, `checksums.sha256`, `release_contract_report.json`, `README.md`, `LICENSE`, `LEGAL.md`; demo-образы — только `demos/` и явно optional
- [ ] [B] `verify_release_contract.py`: asserts SDK manifest/headers/libs/CMake package, UI package/version, checksums, clean consumer smoke, UI headless smoke
- [ ] [B] `ci/normative_contracts.yaml`: зафиксировать `release.payload = sdk+ui`, версии SDK/UI, supported hosts/targets, обязательные checksums; убрать `v0_2_0_release_withdrawn` после v0.2.1
- [ ] [B] `release.yml`: build/consume SDK+UI artifacts, запуск consumer/UI smoke на чистой распаковке, публикация только при `all_ok: true`
- [ ] [B] `create_release_archive.sh` v2: содержит `sdk/` + `ui/`, а demo-образы не могут подменять SDK
- [ ] [B] Проверка отсутствия BIOS/ROM/ключей/build-каталогов в payload; отдельный negative-test

**Неделя 74 — документация и версия:**

- [ ] [B] `docs/RELEASING.md`: обновить статус — v0.2.0 withdrawn, v0.2.1 release checklist; таблица «что считается релизом»
- [ ] [B] `docs/SDK.md`, `docs/UI_CLIENT.md`: версия v0.2.1, установка, quickstart, supported hosts/targets
- [ ] [B] README: download/quickstart SDK+UI, скриншоты, supported matrix; убрать demo-only как основной путь
- [ ] [B] CHANGELOG `[0.2.1]`: SDK packaging, UI client, release gate v2, v0.2.0 withdrawn
- [ ] [B] `VERSION.txt`/`sdk_manifest.json`/UI version = `0.2.1`; commit/ABI/checklists
- [ ] [B] Обновить `docs/DEVELOPMENT_PLAN.md` roadmap: v0.2.1 release, N64 S39+
- [ ] [H] Release notes: что внутри SDK/UI, supported platforms, known limitations, как проверить checksums

**Неделя 75 — полный release gate:**

- [ ] [B] `build-pc`, `build-ps1`, `build-saturn`, `tests`, `sdk-package`, `ui-client` — success на release-коммите
- [ ] [B] `verify_release_contract.py --w1-seconds 60` — `all_ok: true`, включая SDK/UI asserts
- [ ] [B] Clean-room: распаковать `retro-engine-v0.2.1` в пустой каталог; consumer hello_triangle собирается через `find_package`
- [ ] [B] Clean-room UI: создать проект, импортировать ассет, собрать PC target, запустить
- [ ] [B] PS1/Saturn SDK examples собираются из пакета; ручной запуск `.cue` в pinned эмуляторе
- [ ] [B] Negative-tests: отсутствующий SDK/UI/checksum → gate fail; BIOS в payload → gate fail
- [ ] [B] Подготовить `v0.2.1` release branch/tag; v0.2.0 tag оставить историческим

**Неделя 76 — публикация:**

- [ ] [B] Тег `v0.2.1` (annotated) только после зелёного gate
- [ ] [B] GitHub Release: `retro-engine-v0.2.1.tar.gz`, `retro-engine-sdk-*.tar.gz/zip`, `retro-engine-ui-*`, `checksums.sha256`, `release_contract_report.json`
- [ ] [B] Проверить скачанные assets: checksums, размеры, отсутствие BIOS/ROM
- [ ] [B] Обновить README/CHANGELOG/RELEASING статус «v0.2.1 released»
- [ ] [B] Анонс (по возможности) с честной формулировкой: SDK+UI, emulator validation
- [ ] [M] Собрать метрики релиза (downloads/issues) через 1–2 недели

## Критерии приёмки спринта 37–38

- [ ] Payload содержит **SDK** (headers/libs/CMake package/toolchain/examples) и **UI client**; demo-образы только optional
- [ ] `verify_release_contract.py` падает без SDK/UI и проходит на v0.2.1
- [ ] Clean consumer строит hello_triangle через `find_package`; UI создаёт проект/собирает/запускает
- [ ] Checksums проверяются; нет BIOS/ROM/секретов/build-каталогов
- [ ] GitHub Release v0.2.1 опубликован; README/CHANGELOG/RELEASING согласованы
- [ ] v0.2.0 остаётся отозванным; ссылки на него не выдаются за SDK-релиз

## Ручной чек-лист приёмки

- [ ] Скачать release assets с GitHub, распаковать в чистые каталоги
- [ ] Проверить `sha256sum -c checksums.sha256`
- [ ] Собрать hello_triangle из SDK; запустить UI client
- [ ] Создать проект, импортировать `cube.gltf`/`texture_0.png`, Build PC, Run
- [ ] Проверить UI package на Windows/Linux/macOS (или доступных host)
- [ ] Проверить, что BIOS/ROM не попали в release

## Риски

- **Сборка UI/SDK в release workflow тяжёлая.** Митигация: использовать CI artifacts с commit’а, не пересобирать всё в одном job.
- **Checksums/манифест расходятся с assets.** Митигация: генерировать checksums после упаковки, проверять на чистой распаковке.
- **UI нестабилен на одном host.** Митигация: matrix smoke, known limitations в release notes; не блокировать другие hosts.
- **Соблазн опубликовать demo-архив как релиз.** Митигация: gate v2 hard-fail без `sdk/`+`ui/`; README/RELEASING явно запрещают.
