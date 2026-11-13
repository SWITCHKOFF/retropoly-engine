# Релизная политика: SDK + UI client

> Статус: **обязательно для всех публичных релизов начиная с v0.2.0**.
> Изменение политики — только через ADR и обновление
> `ci/normative_contracts.yaml` + `scripts/verify_release_contract.py`
> в одном изменении (DEVELOPMENT_PLAN §1, п. 6).

## 1. Текущий статус v0.2.0

Публикация **v0.2.0 отозвана**: в GitHub Release были вложены только
исходники, `release_contract_report.json` и архив с demo-образами для
эмуляторов (`pc/`, `ps1/`, `saturn/`). Это **не SDK-релиз движка**:
в нём нет готового SDK и UI-клиента движка.

Тег `v0.2.0` может оставаться в истории git как техническая метка, но
**не является действующим публичным релизом**. Не ссылаться на него как
на «релиз SDK» и не публиковать анонс до прохождения нового release gate.

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
  cmake/RetroPolyEngineConfig.cmake # find_package(RetroPolyEngine)
  cmake/RetroPolyEngineTargets.cmake
  tools/                            # asset_converter, утилиты сборки
  examples/                         # hello_triangle, rotating_cube (исходники)
  templates/                        # шаблон нового проекта
  VERSION.txt
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
  retro-engine-ui-<version>-<os>-<arch>.(exe|AppImage|dmg|zip)
  README-ui.md
```

UI client должен уметь как минимум:

- создать/открыть проект;
- выбрать target и платформу (PC/PS1/Saturn);
- импортировать `.gltf`/`.png` и запустить asset_converter;
- собрать проект через SDK (CMake preset);
- запустить demo/эмулятор по документированной команде;
- показать версию SDK и совместимость.

Если UI client ещё не реализован, публичный релиз **блокируется**; demo
archive публикуется только как technical preview с явной пометкой.

## 3. Release gate v2 (обязательные проверки)

До создания тега и GitHub Release должны быть зелёными:

1. `build-pc`, `build-ps1`, `build-saturn`, `tests` — success на release-коммите.
2. `verify_release_contract.py` — `all_ok: true`, включая SDK/UI-проверки.
3. SDK-архив распакован в чистый каталог; пример собирается через
   `find_package`.
4. UI client запускается на поддерживаемых ОС и создаёт проект.
5. В payload нет BIOS/ROM, приватных ключей и build-каталогов.
6. `checksums.sha256` проверяется; `release_contract_report.json` приложен.
7. Release notes явно называют версию SDK, UI и поддерживаемые платформы.

Пока пункты 2–4 не выполнены, GitHub Release **не создаётся**, тег не
анонсируется, а в README/CHANGELOG указывается статус «release
withdrawn / SDK+UI pending».

## 4. Текущие пробелы и следующий шаг

| Пробел | Статус | Блокирует релиз |
|---|---|---|
| SDK package (install/export/CMake config) | не реализован | Да |
| UI client (editor/launcher) | не реализован | Да |
| `verify_release_contract.py` SDK/UI asserts | не реализованы | Да |
| Release payload SDK+UI | не реализован | Да |
| Demo-образы PC/PS1/Saturn | есть | Нет (optional) |

Следующая работа — отдельный спринт/эпик **«SDK packaging + UI client»**:
install/export CMake targets, SDK archive, UI MVP (project/target/import/build),
release gate v2 и только затем повторная публикация v0.2.x/v0.3.0.
