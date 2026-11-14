# SDK sizes — PC / PS1 / Saturn (S33–34)

Замер: 2026-11-13, Linux x86_64, GCC 13.3 (PC), PSn00bSDK v0.24 (MIPS),
Jo Engine v2026.2 / sh-none-elf-gcc 8.2.0. Release-конфигурация.

| Пакет | Install-tree | Файлов | `.tar.gz` | `.zip` | Установка |
|---|---:|---:|---:|---:|---:|
| PC (`pc/x86_64`) | 14 MiB | 151 | 3.07 MiB | 3.19 MiB | 0.10 s |
| PS1 (`ps1/mips`) | 848 KiB | 43 | 224 KiB | 249 KiB | 0.01 s |
| Saturn (`saturn/sh2`) | 1.7 MiB | 100 | 344 KiB | 427 KiB | 0.03 s |

Примечания:

- PC-пакет включает static SDL3 + публичные SDL3-заголовки, glad и
  OpenGL-загрузчик; поэтому он заметно больше кросс-пакетов.
- PS1-пакет содержит только headers/libs/config/examples: PSn00bSDK
  (MPL-2.0, toolchain) не вендорится.
- Saturn-пакет содержит `libjo_engine.a` (MIT) и публичные заголовки
  Jo Engine; bundled-компилятор не входит.
- Время установки — `cmake --install` в пустой каталог на локальном SSD;
  медиана из одного прогона, ориентир, не SLA.
- Контрольные суммы и размеры фиксируются в `sdk_manifest.json` и
  `checksums.sha256`; повторный запуск `scripts/package_sdk.py` при
  неизменном коммите/тулчейне даёт те же архивы (детерминированный mtime).

Воспроизведение:

```sh
cmake --build build/pc-release --target sdk_package
cmake --build build/ps1-release --target sdk_package
cmake --build build/saturn-release --target sdk_package
ls -lh dist/retro-engine-sdk-*
```
