# Релизная политика: SDK + UI client

> Статус: **обязательно для всех публичных релизов начиная с v0.2.0**.
> Изменение политики — только через ADR и обновление
> `ci/normative_contracts.yaml` + `scripts/verify_release_contract.py`
> в одном изменении (DEVELOPMENT_PLAN §1, п. 6).

> **RELEASE FREEZE (2026-11-14):** публичные релизы приостановлены до
> **~v0.9.x**. GitHub Release и тег `v0.2.1` удалены/отозваны; версия
> 0.2.1 остаётся development-версией и не считается публичным релизом.
> `release.yml` создаёт только **draft**; публикация — вручную после
> совместной приёмки maintainer + агент. Автоматический public release
> запрещён до отмены freeze. Решение —
> [`adr/ADR-013-release-freeze.md`](adr/ADR-013-release-freeze.md).

## 1. Текущий статус

Публикация **v0.2.0 отозвана**: в GitHub Release были вложены только
исходники, `release_contract_report.json` и архив с demo-образами для
эмуляторов (`pc/`, `ps1/`, `saturn/`). Это **не SDK-релиз движка**:
в нём нет готового SDK и UI-клиента движка.

Тег `v0.2.0` может оставаться в истории git как техническая метка, но
**не является действующим публичным релизом**. Не ссылаться на него как
на «релиз SDK» и не публиковать анонс до прохождения нового release gate.

**v0.2.1 (S33–38):** GitHub Release и тег `v0.2.1` удалены/отозваны
2026-11-14. SDK+UI payload и release gate v2 реализованы и проходят, но
публикация **отложена до ~v0.9.x**: перед выпуском maintainer сам
тестирует релиз совместно с агентом. Статус в
[`ci/normative_contracts.yaml`](../ci/normative_contracts.yaml):
`v0_2_1_release_status: withdrawn`, `release_freeze.active: true`.
Версия 0.2.1 остаётся development-версией; не ссылаться на неё как на
публичный релиз. v0.2.0 также остаётся отозванным.

## 2. Что считается публичным релизом

Публичный релиз RetroPolyEngine — это **готовый SDK + UI-клиент**, а не
набор demo-образов и не source-only архив.

| Компонент | Обязателен | Содержимое |
|---|---|---|
| **SDK** | Да | публичные заголовки, библиотеки, CMake package, toolchain-скрипты, примеры, лицензии, версия |
| **UI client** | Да | editor/launcher движка: создание проекта, импорт ассетов, сборка/запуск target'ов |
| Demo-образы | Нет (опционально) | `pc/`, `ps1/`, `saturn/` — только как примеры, не вместо SDK |
| Исходники | Нет (опционально) | отдельный source archive или ссылка на git-тег |
| Release report | Да | `release_contract_report.json` от валидатора |

### 2.1. Обязательный payload SDK

```
sdk/
  include/retro_engine/...          # стабильные публичные заголовки
  lib/
    pc/<arch>/...                   # static/shared libs, debug+release
    ps1/...                         # кросс-библиотеки (если входят в релиз)
    saturn/...                      # SH-2 библиотеки (если входят в релиз)
  lib/cmake/RetroPolyEngine/        # find_package(RetroPolyEngine)
    RetroPolyEngineConfig.cmake
    RetroPolyEngineTargets.cmake
  cmake/                            # ps1/saturn toolchain wrappers
  tools/                            # asset_converter, утилиты сборки
  examples/                         # hello_triangle, rotating_cube (исходники)
  templates/                        # шаблон нового проекта
  VERSION.txt
  sdk_manifest.json                 # версия/commit/targets/checksums
  LEGAL.md
  checksums.sha256
```

Требования к SDK:

- `find_package(RetroPolyEngine CONFIG REQUIRED)` работает из распакованного
  архива без ручного `include`/`link` путей;
- public headers не тянут SDL3/OpenGL/PSn00bSDK/Jo Engine в публичный API;
- версия и commit движка зафиксированы в `VERSION.txt`;
- лицензии и notices всех вендоренных компонентов приложены;
- примеры собираются против **установленного SDK**, а не против дерева
  исходников;
- checksums проверяются release gate.

### 2.2. Обязательный payload UI client

```
ui/
  retro-engine-ui[.exe]           # SDL3 + Dear ImGui, host-only
  assets/DejaVuSans.ttf
  samples/cube.gltf
  samples/texture.png
  README-ui.md
  VERSION.txt
  LEGAL.md
  ui_manifest.json                # version/sdk_series/components/files
  checksums.sha256
```

