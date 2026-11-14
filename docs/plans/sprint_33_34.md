# Спринт 33–34 — SDK packaging: install/export и consumer smoke (недели 65–68)

> Детальный план спринта. Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.2).
> Источник требований: [`RELEASING.md`](../RELEASING.md) — публичный релиз
> обязан содержать готовый SDK + UI client.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-13):** S33–34 реализован: `include/retro_engine/`, CMake install/export, `find_package`, SDK-архивы PC/PS1/Saturn, `sdk_manifest.json`/checksums, consumer smoke и docs/SDK.md; release gate v2 (UI) — S35–38.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 33–34 |
| Недели | 65–68 — 16.11.2026 – 13.12.2026 |
| Веха | v0.2.1 — первый валидный SDK+UI релиз (часть 1: SDK) |
| Цель | Превратить дерево движка в устанавливаемый SDK: публичные заголовки, install/export, CMake package, SDK-архив, consumer smoke |
| Входные критерии | S31–32 реализованы; v0.2.0 GitHub Release отозван; `docs/RELEASING.md` действует |
| На критическом пути | Да (release gate v2) |
| Выходные артефакты | `include/retro_engine/`, `RetroPolyEngineConfig.cmake`, `retro-engine-sdk-<ver>-<os>.tar.gz/zip`, `sdk_manifest.json`, consumer smoke, `docs/SDK.md` |
| Следующий спринт | [`sprint_35_36.md`](sprint_35_36.md) — UI client MVP |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 40 | 55 | 75 | 55.8 | 5.8 | 67.5 | 40–60 ч | ⚠ Выше номинала: install/export + три платформы; при просадке переносим PS1/Saturn-пакеты в S35–36 |

## Задачи

**Неделя 65 — публичный API и CMake install/export:**

- [x] [B] ADR-011: границы публичного SDK API — какие заголовки стабильны, что остаётся internal; правило «public headers не тянут SDL3/OpenGL/PSn00bSDK/Jo Engine»
- [x] [B] `include/retro_engine/` — umbrella `retro_engine.h` + публичные `core/`, `hal/`, `assets/`, `game/` заголовки; internal-заголовки не устанавливаются
- [x] [B] CMake: `install(TARGETS retro_engine retro_engine_log retro_engine_hal ... EXPORT RetroPolyEngineTargets)`; namespaced targets `RetroPolyEngine::retro_engine`, `::hal`, `::log`
- [x] [B] `install(DIRECTORY include/retro_engine ...)`; `GNUInstallDirs`; RPATH/relative paths для Linux/macOS
- [x] [B] `RetroPolyEngineConfig.cmake` + `RetroPolyEngineConfigVersion.cmake`; `find_package(RetroPolyEngine CONFIG REQUIRED)` из build-tree и install-tree
- [x] [B] Установка `asset_converter`, `examples/hello_triangle`, `examples/rotating_cube`, `templates/project`; примеры собираются только против установленного SDK
- [x] [B] `VERSION.txt`/`sdk_manifest.json`: версия, git commit, платформы, ABI/compiler, список артефактов и SHA-256
- [x] [B] `scripts/package_sdk.py` (или CMake target `sdk_package`) — детерминированный layout `sdk/`, checksums, без build-каталогов и секретов
- [x] [B] Consumer smoke: `tests/sdk/consumer/` — отдельный CMake-проект, `find_package`, сборка hello_triangle, запуск/`--version`

**Неделя 66 — платформенные пакеты, CI и документация:**

- [x] [B] PC SDK: debug/release libs + headers + examples; Windows/Linux/macOS
- [x] [B] PS1 SDK package: pinned PSn00bSDK v0.24, cross libs, toolchain wrapper, пример hello_triangle PS1; лицензии/notices
- [x] [B] Saturn SDK package: pinned Jo Engine v2026.2, SH-2 libs, preset/toolchain, пример hello_triangle Saturn
- [x] [B] CI job `sdk-package`: build SDK archives на 3 ОС, upload artifacts, consumer smoke на чистой распаковке
- [x] [B] `ci/normative_contracts.yaml`: секция `sdk` (public headers, libs, config, examples, checksums) и release-asserts
- [x] [B] `verify_release_contract.py`: assert SDK-архива, manifest, `find_package` consumer, checksums; без SDK релиз недействителен
- [x] [B] `docs/SDK.md`: установка, `find_package`, targets, платформенные пакеты, пример consumer-проекта
- [x] [B] Обновить `docs/BUILDING.md`, `docs/API.md`, `README.md` (раздел SDK), `LEGAL.md` (redistribution/notices)
- [x] [H] `pkg-config`/CMake presets для SDK-примеров; `sdk/README.md` с быстрым старт-сниппетом
- [x] [M] Сравнить размер/время установки PC/PS1/Saturn SDK; зафиксировать в `docs/benchmarks/sdk_sizes.md`

## Критерии приёмки спринта 33–34

- [x] `cmake --install` / SDK-архив содержит headers, libs, CMake package, converter, examples, LEGAL, checksums
- [x] Чистый consumer-проект собирает hello_triangle через `find_package(RetroPolyEngine)` без доступа к исходникам движка
- [x] `verify_release_contract.py` проверяет SDK и падает при отсутствии/повреждении manifest/checksums
- [x] PC/PS1/Saturn SDK-пакеты собираются в CI; артефакты скачиваются и проверяются
- [x] Public headers не включают third-party/SDK-заголовки; internal-заголовки не установлены
- [x] Документация SDK и consumer-пример воспроизводимы с нуля

## Ручной чек-лист приёмки

- [x] Распаковать SDK в пустой каталог, собрать `examples/hello_triangle` через `find_package`
- [x] Проверить `sdk_manifest.json` и `checksums.sha256`
- [x] Собрать PS1 и Saturn примеры из SDK-пакетов
- [x] Убедиться, что BIOS/ROM/ключи/build-каталоги не попали в архив

## Риски

- **Публичный API «протекает» third-party зависимостями.** Митигация: umbrella + include-аудит, CI-тест «public headers compile standalone».
- **Install/export ломается на Windows/macOS.** Митигация: matrix CI, `GNUInstallDirs`, consumer smoke на каждой ОС.
- **PS1/Saturn SDK-пакеты тянут лицензионные компоненты.** Митигация: pinned notices, LEGAL, только разрешённые к распространению части.
- **Scope creep: SDK = полноценный package manager.** Митигация: v0.2.1 — только headers/libs/config/examples/checksums; dependency manager — post-release.
