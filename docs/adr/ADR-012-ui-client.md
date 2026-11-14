# ADR-012 — UI client: SDL3 + Dear ImGui, project model и границы UI/SDK

- **Статус:** Accepted (2026-11-13)
- **Спринт:** S35–36 (недели 69–72), веха v0.2.1
- **Связанные документы:** [`../plans/sprint_35_36.md`](../plans/sprint_35_36.md),
  [`../UI_CLIENT.md`](../UI_CLIENT.md), [`ADR-011-public-sdk-api.md`](ADR-011-public-sdk-api.md),
  [`../RELEASING.md`](../RELEASING.md)

## 1. Контекст

Публичный релиз v0.2.1 обязан содержать не только SDK, но и UI client
(project/target/import/build/run). UI должен:

- запускаться на Windows/Linux/macOS без admin-прав и без установки
  системного GUI-фреймворка;
- работать с установленным SDK через `sdk_manifest.json` и `asset_converter`,
  не линкуя demo-код и не требуя дерева исходников движка;
- быть достаточно компактным, чтобы уложиться в S35–36 (≈60 ч) и не
  превратиться в полноценную IDE;
- иметь тестируемое без GUI ядро (project model, path validation, command
  builder, process runner).

Рассматривались:

| Вариант | Плюсы | Минусы |
|---|---|---|
| **SDL3 + Dear ImGui** | уже вендорен SDL3; ImGui MIT; один бинарник; простой immediate-mode UI; кроссплатформенность; легко собрать в CI | нет нативных виджетов/file dialog (SDL3 даёт диалоги); UI требует ручной компоновки |
| Qt | нативные виджеты, дизайнер | LGPL/commercial, тяжёлый runtime, deployment (windeployqt/macdeployqt), выходит за scope |
| GTK/wxWidgets | нативные | системные зависимости/версии, сложная упаковка на 3 ОС |
| Electron/web UI | быстрый UI | десятки MB runtime, Node-стек, лишняя поверхность атаки; противоречит «retro/minimal» |

## 2. Решение

1. **Toolkit:** SDL3 `release-3.4.18` + Dear ImGui `v1.93.0` (MIT,
   commit `c1aaac65`), backends `imgui_impl_sdl3` + `imgui_impl_opengl3`.
   Пин — `VERSION.txt` `[imgui]`, лицензия — `LEGAL.md`.
2. **Отдельный target:** `retro_engine_ui` (host-only, пресеты
   `ui-debug`/`ui-release`). UI **не линкует** `retro_engine`, примеры или
   игровые системы; он вызывает SDK-инструменты и CMake как внешние
   процессы. Единственная связь с движком — публичный SDK-контракт
   (manifest, `find_package`, `asset_converter`, toolchain-обёртки).
3. **Разделение core/GUI:** `tools/ui_client/core/` — статическая
   библиотека без SDL/ImGui: JSON, `.rpeproject`, settings, SDK discovery,
   command builder, process runner, asset import, diagnostics, i18n.
   `tools/ui_client/gui/` — тонкий слой ImGui. Unit-тесты работают без
   дисплея; headless smoke запускает реальный GUI под `xvfb-run`.
4. **Project model:** JSON-файл `.rpeproject` (схема 1) в корне проекта:
   `name`, `engine_version`, `sdk_path`, `target`, `platform`, `assets[]`,
   `build_dir`, `run_target`, `emulator_path`. Неизвестные поля,
   небезопасные пути (`..`, абсолютные пути для внутренних файлов) и
   несовместимые версии — детерминированная ошибка, не краш.
5. **SDK discovery:** `sdk_manifest.json` (schema 1) — единственный источник
   истины о версии/платформе/компонентах. UI ищет SDK в явных настройках,
   `RPE_SDK_*`, `sdk_root` и `sdk_root/<platform>`; target недоступен, если
   SDK/компонент/toolchain env отсутствует.
6. **Совместимость:** UI 0.2.x работает только с SDK той же major/minor
   серии; patch-отличие — warning. `sdk_series` фиксируется в UI package
   manifest; release gate проверяет совпадение версий.
7. **Процессы:** CMake/Ninja, `asset_converter`, target и эмулятор
   запускаются в отдельных процессах со streaming stdout/stderr, отменой и
   exit code. UI не исполняет shell-строки и не подставляет пути в shell.
8. **Конфигурация:** settings/recent projects хранятся вне репозитория
   (`~/.config/retro-engine-ui`, `%APPDATA%/RetroPolyEngine`,
   `~/Library/Application Support/RetroPolyEngine`); `RPE_UI_CONFIG_DIR`
   переопределяет каталог для CI/smoke.
9. **Локализация:** только строки UI, встроенная таблица RU/EN; без внешних
   сервисов и runtime-загрузки переводов.
10. **Упаковка:** `scripts/package_ui.py` — детерминированный пакет
    `ui/` с `ui_manifest.json`, `checksums.sha256`, шрифтом DejaVuSans
    (Bitstream Vera license), samples `cube.gltf`/`texture.png`, README,
    LEGAL. BIOS/ROM/секреты/build-каталоги запрещены и проверяются.

## 3. Последствия

- UI package не зависит от системного Qt/GTK; SDL3 собирается статически,
  системным остаётся OpenGL (как и у PC SDK).
- Immediate-mode UI проще в поддержке, но требует собственных table/tab/
  dialog-композиций; file/folder dialogs делегированы SDL3.
- Тестируемость: core-тесты не требуют дисплея; GUI smoke — `xvfb-run` на
  Linux; Windows/macOS CI собирает и прогоняет core-тесты, E2E — Linux.
- Появление новых target'ов (N64 и далее) не меняет UI core: достаточно
  расширить `Platform`, manifest и command builder.
- Ограничение MVP: переключение target у уже созданного проекта меняет
  платформу/CMake-цель и каталог сборки, но не портирует исходники проекта;
  для PS1/Saturn рекомендуется создавать проект сразу под нужную платформу.
  Это документировано в `docs/UI_CLIENT.md`.
