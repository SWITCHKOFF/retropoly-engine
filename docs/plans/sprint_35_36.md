# Спринт 35–36 — UI client MVP: project/target/import/build/run (недели 69–72)

> Детальный план спринта. Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.2).
> Источник требований: [`RELEASING.md`](../RELEASING.md) — UI client обязателен
> для публичного релиза.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-13):** MVP реализован: `tools/ui_client/` (core+GUI),
> `.rpeproject`, SDK discovery, asset import, build/run, diagnostics,
> version panel, unit-тесты (47), headless smoke и clean E2E PC,
> `ui-client.yml`, `scripts/package_ui.py`, `docs/UI_CLIENT.md`, ADR-012.
> Ручной gate пройден: PS1 (DuckStation, `ui_gate_ps1.png`) и Saturn
> (Mednafen 1.32.1, `ui_gate_saturn.png`) запускают `.cue`, собранный из
> UI-проекта. Первый зелёный CI matrix (push `79ce6f4`): `build-pc`
> 43491683505, `build-ps1` 43491683796, `build-saturn` 43491683545,
> `tests` 43491683683, `sdk-package` 43491683616, `ui-client` 43491683644.
> Осталось: release gate v2 и публикация — S37–38.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 35–36 |
| Недели | 69–72 — 14.12.2026 – 10.01.2027 |
| Веха | v0.2.1 — первый валидный SDK+UI релиз (часть 2: UI client) |
| Цель | Рабочий UI client MVP: создать/открыть проект, выбрать target, импортировать ассеты, собрать и запустить через SDK |
| Входные критерии | S33–34: SDK install/export + consumer smoke + `sdk_manifest.json` ([`sprint_33_34.md`](sprint_33_34.md)) |
| На критическом пути | Да (release gate v2) |
| Выходные артефакты | `tools/ui_client/`, `retro-engine-ui-<ver>-<os>-<arch>`, `docs/UI_CLIENT.md`, UI smoke/E2E, CI artifact |
| Следующий спринт | [`sprint_37_38.md`](sprint_37_38.md) — release gate v2 + v0.2.1 |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 40 | 60 | 85 | 60.8 | 7.5 | 75.8 | 40–60 ч | ⚠ UI MVP плотный; [M]-фичи (recent projects, preview) режем первыми |

## Задачи

**Неделя 69 — архитектура UI и project model:**

- [x] [B] ADR-012: toolkit/архитектура UI client. Базовый вариант — SDL3 + Dear ImGui (MIT), pinned в `VERSION.txt`/`LEGAL.md`; альтернатива — native toolkit, если ADR обоснует
- [x] [B] `tools/ui_client/` — отдельный target `retro_engine_ui`, пресеты `ui-debug`/`ui-release`; не линкует demo-код
- [x] [B] Project model `.rpeproject` (JSON): name, engine_version, sdk_path, target/platform, assets, build_dir, run_target, emulator_path
- [x] [B] Валидация проекта: неизвестные поля/пути/версии — детерминированная ошибка, без краша; unit-тесты
- [x] [B] Welcome screen: New Project (template), Open Project, Recent Projects (список хранится в user-config, не в репо)
- [x] [B] Settings: SDK path/auto-detect, CMake/Ninja, emulator path (DuckStation/Mednafen), тема/язык — минимально
- [x] [B] Project wizard: имя/путь/target; создание структуры `assets/`, `src/`, `build/`, `.rpeproject`

**Неделя 70 — targets, assets, build/run:**

- [x] [B] Target/platform selection из `sdk_manifest.json`: PC/PS1/Saturn; недоступный SDK — disabled с объяснением
- [x] [B] Asset import: `.gltf`/`.glb`/`.png`/`.jpg`; вызов установленного `asset_converter`; показ результата `.mesh`/`.tex`, ошибок и размеров
- [x] [B] Asset list/preview: имя, тип, размеры, texture_id/slot; удаление/повторный импорт
- [x] [B] Build: запуск CMake preset/SDK в отдельном процессе, streaming stdout/stderr в консоль UI, отмена, exit code
- [x] [B] Run: запуск PC executable; для PS1/Saturn — запуск `.cue` в настроенном эмуляторе; путь и команда видны в логе
- [x] [B] Diagnostics: отсутствует SDK/CMake/Ninja/эмулятор — понятное сообщение и ссылка на docs/BUILDING
- [x] [H] Version/compatibility panel: версия SDK, engine commit, target, ABI/compiler; предупреждение при несовпадении
- [x] [M] Новый проект из шаблона `hello_triangle`/`empty`; копирование assets и README