Архивы: `retro-engine-ui-<version>-<os>-<arch>.{tar.gz,zip}` (+ macOS
`.dmg` при наличии `hdiutil`) с sidecar `.sha256`; Linux AppImage —
опционально при наличии `appimagetool`.

UI client должен уметь как минимум:

- создать/открыть проект;
- выбрать target и платформу (PC/PS1/Saturn);
- импортировать `.gltf`/`.png` и запустить asset_converter;
- собрать проект через SDK (CMake preset);
- запустить demo/эмулятор по документированной команде;
- показать версию SDK и совместимость.

Если UI client ещё не реализован, публичный релиз **блокируется**; demo
archive публикуется только как technical preview с явной пометкой.

### 2.3. Комбинированный payload релиза

`scripts/build_release_bundle.py` собирает архив
`retro-engine-v<version>.tar.gz` (рядом `.sha256`) со структурой:

```
retro-engine-v0.2.1/
  sdk/                          # распакованный PC SDK (headers/libs/config)
  ui/                           # распакованный UI client для host-платформы
  demos/                        # OPTIONAL demo-образы pc/ps1/saturn
  checksums.sha256              # все файлы payload (кроме себя)
  release_contract_report.json  # полный отчёт release gate v2
  README.md
  LICENSE
  LEGAL.md
```

- `sdk/` и `ui/` обязательны; без них сборка payload завершается ошибкой.
- `demos/` — только optional примеры; demo-образ не может подменить SDK/UI.
- `checksums.sha256` покрывает все файлы payload, включая `sdk/` и `ui/`;
  `sdk/checksums.sha256` и `ui/checksums.sha256` проверяются отдельно.
- Запрещены BIOS/ROM, приватные ключи, `.env`, `.git`, build-каталоги
  (`build/`, `CMakeFiles/`), `secrets/`, `roms/`.
- Cross-platform SDK/UI-архивы (`retro-engine-sdk-*`,
  `retro-engine-ui-*`) публикуются отдельными assets; комбинированный
  payload содержит native-host сборку.

Сборка:

```sh
python3 scripts/build_release_bundle.py \
  --sdk-archive dist/retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz \
  --ui-archive dist/retro-engine-ui-0.2.1-linux-x86_64.tar.gz \
  --report release_contract_report.json \
  --demos-dir dist/demos        # optional
```

или обёртка `./scripts/create_release_archive.sh v0.2.1`.

## 3. Release gate v2 (обязательные проверки)

До публикации (draft → public) должны быть зелёными:

1. `build-pc`, `build-ps1`, `build-saturn`, `tests`, `sdk-package`,
   `ui-client` — success на release-коммите.
2. `verify_release_contract.py --w1-seconds 60` — `all_ok: true`, включая
   SDK/UI-проверки и UI headless smoke.
3. SDK-архив распакован в чистый каталог; пример собирается через
   `find_package`.
4. UI client запускается на поддерживаемых ОС и создаёт проект; clean
   E2E `scripts/ui_e2e.py` проходит на распакованных SDK+UI.
5. Комбинированный payload собран `scripts/build_release_bundle.py`;
   `verify_release_contract.py --payload-only` — `all_ok: true`.
6. Negative-tests `scripts/release_gate_negative_tests.py` подтверждают
   fail без `sdk/`, `ui/`, checksums/report, при BIOS/ROM/секретах и
   build-каталогах.
7. В payload нет BIOS/ROM, приватных ключей и build-каталогов.
8. `checksums.sha256` проверяется; `release_contract_report.json` и
   `payload_verification_report.json` приложены.
9. Release notes явно называют версию SDK, UI и поддерживаемые платформы.
10. **Ручная приёмка maintainer + агент** на скачанных draft-assets:
    распаковка, checksums, SDK consumer, UI New Project → import → Build →
    Run, PS1/Saturn `.cue` (если применимо). Только после этого draft
    публикуется вручную.

Команды gate на release-коммите:

