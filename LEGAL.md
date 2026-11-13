# LEGAL.md — Лицензии вендоренных зависимостей

Спринт 0 §3.6 (docs/plans/sprint_00.md). Полная фиксация — VERSION.txt (тег/commit, SHA-256).

| Компонент | Путь | Лицензия | Примечание |
|---|---|---|---|
| SDL3 | `src/third_party/sdl3` | zlib | Подключение ТОЛЬКО через `add_subdirectory` (vcpkg — не-канонический fallback) |
| GoogleTest | `src/third_party/googletest` | BSD-3-Clause | Canonical; FetchContent запрещён |
| PSn00bSDK (core) | `src/third_party/psn00bsdk` | MPL-2.0 | Pinned v0.24 (`06e65be`); обновление — отдельная задача с ADR |
| mkpsxiso / dumpsxiso | `psn00bsdk/tools/mkpsxiso` | GPL-2.0-or-later | Сохранять notices; вложенный submodule (не инициализируется) |
| cgltf | `src/third_party/cgltf/cgltf.h` | MIT | Pinned v1.15 |
| stb (image, image_write) | `src/third_party/stb/` | PD/MIT (dual) | Pinned archive `003501f6` |
| glad | `src/third_party/glad/` | MIT | Сгенерирован glad2: OpenGL Core 3.3 + loader, extensions=all |
| Dear ImGui | `src/third_party/imgui` | MIT | Pinned v1.93.0 (`c1aaac65`); UI client (S35–36), SDL3/OpenGL3 backends |
| DejaVuSans | `tools/ui_client/assets/DejaVuSans.ttf` | Bitstream Vera Fonts license (см. below) | Шрифт UI client для RU/EN; распространяется вместе с `LEGAL.md` |
| Jo Engine | `JO_ENGINE_ROOT` (env-install) | MIT | Pinned v2026.2 (`5e99944`); Saturn backend, не вендорится |
| sh-none-elf-gcc | bundled в Jo Engine `Compiler/LINUX` | GPL-3.0-or-later + GCC Runtime Library Exception; newlib BSD-style | Только сборка; скомпилированный код движка остаётся MIT |
| Mednafen | env-install (ручная приёмка) | GPL-2.0-or-later | В релизы/CI/репозиторий не входит; BIOS не вендорится |

Правило распространения: проект распространяется под MIT (см. LICENSE); при включении компонентов под GPL (mkpsxiso, GCC toolchain при распространении, Mednafen при распространении) в распространяемые пакеты соблюдаются их лицензии и обязательные attributions. BIOS Sega Saturn в проект не входит и не распространяется.

## SDK-пакеты (S33–34)

- `retro-engine-sdk-*-pc-*`: содержит static SDL3 (zlib) и glad (MIT) —
  их лицензии/уведомления сохраняются в `include/SDL3`, `lib/cmake/SDL3` и
  `LEGAL.md`; исходники SDL3 доступны в upstream-репозитории и поставляются
  без изменений лицензионных файлов.
- `retro-engine-sdk-*-ps1-*`: библиотеки движка под MIT; PSn00bSDK
  (MPL-2.0) и mkpsxiso (GPL-2.0-or-later) **не** входят в архив — SDK
  требует внешней установки pinned PSn00bSDK, пользователь принимает его
  лицензии отдельно.
- `retro-engine-sdk-*-saturn-*`: библиотека Jo Engine v2026.2 (MIT) и её
  публичные заголовки входят в пакет с сохранением copyright/notice;
  bundled `sh-none-elf-gcc` (GPL-3.0-or-later + GCC Runtime Exception,
  newlib BSD-style) в SDK **не** входит — используется внешний
  `JO_ENGINE_ROOT`.
- BIOS/ROM/ключи и build-каталоги запрещены в любом SDK-архиве; проверка —
  `scripts/package_sdk.py` + `scripts/verify_release_contract.py`.

## UI-пакет (S35–36)

- `retro-engine-ui-*-<os>-<arch>` содержит Dear ImGui v1.93.0 (MIT) и
  шрифт DejaVuSans (Bitstream Vera Fonts license). Тексты лицензий
  сохраняются в `LEGAL.md`; исходники ImGui доступны в upstream.
- DejaVu Fonts license: Copyright (c) 2003 by Bitstream, Inc. All Rights
  Reserved. DejaVu changes are in public domain. Permission is hereby
  granted, free of charge, to any person obtaining a copy of the fonts and
  associated documentation files, to use, copy, merge, publish, distribute,
  and/or sell copies, subject to the conditions of the Bitstream Vera
  license (copyright notices preserved; fonts not sold by themselves;
  modified versions not use the reserved names). Полный текст —
  `assets/DejaVu-LICENSE.txt` в UI-пакете.
- BIOS/ROM/секреты/build-каталоги не входят в UI-пакет; проверка —
  `scripts/package_ui.py`, `scripts/ui_e2e.py`,
  `scripts/verify_release_contract.py`.
