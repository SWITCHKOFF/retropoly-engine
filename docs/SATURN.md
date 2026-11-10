# Sega Saturn — платформа, toolchain и контракты v0.2.0

> Статус: **S21–22 (недели 41–44) — локальный gate закрыт**.
> `saturn-release` собирается с чистого checkout + pinned Jo Engine v2026.2;
> bootable ISO/CUE + IP.BIN + ELF/raw/map проходят структурные проверки.
> Ручной 60-секундный smoke в Mednafen 1.32.1 с легальным Japanese BIOS
> (1.01) пройден: VDP2 backdrop виден, краша нет; скриншот —
> [`../screenshots/saturn_backdrop.png`](../screenshots/saturn_backdrop.png).
> CI на удалённом runner ещё не прогонялся. 3D/VDP1/VDP2-контент — S27–32.
>
> Нормативные решения: [`adr/ADR-008-saturn-backend.md`](adr/ADR-008-saturn-backend.md).
> План: [`plans/sprint_21_22.md`](plans/sprint_21_22.md).
> Сборка: [`BUILDING.md`](BUILDING.md) § Saturn.

## 1. Что уже работает (S21–22)

- CMake preset `saturn-release` и compile-time backend `PLATFORM_SATURN`
  (ровно один из PC/PS1/Saturn).
- Pinned **Jo Engine v2026.2** + bundled **GCC 8.2.0 `sh-none-elf`**
  (big-endian SH-2); лицензии — [`LEGAL.md`](LEGAL.md), пины —
  [`../VERSION.txt`](../VERSION.txt).
- `src/hal/saturn/`: C++ lifecycle/backdrop/input/FS и `hal_render_backend_*`
  stubs + узкий C-мост к Jo Engine; `hal/platform.h` не содержит SH-2/Jo
  Engine; бюджет — `saturn_memory.h`.
- Bootable ISO9660-образ (`0.BIN` + `IP.BIN`) и CUE; артефакты публикует
  `.github/workflows/build-saturn.yml`.
- CI: configure/build + структурная проверка ELF/ISO/IP/CUE. Эмулятор в CI
  **не запускается** (BIOS не вендорится); 60-секундный smoke — ручной.

## 2. Проверенные reference-факты (и уточнения к §2.2 документации)

Легенда: **HW** — поведение кремния/документации; **policy** — решение
движка, которое можно менять только ADR.