```sh
# 1. Полный контракт (нужны собранные пресеты и dist-архивы)
xvfb-run -a python3 scripts/verify_release_contract.py --w1-seconds 60 \
  --skip-payload \
  --sdk-archive dist/retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz \
  --ui-archive dist/retro-engine-ui-0.2.1-linux-x86_64.tar.gz

# 2. Payload SDK+UI и checksums
python3 scripts/build_release_bundle.py \
  --sdk-archive dist/retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz \
  --ui-archive dist/retro-engine-ui-0.2.1-linux-x86_64.tar.gz \
  --report release_contract_report.json --demos-dir dist/demos
python3 scripts/verify_release_contract.py --payload-only \
  --output payload_verification_report.json

# 3. Negative-tests
python3 scripts/release_gate_negative_tests.py
```

Пока пункты 2–10 не выполнены, draft GitHub Release **не публикуется**,
тег не анонсируется, а в README/CHANGELOG указывается статус «release
withdrawn / SDK+UI pending».

## 4. Текущие пробелы и следующий шаг

| Пробел | Статус | Блокирует релиз |
|---|---|---|
| SDK package (install/export/CMake config) | реализован в S33–34 (`docs/SDK.md`, `sdk_package`) | Нет (gate проходит) |
| UI client (editor/launcher) | MVP реализован в S35–36 (`tools/ui_client/`, `retro-engine-ui-*`, smoke/E2E) | Нет (gate проходит) |
| `verify_release_contract.py` SDK/UI/payload asserts | реализован в S37–38 (`sdk_*`, `ui_*`, `release_payload`) | Нет |
| Release payload SDK+UI | реализован в S37–38 (`scripts/build_release_bundle.py`, `create_release_archive.sh` v2) | Нет |
| Negative-tests gate | реализован в S37–38 (`scripts/release_gate_negative_tests.py`) | Нет |
| Demo-образы PC/PS1/Saturn | есть, только optional `demos/` | Нет |
| Публикация v0.2.1 | **GitHub Release удалён/отозван; release freeze до ~v0.9.x** | Да (release freeze) |
| Ручная приёмка maintainer + агент | не выполнена | Да (перед public release) |

Поддерживаемые хосты/таргеты v0.2.1:

| Компонент | Хосты | Таргеты |
|---|---|---|
| SDK | Linux x86_64, Windows x86_64, macOS arm64 | PC, PS1, Saturn |
| UI client | Linux x86_64, Windows x86_64, macOS arm64 | host UI; сборка проектов PC/PS1/Saturn |

macOS x86_64 собирается из исходников, но не входит в assets v0.2.1.

Следующий этап — **разработка до ~v0.9.x** при активном release freeze:

- [`plans/sprint_33_34.md`](plans/sprint_33_34.md) — SDK install/export,
  CMake package, consumer smoke (готово);
- [`plans/sprint_35_36.md`](plans/sprint_35_36.md) — UI client MVP
  (project/target/import/build/run) (готово);
- [`plans/sprint_37_38.md`](plans/sprint_37_38.md) — release gate v2,
  payload `sdk/`+`ui/` (готово; публикация отложена).

## 5. Release checklist (draft; release freeze)

- [x] SDK-архивы PC/PS1/Saturn, `sdk_manifest.json`, checksums
- [x] UI-пакеты Linux/Windows/macOS, `ui_manifest.json`, checksums
- [x] `verify_release_contract.py` SDK/UI/payload asserts
- [x] `release_payload` assert и layout `sdk/` + `ui/`
- [x] `build_release_bundle.py` / `create_release_archive.sh` v2
- [x] Negative-tests: missing SDK/UI/checksum, BIOS/ROM, build-каталоги
- [x] `release.yml`: reusable SDK/UI workflows → gate → **draft** only
- [x] Локальный полный gate 2026-11-14 (`--w1-seconds 60`, `all_ok: true`,
      UI headless smoke, consumer PC/PS1/Saturn)
- [x] Clean-room consumer PC и clean-room UI E2E на локальных ассетах
- [x] Remote CI на коммите `3e816d1` — 12/12 green
- [x] Тег `v0.2.1` и GitHub Release — созданы, затем **удалены/отозваны**;
      версия 0.2.1 остаётся development-версией
- [x] GitHub Release v0.2.1 удалён/отозван; assets проверены до отзыва
- [ ] Публикация отложена до **~v0.9.x** (release freeze)
- [ ] Ручная приёмка maintainer + агент на draft-assets
- [ ] После ручной приёмки: вручную опубликовать draft и снять freeze
      (статус `released` — только для будущего v0.9.x)

v0.2.1 **не является** публичным релизом; v0.2.0 также остаётся
отозванным. N64 (v0.3.0) и дальнейшая разработка продолжаются без
публикации релизов до ~v0.9.x.