**Неделя 71 — качество, упаковка, документация:**

- [x] [B] UI unit-тесты project model/path validation/command builder (без GUI)
- [x] [B] Headless UI smoke под `xvfb-run`: старт, создание проекта, импорт ассета, сборка PC hello_triangle
- [x] [B] CI workflow `ui-client.yml`: matrix ubuntu/windows/macos, build + tests + artifact
- [x] [B] Packaging: Windows `.zip`/`.exe`, Linux `.AppImage`/`.tar.gz`, macOS `.dmg`/`.zip`; версия и checksums
- [x] [B] `docs/UI_CLIENT.md`: установка, создание проекта, target/asset/build/run, troubleshooting
- [x] [B] README: скриншот UI, быстрый старт «SDK + UI», ссылка на `docs/RELEASING.md`
- [x] [H] Скриншоты: welcome, project, asset import, build log, run
- [x] [M] Локализация RU/EN (только строки UI; без внешних сервисов)

**Неделя 72 — E2E и приёмка:**

- [x] [B] E2E: чистый SDK-архив + UI package → New Project → import `cube.gltf`/`texture.png` → Build PC → Run
- [x] [B] E2E: target PS1/Saturn — сборка из UI (`hello_triangle_ps1`/`hello_triangle_saturn`), `.cue` из UI-проекта запущен в DuckStation/Mednafen (ручной gate, see `screenshots/ui_gate_ps*.png`)
- [x] [B] Проверка отсутствия секретов/BIOS/ROM/build-каталогов в UI package
- [x] [B] `sdk_manifest.json`/UI version согласованы; UI отказывается работать с несовместимым SDK
- [x] [B] Артефакты UI прикреплены к CI; release gate может их проверить

## Критерии приёмки спринта 35–36

- [x] UI client запускается на Windows/Linux/macOS — CI matrix `ui-client` 43491683644 без admin-прав
- [x] New Project создаёт валидный `.rpeproject`, Open Project его читает
- [x] Импорт `.gltf`/`.png` вызывает SDK asset_converter и показывает результат
- [x] Build собирает hello_triangle через установленный SDK; Run запускает PC target
- [x] PS1/Saturn target выбираются только при наличии соответствующего SDK; `.cue` запускается в настроенном эмуляторе
- [x] UI package содержит версию/checksums; нет BIOS/ROM/секретов
- [x] Headless smoke и E2E зелёные в CI (`ui-client` 43491683644)

## Ручной чек-лист приёмки

- [x] Распаковать UI package + SDK в чистые каталоги, пройти New Project → Build → Run
- [x] Импортировать `assets/source/cube.gltf` и `texture_0.png`, проверить `.mesh`/`.tex`
- [x] Переключить target PC→PS1→Saturn; проверить disabled-состояния без SDK
- [x] Закрыть/открыть проект, проверить recent projects и сохранение настроек
- [x] Проверить логи сборки/ошибки при отсутствии CMake/Ninja/эмулятора

## Риски

- **UI toolkit/scope.** Митигация: ADR-012, Dear ImGui baseline, [M]-фичи режем; не делать full IDE.
- **Кросс-платформенная упаковка.** Митигация: CI matrix, AppImage/dmg/zip smoke, относительные пути SDK.
- **Запуск эмуляторов из UI.** Митигация: только настроенный пользователем путь/команда; не вендорить BIOS/эмулятор.
- **Синхронизация версий SDK/UI.** Митигация: `sdk_manifest.json` + semver, UI отказывается при несовместимости.