| Параметр | Проверенное значение | Статус | Источник |
|---|---|---|---|
| CPU | 2× Hitachi SH7604 (SH-2), Master + Slave, 32-bit, 4 KiB cache на CPU | HW | SH7604 Hardware Manual; SCU User's Manual |
| Тактовая частота | **26.8741 MHz** в 320-режиме (NTSC); **28.6364 MHz** в 352-режиме (NTSC). «28.6 MHz» — максимум/352-режим, не частота target 320×224 | HW, уточнение | SMPC User's Manual, Table 1.2 |
| Work RAM | 2 MiB total: WRAM-L 1 MiB (`0x06000000`, CPU-bus) + WRAM-H 1 MiB (`0x00200000`, общая шина) | HW | SCU User's Manual §1.2; Copetti (secondary) |
| Sound RAM | 512 KiB | HW | SCSP User's Manual; Copetti |
| VDP1 VRAM | 512 KiB (4 Mbit) + два 256 KiB frame buffer (A/B) | HW | VDP1 User's Manual §1 (System Configuration) |
| VDP2 VRAM | 512 KiB (4 Mbit; контроллер поддерживает 4 или 8 Mbit) + **4 KiB Color RAM** (32 Kbit) | HW | VDP2 User's Manual §1.1/§1.2 |
| SCU DSP SRAM | 32 KiB | HW | SCU User's Manual; Copetti |
| Разрешение NTSC | Normal 320×224; non-interlace 320×240; 352×224/240; hi-res 640×224/240 (и выше) | HW | VDP2 User's Manual §2.1 |
| Кадровая частота | 60 полей/с в non-interlace NTSC (1/60 c) | HW | VDP2 User's Manual §2.2 |
| Z-буфер | Нет per-pixel Z-buffer; VDP1 не выполняет VSD, порядок примитивов задаёт ПО (Painter's/SGL Z-sort) | HW + policy | VDP1 User's Manual; Copetti (secondary); ADR-003 |
| Примитивы VDP1 | Полигоны — заполненные **четырёхугольники** (4 вершины); distorted sprites; треугольники конвертируются движком | HW + policy | VDP1 User's Manual §1; ADR-008 §6 |
| Максимальный character pattern | 8..504 px по X (шаг 8), 1..255 px по Y; 4/8/16 bpp | HW, уточнение | VDP1 User's Manual §5.1 |
| «256×256 max texture» из §2.2 | **Не hardware maximum**: это engine-policy потолок `.tex` v1/asset pipeline; VDP1 допускает до 504×255, но VRAM 512 KiB ограничивает бюджет | policy | §2.2 инженерной документации; ADR-008 |
| CD-ROM | 2× speed, reference 300 KiB/s; ISO9660 | reference | Disc Format Standards ST-040-R4; Copetti |
| Target profile | NTSC 320×224, 60 Hz, CD-ROM, 6-button pad | policy | ADR-008 §4 |

> Важно: «28.6 MHz» и «256×256» из §2.2 — **ориентиры**, а не acceptance-
> контракт. Для target 320×224 используется 26.8741 MHz; потолок 256×256
> сохраняется как политика ассетов, а не как утверждение о кремнии.

### 2.1. Карта памяти, используемая engine

```
0x06000000  WRAM-L (1 MiB)       код/данные/стек/engine work
0x06004000  старт программы      ___Start (Jo Engine linker script)
0x060FBC00  верх program/work    master stack 0x060FBC00..0x060FFC00 (16 KiB)
0x00200000  WRAM-H (1 MiB)       asset arena v0.2.0
```

`0x06004000` — first read address из `IP.BIN` (проверено по шаблону Jo
Engine и по ELF entry point). `jo.linker` содержит ASSERT:
`__jo_work_end <= 0x060FBC00` (иначе «Program too big»).

## 3. Toolchain

### 3.1. Состав

| Компонент | Pin | Лицензия | Где |
|---|---|---|---|
| Jo Engine | v2026.2, commit `5e999442…` | MIT | `JO_ENGINE_ROOT` (env-install) |
| sh-none-elf-gcc | GNU GCC 8.2.0 + newlib headers | GPL-3.0-or-later + GCC Runtime Library Exception; newlib — BSD-style | `$JO_ENGINE_ROOT/Compiler/LINUX` |
| Linker script | `Compiler/COMMON/jo.linker` | часть Jo Engine (MIT) | `$JO_ENGINE_ROOT/Compiler/COMMON` |
| `mkisofs`/`genisoimage` | host package | GPL-2.0-or-later | CI `apt-get install genisoimage` |
| Mednafen (ручная приёмка) | 1.32.1 | GPL-2.0-or-later | env-install; BIOS отдельно, не вендорится |

Почему не отдельный `sh-elf-gcc`: Jo Engine поставляет согласованный
компилятор, newlib-заголовки, linker script и стартовый код; второй
toolchain на v0.2.0 не тянем (ADR-008 §2). Тройка компилятора —
`sh-none-elf` (не `sh-elf`), ABI — big-endian SH-2.

### 3.2. Установка

```sh
git clone --depth 1 --branch v2026.2 \
  https://github.com/johannes-fetz/joengine.git "$HOME/joengine"
export JO_ENGINE_ROOT="$HOME/joengine"
```

Для CI-reproducibility проверяются commit и SHA-256 компилятора:

```sh
(cd "$JO_ENGINE_ROOT" && sha256sum -c ci/joengine-v2026.2.sha256)
```

Полный checkout должен содержать `Compiler/LINUX`, `Compiler/COMMON`,
`Compiler/WINDOWS/sh-elf/include` (newlib headers, используются и на
Linux) и `jo_engine`. Sparse-checkout без этих каталогов не соберётся.

### 3.3. Сборка

```sh
cmake --preset saturn-release
cmake --build --preset saturn-release
```

Артефакты (`build/saturn-release/src/hal/saturn/`):

| Файл | Что это |
|---|---|
| `retro_saturn.elf` | SH-2 ELF, entry `0x06004000` |
| `retro_saturn.bin` | raw-программа (objcopy ELF) |
| `retro_saturn.iso` | bootable ISO9660 (2048-byte sectors, `0.BIN` + IP.BIN) |
| `retro_saturn.cue` | CUE `TRACK 01 MODE1/2048` |
| `IP.BIN` | Saturn IP header (title `RetroPolyEngine`) |
| `retro_saturn.map` | linker map, `JO_WORK`/stack ASSERT |

> Формат образа: pinned Linux CueMaker Jo Engine выпускает
> **MODE1/2048**, а cdrkit `genisoimage` не умеет raw 2352. Это capability
> toolchain, а не выбор движка. 2352/CCD — кандидат на отдельную задачу,
> если понадобится прожиг/железо; для Mednafen 1.32.1 2048-ISO достаточно.
> В образ кладётся **raw-программа** `retro_saturn.bin` (objcopy), не ELF:
> CI проверяет `cmp` извлечённого `0.BIN;1` с raw.

## 4. Target profile и разрешение

- **Native:** 320×224, NTSC, 60 Гц, non-interlace, 6-button pad, CD-ROM.
- Общий reference движка — **320×240** (PC/PS1, `RENDER_FB_WIDTH/HEIGHT`).
- Saturn policy v0.2.0 — **crop**: показываются центральные 224 строки
  reference-кадра (8..231). Letterbox и 320×240-режим не выбираются без
  нового ADR. HUD не размещать в строках 0..7 и 232..239.
- `Render_Init` Saturn принимает только 320×224; иное — `false` +
  диагностика (не silent clamp).

## 5. Память

`src/hal/saturn/saturn_memory.h` фиксирует ledger и `static_assert`-ы:

| Ресурс | Ёмкость | Владелец v0.2.0 |
|---|---|---|
| WRAM-L | 1 MiB | код/данные/стек + Jo Engine work |
| WRAM-H | 1 MiB | asset arena (mesh/CI8/палитры) |
| Sound RAM | 512 KiB | SCSP (post-MVP, не занимаем) |
| VDP1 VRAM | 512 KiB | команды/CLUT/текстуры (S27–28) |
| VDP1 FB | 2×256 KiB | page flip A/B |
| VDP2 VRAM | 512 KiB | фоны/NBG (S29–30) |
| VDP2 CRAM | 4 KiB | RGB555-палитры |
| SCU DSP SRAM | 32 KiB | SCU DSP (не бюджет движка) |

Правила: только static storage/арены; runtime `new`/`delete`/`malloc`/
`free` запрещены (ADR-007). Фактическое потребление — MemoryProfiler
(S23–24). Linker ASSERT + map — часть gate.

## 6. Ввод: 6-button pad → `INPUT_PAD_*`

| Saturn | `INPUT_PAD_*` | Примечание |
|---|---|---|
| D-pad | UP/DOWN/LEFT/RIGHT | цифровой |
| A | CROSS | нижняя/основная кнопка |
| B | CIRCLE | |
| C | SQUARE | |
| X | TRIANGLE | верхний ряд |
| Y | L1 | |
| Z | R1 | |
| L | L2 | shoulder |
| R | R2 | shoulder |
| START | START | |
| — | SELECT | на 6-button pad отсутствует |

Аналоговых осей и мыши в v0.2.0 нет: `Input_GetPadAxis` → 0,
`Input_SetMouseRelativeMode` → false. Enum расширяется только перед
`INPUT_PAD_COUNT` с ADR/обновлением всех бэкендов.

## 7. Ассеты и triangle→quad

- `.mesh` v1 (triangle list) и `.tex` v1 (CI8 + RGB555+STP) остаются
  общими; Saturn-специфичный формат не вводится.
- `.tex` CI8/256 → VDP1 8bpp CLUT; палитра 256×RGB555 помещается в CRAM.
- VDP1 рисует квады: пара треугольников с общей стороной и материалом
  объединяется в quad; без пары — вырожденный quad (`v3 = v2`/`v0`) с
  сохранением winding/UV. Детали — S27–28, policy — ADR-008 §6.
- Потолок `.tex` v1 256×256 — engine policy (не hardware max); при
  необходимости большего — новый формат/ADR, v1 не ломать.

## 8. FileSystem и образ

- Read-only ISO9660, 2048-byte sectors, CUE `MODE1/2048`.
- Имена 8.3 upper-case, `NAME.EXT;1`; разделитель `\`.
- Host-пути, `..`, абсолютные пути, запись — запрещены.
- Отсутствующий/повреждённый файл → `{nullptr, 0}` + диагностика.
- S21–22: `FileSystem_LoadFile` — заглушка; реализация — S23–24.
- В образе v0.2.0: `0.BIN` (программа) + `IP.BIN` (boot header) +
  ABS/CPY/BIB. Ассеты появятся в S23–24 вместе с CD-FS.

## 9. Эмулятор и ручной smoke-тест

Pinned: **Mednafen 1.32.1**. BIOS не входит в репозиторий/CI/артефакты.

1. Установить Mednafen 1.32.1 (source: `https://mednafen.github.io/releases/`).
2. Положить легально приобретённый BIOS:
   - NA/EU `mpr-17933.bin` (по умолчанию `ss.bios_na_eu`);
   - JP `sega_101.bin` (по умолчанию `ss.bios_jp`).
   BIOS можно указать явно:
   ```sh
   mednafen -ss.bios_jp /path/to/sega_101.bin \
            -ss.region_autodetect 0 -ss.region_default jp \
            build/saturn-release/src/hal/saturn/retro_saturn.cue
   ```
   Для мультирегионального IP.BIN (`JTUE`) обязательно отключить
   autodetect, иначе Mednafen выберет NA/EU BIOS.
3. Настройки фиксируются в `~/.mednafen/mednafen.cfg`:
   `ss.bios_sanity=1`, `ss.region_autodetect=0`, `ss.region_default=jp`,
   `ss.correct_aspect=1`, `ss.h_overscan=1`, `ss.h_blend=0`,
   `ss.cart=none`, `ss.input.port1=gamepad`.
4. Smoke-чек-лист (выполнен 2026-11-10):
   - [x] образ стартует без BIOS error screen;
   - [x] виден однотонный тёмно-синий backdrop 320×224;
   - [x] 60 секунд без краша/зависания/сброса
         (`timeout 60` → exit 124, в логе нет fatal/assert);
   - [x] скриншот сохранён: [`../screenshots/saturn_backdrop.png`](../screenshots/saturn_backdrop.png)
         (BIOS в кадр не попадает);
   - [x] `retro_saturn.map`: `__jo_work_end = 0x060E160C < 0x060FBC00`.

CI выполняет только build + structural checks; пункты 1–4 — ручная
приёмка. Если BIOS нет, gate **не закрыт** — это явный blocker, а не
повод ослабить CI.

## 10. CI

`.github/workflows/build-saturn.yml`:

1. checkout без submodules (Saturn не тянет SDL3/PSn00bSDK);
2. `apt-get install ninja-build genisoimage`;
3. sparse clone Jo Engine v2026.2, проверка commit + `ci/joengine-v2026.2.sha256`;
4. `cmake --preset saturn-release` + build;
5. проверки: SH-2 ELF, entry `0x0604000`, `SEGA SEGASATURN` в IP.BIN,
   `0.BIN;1` в ISO, CUE `MODE1/2048`, raw ≤ 1 MiB;
6. upload `retro-saturn-disc` (ISO/CUE/ELF/BIN/IP/map).

## 11. Известные ограничения S21–22

- Нет запуска эмулятора в CI и нет BIOS в репозитории.
- Нет 3D/VDP1/VDP2-контента: только backdrop.
- Нет ввода/CD-FS/render clear implementation — контракты и заглушки
  готовы, реализация S23–24.
- Нет PAL, 640×480, 3D pad, SCSP PCM, CD streaming.
- 2048-ISO/CUE — capability pinned toolchain; raw 2352/CCD не выпускается.

## 12. Источники

### 12.1. Первоисточники Sega / Hitachi

- Sega, **VDP1 User's Manual**, Doc. ST-013-R3-061694:
  <https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf>
  (system configuration, quad polygons, character size §5.1).
- Sega, **VDP2 User's Manual**, Doc. ST-058-R2-060194:
  <https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf>
  (VRAM 4/8 Mbit, Color RAM 32 Kbit, resolution/60 Hz).
- Sega, **SCU User's Manual**, Third Version, Doc. ST-097-R5-072694:
  <https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf>
  (memory map: WRAM-L/H, VDP1/VDP2 regions).
- Sega, **SMPC User's Manual**, Doc. ST-169-R1-072694:
  <https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf>
  (Table 1.2: 26.8741/28.6364 MHz PLL, pad/peripherals).
- Sega, **SCSP User's Manual**, Doc. ST-077-R2-052594:
  <https://antime.kapsi.fi/sega/files/ST-077-R2-052594.pdf>
  (sound RAM/audio subsystem).
- Sega, **Dual CPU User's Guide**, Doc. ST-202-R1-120994:
  <https://antime.kapsi.fi/sega/files/ST-202-R1-120994.pdf>
  (Master/Slave, SCU synchronization).
- Sega, **Disc Format Standards Specification Sheet**, ST-040-R4-051795:
  <https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf>
  (ISO9660/IP.BIN/образ).
- Sega, **Boot ROM User's Manual**, ST-079B-R3-011895:
  <https://antime.kapsi.fi/sega/files/ST-079B-R3-011895.pdf>
  (IPL/boot flow).
- Hitachi, **SH7604 Hardware Manual**:
  <https://antime.kapsi.fi/sega/files/sh7604.pdf>
  (SH-2, cache, DMA, шины).
- Hitachi, **SH-1/SH-2 Programming Manual**:
  <https://antime.kapsi.fi/sega/files/h12p0.pdf>

### 12.2. Toolchain / эмулятор

- Jo Engine v2026.2: <https://github.com/johannes-fetz/joengine/releases/tag/v2026.2>
  (commit `5e99944238030821a48e17a40abefd77e81953c2`, MIT).
- Jo Engine site/docs: <https://www.jo-engine.org/>,
  <https://www.jo-engine.org/doxygen/>.
- Mednafen 1.32.1: <https://mednafen.github.io/releases/>;
  настройки Saturn — `src/ss/ss.cpp` (`ss.bios_na_eu`, `ss.bios_jp`,
  `ss.bios_sanity`, `ss.correct_aspect`, `ss.h_overscan`, `ss.h_blend`).

### 12.3. Secondary

- Rodrigo Copetti, *Sega Saturn Architecture*:
  <https://www.copetti.org/writings/consoles/sega-saturn/>
  (используется только для сверки/контекста; при расхождении приоритет у
  первоисточников Sega/Hitachi).
