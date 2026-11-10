# Юридическая информация — RetroPolyEngine

> Двигатель распространяется под лицензией MIT. Настоящий документ фиксирует
> лицензии вендоренных зависимостей и правила сохранения notices.

## Лицензия проекта

См. [`LICENSE`](../LICENSE).

```
MIT License
Copyright (c) 2026 RetroPolyEngine contributors
```

## Вендоренные зависимости

| Компонент | Версия / Pin | Лицензия | Путь |
|---|---|---|---|
| SDL3 | release-3.4.18 (`829a65d`) | zlib | `src/third_party/sdl3` |
| GoogleTest | v1.18.0 (`063de7e`) | BSD-3-Clause | `src/third_party/googletest` |
| PSn00bSDK | v0.24 (`06e65be`) | MPL-2.0 (core); `mkpsxiso`/`dumpsxiso` GPL-2.0-or-later | `src/third_party/psn00bsdk` |
| cgltf | v1.15 | MIT | `src/third_party/cgltf/cgltf.h` |
| stb | `003501f` | PD/MIT (dual) | `src/third_party/stb/stb_image.h`, `stb_image_write.h` |
| glad (glad2) | OpenGL 3.3 Core | MIT | `src/third_party/glad/` |
| OpenGL | runtime | от драйвера | загружается glad2 |
| Jo Engine | v2026.2 (`5e99944`) | MIT | `JO_ENGINE_ROOT` (env-install; `src/hal/saturn/` использует SDK) |
| sh-none-elf-gcc | GNU GCC 8.2.0 + newlib | GPL-3.0-or-later + GCC Runtime Library Exception; newlib BSD-style | bundled в Jo Engine `Compiler/LINUX` |
| Mednafen | 1.32.1 | GPL-2.0-or-later | только локальная ручная приёмка; не вендорится и не распространяется |

### Sega Saturn

- **Jo Engine** распространяется под MIT; при сборке `saturn-release`
  используются его исходники/компилятор, но бинарники Jo Engine в
  репозиторий не вендорятся.
- **GCC 8.2.0** — GPL-3.0-or-later с GCC Runtime Library Exception;
  скомпилированный код движка не становится GPL. **newlib** — BSD-style
  лицензии (сохранять notices при распространении toolchain).
- **BIOS Saturn** (`mpr-17933.bin`, `sega_101.bin`) не входит в проект,
  не вендорится, не публикуется в CI/артефактах; пользователь
  предоставляет его локально.
- **Mednafen** (GPL-2.0-or-later) — только dev-инструмент ручной приёмки;
  в релизные архивы не входит.

## Правила сохранения notices

1. **Собственный код**: в начале каждого файла — `SPDX-License-Identifier: MIT`.
2. **Вендоренные файлы**: не изменять их лицензионные заголовки.
3. **Бинарные релизы**: включать `LICENSE` и настоящий `LEGAL.md`.
4. **PSn00bSDK**: при распространении инструментов `mkpsxiso`/
   `dumpsxiso` сохранять GPL-2.0 notices; core SDK — MPL-2.0.
5. **stb**: при желании указать "Public Domain" или MIT по выбору.

## Отказ от ответственности

OpenGL® является зарегистрированным товарным знаком Silicon Graphics, Inc.
Sony PlayStation® является товарным знаком Sony Interactive Entertainment
Inc. RetroPolyEngine не связан с этими компаниями и не претендует на
официальную поддержку какой-либо платформы.
