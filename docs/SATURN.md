# Sega Saturn — платформа, toolchain и контракты v0.2.0

> **Статус (2026-11-13):** S21–32 реализованы; visual/stress gate в
> Mednafen 1.32.1 пройдены. **GitHub Release v0.2.0 отозван** — payload
> содержит demo-образы, но не готовый SDK и UI-клиент; публичный релиз
> требует release gate v2 ([`RELEASING.md`](RELEASING.md)).
>
> S23–24, S25–26, S27–28, S29–30 и S31–32 — gate закрыт. `saturn-release` собирается с pinned Jo Engine v2026.2; bootable
> ISO/CUE + IP.BIN + ELF/raw/map проходят структурные проверки. Работают
> lifecycle VDP1/VDP2 (320×224 NTSC), 6-button pad, read-only ISO9660 FS с
> арены Work RAM-L, VDP2-лог и детерминированный clear. CD-ассеты
> (`CUBE.MSH`, `TEX0.TEX`…) читаются с образа и валидируются загрузчиками
> ядра. **S25–26:** оба SH-2 реально используются через Slave worker
> (SPSC-очередь, bounded wait, fallback); параллельный clip-space workload
> даёт **32%** выигрыша (23838 → 16113 µs), hash single/dual совпадает.
> **S27–28:** VDP1-полигоны (flat/Gouraud и textured 8bpp CI8), CLUT 1..6,
> triangle→quad, Painter's порядок без Z-буфера, VRAM-аллокатор 451 832 B,
> FNV-hash VDP1 framebuffer; ADR-010. 5-минутный smoke S23–24 и 10-минутный
> stress S25–26 в Mednafen 1.32.1 с легальным Japanese BIOS (1.01) пройдены;
> скриншоты —
> [`../screenshots/saturn_clear_s23_24.png`](../screenshots/saturn_clear_s23_24.png),
> [`../screenshots/saturn_dual_cpu_s25_26.png`](../screenshots/saturn_dual_cpu_s25_26.png).
> VDP2-контент/hello_triangle — S29–32.
>
> Нормативные решения: [`adr/ADR-008-saturn-backend.md`](adr/ADR-008-saturn-backend.md),
> [`adr/ADR-009-master-slave.md`](adr/ADR-009-master-slave.md),
> [`adr/ADR-010-vdp1-polygons.md`](adr/ADR-010-vdp1-polygons.md).
> Планы: [`plans/sprint_21_22.md`](plans/sprint_21_22.md),
> [`plans/sprint_23_24.md`](plans/sprint_23_24.md),
> [`plans/sprint_25_26.md`](plans/sprint_25_26.md),
> [`plans/sprint_27_28.md`](plans/sprint_27_28.md).
> Benchmark-снапшот: [`benchmarks/saturn_reference_environment.yaml`](benchmarks/saturn_reference_environment.yaml).
> Сборка: [`BUILDING.md`](BUILDING.md) § Saturn.

## 1. Что уже работает (S23–26)

- CMake preset `saturn-release` и compile-time backend `PLATFORM_SATURN`
  (ровно один из PC/PS1/Saturn).
- Pinned **Jo Engine v2026.2** + bundled **GCC 8.2.0 `sh-none-elf`**
  (big-endian SH-2); лицензии — [`LEGAL.md`](LEGAL.md), пины —
  [`../VERSION.txt`](../VERSION.txt).
- `Platform_Init/Shutdown` идемпотентны; core init проверяет наблюдаемые
  инварианты (320×224, NTSC TVSTAT, наличие `0.BIN`) и при сбое маскирует
  SCU-прерывания. `Platform_PumpEvents` опрашивает пад **ровно один раз** за
  кадр; `Input_*` читают защёлкнутое состояние. Полного deinit API у Jo
  Engine нет: `Shutdown` выключает VDP2-вывод и маскирует `JO_SCU_IMS` /
  `JO_BIOS_SCU_IMS`; повторный `Init` допустим.
- Read-only ISO9660: bare 8.3-имена без `;1` (`CUBE.MSH`, `TEX0.TEX`),
  проверка размера, точное чтение секторами 2048 Б, first-fit asset heap
  (932 KiB) с coalescing; любая ошибка возвращает 0 байт persistent-памяти.
- Render skeleton: `hal_render_backend_*` принимает только 320×224,
  `Render_BeginFrame/Clear/EndFrame` меняют VDP2 backdrop; VDP1-draw/texture/
  readback честно возвращают `false` + диагностику (не silent drop).
- `Log_Message` → VDP2-текст (строка 27); форматирование — `jo_vsnprintf`
  в C-мосте (без newlib `vsnprintf`/malloc/sbrk в runtime).
- Boot self-test: `CUBE.MSH` + `TEX0.TEX` читаются с CD, валидируются
  `mesh_validate_and_load`/`texture_validate_and_load`; `CUBE.MSH` остаётся
  в persistent-арене и служит входом параллельного workload, текстура
  освобождается.
- Bootable ISO9660-образ (`0.BIN` + `IP.BIN` + `.mesh`/`.tex`) и CUE;
  артефакты публикует `.github/workflows/build-saturn.yml`.
- **S25–26:** Slave worker (`scu_sync`, `slave_worker`, SPSC-очередь 8 job'ов),
  handshake 200 мс, bounded wait 20 мс, явный `FAILED` и single-CPU fallback;
  `c`/`w`/`slave_jobs` — в VDP2-метриках. Параллельный workload — clip-space
  подготовка 256 треугольников (128/CPU), fixed 16.16, hash single==dual.
- **S27–28:** VDP1-путь `vdp1_backend.*`: view-space → экран 320×224 (fixed),
  flat/Gouraud polygon и textured distorted sprite 8bpp CI8 + CLUT 1..6,
  triangle→quad с UV-углами, first-fit VRAM-аллокатор 451 832 B,
  команды через pinned Jo Engine pipeline (radix sort far→near, VBlank/DMA),
  Painter's порядок без Z-буфера. Два набора Gouraud-таблиц по чётности
  кадра; переполнение command list/VRAM/CLUT — явный `false` + диагностика.
  `rpe_saturn_vdp1_fb_hash()` — FNV-1a по 320×224 словам VDP1 FB. Демо
  рисует текстурированный Gouraud-куб `CUBE.MSH` + `TEX0.TEX` с CD.
- Host-тесты `saturn_hal_tests` (**106 кейсов**) — без SH-2/эмулятора; CI
  `tests.yml` запускает их в PC-debug. Эмулятор в Saturn-CI **не
  запускается** (BIOS не вендорится); smoke/stress — ручные. Сборки
  `saturn-single` (`RPE_SATURN_DUAL_CPU=OFF`) и fault-варианты `hang`/`flood`
  проверяются сборкой в CI.

## 2. Проверенные reference-факты (и уточнения к §2.2 документации)

Легенда: **HW** — поведение кремния/документации; **policy** — решение
движка, которое можно менять только ADR.

| Параметр | Проверенное значение | Статус | Источник |
|---|---|---|---|
| CPU | 2× Hitachi SH7604 (SH-2), Master + Slave, 32-bit, 4 KiB cache на CPU | HW | SH7604 Hardware Manual; SCU User's Manual |
| Тактовая частота | **26.8741 MHz** в 320-режиме (NTSC); **28.6364 MHz** в 352-режиме (NTSC). «28.6 MHz» — максимум/352-режим, не частота target 320×224 | HW, уточнение | SMPC User's Manual, Table 1.2 |
| Work RAM | 2 MiB total: **Work RAM-H** 1 MiB (`0x06000000`, high-speed CPU bus) + **Work RAM-L** 1 MiB (`0x00200000`, low-speed shared bus; SCU DMA сюда не ходит) | HW | SCU User's Manual §1.2; Copetti (secondary) |
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

Терминология исправлена в S25–26: **H** — `0x06000000` (high-speed CPU bus),
**L** — `0x00200000` (low-speed shared bus). В S23–24 подписи были наоборот
(см. ADR-008, уточнение 2026-11-10).

```
0x06000000  Work RAM-H (1 MiB)   код/данные/BSS/стек/engine work
0x06004000  старт программы      ___Start (Jo Engine linker script)
0x060FBC00  верх program/work    master stack 0x060FBC00..0x060FFC00 (16 KiB)
0x00200000  Work RAM-L (1 MiB)   +0 KiB     asset heap (932 KiB)
                                  +932 KiB   frame scratch StackAllocator (48 KiB)
                                  +980 KiB   command arena LinearAllocator (16 KiB)
                                  +996 KiB   parallel shared region (28 KiB, S25–26)
```

`0x06004000` — first read address из `IP.BIN` (проверено по шаблону Jo
Engine и по ELF entry point). `jo.linker` содержит ASSERT:
`__jo_work_end <= 0x060FBC00` (иначе «Program too big»).

#### 2.1.1. Shared-память Master + Slave (S25–26)

| Регион | Адрес | Владелец / контракт |
|---|---|---|
| `s_shared_local` (SPSC control: head/tail/completed, 8 job'ов, stats) | static BSS Work RAM-H; доступ через `addr \| 0x20000000` | Master пишет `head`, Slave — `tail`/`completed`; только cache-through |
| parallel work (3×128 `PreparedTriangle` + batch; зарезервировано 28 KiB) | Work RAM-L `0x002F9000`..`0x00300000` (28 KiB) | Master пишет 1-ю половину/fallback, Slave — 2-ю; output Slave читается Master после purge |
| mesh input (`CUBE.MSH`) | asset heap Work RAM-L | read-only после загрузки; Slave purge перед job'ом |
| asset heap / frame scratch / command arena | Work RAM-L | Master-only в v0.2.0 |

Кэши SH-2 не когерентны: любое shared-поле читается/пишется только через
cache-through alias; после `wait` Master вызывает purge. Slave не трогает
VDP1/VDP2/VRAM — только Work RAM-L. Детали протокола — ADR-009.

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
| `retro_saturn.iso` | bootable ISO9660 (2048-byte sectors, `0.BIN` + IP.BIN + `.mesh`/`.tex`) |
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
| Work RAM-H | 1 MiB | код/данные/BSS/стек Master + Jo Engine work |
| Work RAM-H: slave worker control | ~0.2 KiB static BSS | SPSC queue + metrics, cache-through |
| Work RAM-L: asset heap | 932 KiB | first-fit heap (mesh/CI8/палитры); coalescing |
| Work RAM-L: frame scratch | 48 KiB | `StackAllocator` временных данных кадра |
| Work RAM-L: command arena | 16 KiB | `LinearAllocator` + `vdp1_command_list` (S27–28) |
| Work RAM-L: parallel shared | 28 KiB reserved | S25–26: 3×128 `PreparedTriangle` (8 B) + batch ≈3.1 KiB; запас на S27–28 |
| Sound RAM | 512 KiB | SCSP (post-MVP, не занимаем) |
| VDP1 VRAM | 512 KiB | команды/CLUT-адреса/текстуры (S27–28): texture area 451 832 B + Gouraud 512 B |
| VDP1 FB | 2×256 KiB | page flip A/B |
| VDP2 VRAM | 512 KiB | фоны/NBG (S29–30) |
| VDP2 CRAM | 4 KiB | RGB555-палитры |
| SCU DSP SRAM | 32 KiB | SCU DSP (не бюджет движка) |

Правила: только static storage/арены; runtime `new`/`delete`/`malloc`/
`free` запрещены (ADR-007). `AssetHeap` — first-fit free-list с
коалесцированием, распознаёт double-free/foreign pointer/OOM (без порчи
heap). Три региона Work RAM-L (asset/scratch/command) регистрируются в
MemoryProfiler; parallel shared входит в ledger без отдельного profiler-
источника. Бюджет RAM — 2 MiB. `static_assert` фиксирует сумму регионов
Work RAM-L ровно 1 MiB (932 + 48 + 16 + 28). Linker ASSERT + map — часть gate; после S25–26
`__jo_work_end` оставляет ≈38 KiB до master stack.

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

Реализация (`saturn_input.cpp`): C-мост возвращает raw active-low 16-bit
`jo_inputs[0].data` + флаги `connected`/`regular`. `Platform_PumpEvents`
вызывает `input_poll()` ровно один раз; HAL инвертирует биты и мапит их в
`INPUT_PAD_*`. Если пад отключён или тип периферии не
`JoRegularGamepad`, защёлка обнуляется (безопасные значения).

## 7. Ассеты и triangle→quad

- `.mesh` v1 (triangle list) и `.tex` v1 (CI8 + RGB555+STP) остаются
  общими; Saturn-специфичный формат не вводится.
- `.tex` CI8/256 → VDP1 8bpp CLUT; палитра 256×RGB555 помещается в CRAM
  (RPE — палитры 1..7; 0..255 слова CRAM — VDP2-текст Jo Engine).
- VDP1 рисует квады. Политика S27–28 (ADR-010): UV трёх вершин обязаны
  быть в пределах 2 текселей от трёх разных углов прямоугольника текстуры;
  source rect квантуется по X до 8 текселей, по Y до 1. Четвёртая вершина
  дублирует следующий назначенный угол (TL→TR→BR→BL). UV не в углах →
  flat-Gouraud с диагностикой, а не искажённая текстура.
- `.tex` 256×256: VDP1 pattern height max 255, поэтому загружаются строки
  0..254 (потеря последней строки, `height_clamped`); splitting на два
  pattern'а — future work. Потолок 256×256 — engine policy, не hardware max.
- Детали — [`adr/ADR-010-vdp1-polygons.md`](adr/ADR-010-vdp1-polygons.md).

## 8. FileSystem и образ

- Read-only ISO9660, 2048-byte sectors, CUE `MODE1/2048`.
- HAL принимает **bare 8.3-имена без `;1`**: `[A-Z0-9_-]{1,8}`
  (`.[A-Z0-9_-]{1,3}`)?; строчные буквы нормализуются в upper-case.
  Jo Engine сам добавляет/скрывает `;1` и сравнивает 12 символов.
- Host-пути (`/`, `\`, буква диска, `:`), `;`, `..`, wildcard'ы,
  абсолютные пути и запись — запрещены (`saturn_fs_path.cpp`).
- Размер проверяется до аллокации (`> 8 MiB` — отказ); файл читается
  секторами по 2048 Б через `__jo_fs_read_sectors` с точной длиной
  (`jo_fs_read_file` не используется: пишет NUL на байт за буфером).
- Отсутствующий/пустой/обрезанный/повреждённый файл или ошибка чтения →
  `{nullptr, 0}` + диагностика; буфер освобождается, heap не изменён.
- В образе v0.2.0: `0.BIN` (программа) + `IP.BIN` (boot header) +
  ABS/CPY/BIB + `CUBE.MSH`, `TRIANGLE.MSH`, `TEXTURE.TEX`, `TEX0.TEX`…
  `TEX3.TEX`. CI проверяет наличие и байтовое совпадение `CUBE.MSH`/`TEX0.TEX`
  с `assets/build/`.
- Boot self-test в `main_saturn.cpp` читает `CUBE.MSH` и `TEX0.TEX`,
  валидирует загрузчиками во временной арене (160 KiB из asset heap),
  логирует размер/FNV-1a и освобождает всё до старта кадрового цикла.

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
4. Smoke-чек-лист S23–24 (выполнен 2026-11-10) и S25–26 (выполнен
   2026-11-10; метрики — §11.4):
   - [x] образ стартует без BIOS error screen;
   - [x] виден тёмно-синий clear 320×224 (`0x9000`), без артефактов;
   - [x] строка 27 VDP2: `[INFO][asset] CD mesh=1 tex=1 heap=0` —
         `CUBE.MSH` и `TEX0.TEX` прочитаны с CD и провалидированы;
   - [x] 5 минут без краша/зависания/сброса
         (`timeout 300` → exit 124; в логе нет fatal/assert; процесс дожил
         до SIGTERM, скриншот снят после 22 с);
   - [x] скриншот сохранён:
         [`../screenshots/saturn_clear_s23_24.png`](../screenshots/saturn_clear_s23_24.png);
   - [x] `retro_saturn.map`: `__jo_work_end = 0x060E560C < 0x060FBC00`
         (запас ≈ 89.5 KiB до master stack; VDP2-log через `jo_vsnprintf`
         не тянет newlib malloc/sbrk);
   - [x] pad: опрос ровно один раз за кадр, active-low mapping и
         idle/disconnect покрыты host-тестами; SMPC-путь использует
         `jo_inputs[0].data` и `JoRegularGamepad` (Jo Engine input.c).

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
   `0.BIN;1` + все `.mesh`/`.tex` в ISO, байтовое совпадение `CUBE.MSH` и
   `TEX0.TEX` с `assets/build/`, CUE `MODE1/2048`, raw ≤ 1 MiB;
6. upload `retro-saturn-disc` (ISO/CUE/ELF/BIN/IP/map).

Host-тесты Saturn HAL (`tests/saturn/`, 63 кейса) собираются и запускаются
в `tests.yml` (PC-debug, `PLATFORM_SATURN=1`, fake C-мост, без SH-2/BIOS).
`build-saturn.yml` дополнительно собирает `saturn-single`, `saturn-hang` и
`saturn-flood` variants, проверяя, что fallback/fault-конфигурации
компилируются и выпускают ELF/ISO.

## 11. Master + Slave SH-2 (S25–26)

> Нормативное решение: [`adr/ADR-009-master-slave.md`](adr/ADR-009-master-slave.md).
> План: [`plans/sprint_25_26.md`](plans/sprint_25_26.md).

### 11.1. Разделение работ

| CPU | Ответственность |
|---|---|
| Master | lifecycle, ввод, CD-ФС, VDP1/VDP2, SMPC/периферия, финальная композиция и отправка команд |
| Slave | только compute-job'ы над shared Work RAM: в S25–26 — clip-space подготовка вершин для будущего command list |

Slave **не пишет** VRAM/VDP-регистры и не вызывает SMPC/CD/FS. VDP1 не
«закреплён» за Slave аппаратно — разделение software-defined.

### 11.2. Синхронизация

- Транспорт — fixed-size **SPSC ring** на 8 job'ов без динамических
  аллокаций (`src/hal/saturn/slave_worker.*`).
- Master пишет `head`, Slave — `tail`/`completed`; публикация — release,
  чтение — acquire (`src/hal/saturn/scu_sync.*`).
- Все shared-поля — только через cache-through alias (`addr | 0x20000000`):
  кэши SH-2 не когерентны. После успешного `wait` Master делает purge.
- Wake-up — FRT input capture Jo Engine (`jo_core_exec_on_slave`).
- **Никаких бесконечных busy-wait:** handshake 200 мс, кадровый wait 20 мс,
  stop 100 мс. Таймаут/ошибка → `FAILED` и single-CPU fallback до
  перезагрузки. `jo_core_wait_for_slave()` не используется (у него нет
  таймаута).
- Overflows/`post` failures/timeouts считаются; очередь возвращает явный
  `false`, а не silent drop.
- Логическая остановка — STOP-job; restart из `STOPPED` повторяет handshake.
  Из `FAILED`/`UNAVAILABLE` restart невозможен без reboot.

### 11.3. Параллельный workload

- 256 треугольников, split `[0..128)` Master / `[128..256)` Slave.
- Вход — `CUBE.MSH` с CD (fallback — встроенный куб), fixed 16.16,
  специализированный `transform_clip` (32×32→64 `dmulu.l`, один clamp на
  компоненту, без делений).
- Результат — 8-байтовый `PreparedTriangle` (FNV hash трёх clip-space
  вершин, depth, near/far flags). Perspective divide/screen-mapping/backface
  остаются на Master в VDP1-стадии S27–28.
- Single-CPU baseline и dual-CPU дают одинаковый hash; при timeout Master
  пересчитывает вторую половину в отдельный fallback-буфер.
- Slave завершает job до `__jo_core_synch()`; в S25–26 он вообще не касается
  VDP, поэтому VBlank/VRAM-контракт не нарушается.

### 11.4. Метрики и измеренный baseline

VDP2-строка 27:

- boot: `[INFO][par] 1=<µs> 2=<µs> g=<%> h=<0|1>` — single/dual время,
  выигрыш, hash match; `2=n/a` в single-CPU сборке;
- кадры: `[INFO][par] f=<frames>c=<completed jobs>w=<wait µs>h=<hash24>`.

Mednafen 1.32.1, Japanese BIOS 1.01, reference-стенд
([`benchmarks/saturn_reference_environment.yaml`](benchmarks/saturn_reference_environment.yaml)):

| Метрика | Значение |
|---|---|
| single-CPU workload | **23838 µs** |
| dual-CPU workload | **16113 µs** |
| выигрыш | **32%** (порог ADR-009 — ≥15%) |
| hash single/dual | совпал (`h=1`) |
| кадровый wait | ≈4007 µs (`w=4007`, `c=1503` при `f=1500`) |
| кадровая частота demo | ≈50–60 FPS |
| hash кадра | `af890f` (стабилен между прогонами) |

10-минутный stress: процесс дожил до `timeout` (exit 124), без
deadlock/hang; середина прогона — `f=12300c=12303w=4035h=af890f`
(≈205 с после boot, hash стабилен). End-screen кадр перекрыт системным
окном, поэтому визуальное подтверждение — stress- и metric-скриншоты.

Скриншоты ручной приёмки:
[10-мин stress, f=12300](../screenshots/saturn_stress_s25_26.png),
[dual benchmark](../screenshots/saturn_dual_cpu_s25_26.png),
[dual metrics f=1500](../screenshots/saturn_dual_metrics_s25_26.png),
[single-CPU fallback](../screenshots/saturn_single_cpu_s25_26.png),
[hang fault → timeout/fallback](../screenshots/saturn_fault_hang_s25_26.png).

### 11.5. Сборки и fault injection

```sh
# dual-CPU (канон)
cmake --preset saturn-release
cmake --build --preset saturn-release

# single-CPU fallback (Slave отсутствует)
cmake --preset saturn-single
cmake --build --preset saturn-single

# Slave зависает на следующем job
cmake --preset saturn-hang
cmake --build --preset saturn-hang

# переполнение очереди (boot-проба)
cmake --preset saturn-flood
cmake --build --preset saturn-flood
```

Ожидаемое поведение: single — `1=... 2=n/a h=1`; hang —
`[ERROR][scu] slave failed: wait timeout` и Master продолжает fallback;
flood — `flood overflows=1`, worker остаётся READY. Host-тесты
`saturn_hal_tests` покрывают unavailable/queue full/post failure/timeout/
stop/restart/flood (63 кейса).

### 11.6. Ручной чек-лист S25–26

- [x] dual-CPU сборка стартует; benchmark `h=1`, `g≥15` (измерено 32%);
- [x] кадровая строка `f/c/w/h` обновляется, `c` растёт вместе с `f`;
- [x] hash кадра стабилен между периодами и повторными прогонами;
- [x] 10-минутный stress без deadlock/hang, стабильный frame time;
- [x] single-CPU сборка: `2=n/a`, Master работает без Slave;
- [x] hang fault: timeout + fallback, без краша;
- [x] flood fault: overflow зафиксирован, worker READY;
- [x] PC/PS1 тесты и сборки не регрессировали.

## 12. VDP1: полигоны и текстуры (S27–28)

> Нормативное решение: [`adr/ADR-010-vdp1-polygons.md`](adr/ADR-010-vdp1-polygons.md).
> План: [`plans/sprint_27_28.md`](plans/sprint_27_28.md).

### 12.1. Карта VDP1 VRAM

| Диапазон | Размер | Владелец |
|---|---:|---|
| `0x00000..0x0001F` | 32 B | jump table |
| `0x00020..0x07FFF` | 32 736 B | command table A (Jo Engine) |
| `0x08000..0x0FFFF` | 32 768 B | command table B (Jo Engine) |
| `0x10000..0x7E4F7` | 451 832 B | RPE texture area |
| `0x7E4F8..0x7E6F7` | 512 B | RPE Gouraud, 2×32 |
| `0x7E700..0x7FF7F` | 6 016 B | Jo Engine 3D clip (не используется) |
| `0x7FF00..0x7FFFF` | 256 B | Jo Engine light Gouraud |

Framebuffer A/B — отдельные 2×256 KiB по `0x25C80000`. `vdp1_vram.h`
проверяет карту `static_assert`-ами; texture allocator — first-fit,
32-байтовое выравнивание, coalescing, явные ошибки.

### 12.2. Конвейер кадра

1. Ядро: view·model → near/far clip → backface cull → Painter's sort.
2. `Render_DrawTriangle`: backend проецирует view-space через `proj`
   (fixed 16.16) в 320×224 (crop 8 строк), делает triangle→quad.
3. `emit_quad`: flat polygon (`CMDCTRL=0x0004`) или textured distorted
   sprite (`0x0002`), Gouraud-таблица, 256-цветный CLUT.
4. `Render_EndFrame`: команды в порядке подачи передаются в
   `__jo_vdp1_add_command` с ключом `count−1−index`; pinned pipeline
   сортирует far→near, линкует, копирует в VRAM и ждёт VBlank.
5. VDP1 автоматически очищает framebuffer цветом `EWDR` (== `Render_Clear`).

### 12.3. Текстуры, CLUT, Gouraud

- CI8 8bpp, width кратна 8 (8..256), height 1..255.
- CLUT — CRAM-палитры **1..6** (7 зарезервирована под VDP2 NBG1
  background с S29–30); максимум 6 уникальных палитр, седьмая — явный
  `CLUT_OUT_OF_MEMORY`.
- `CMDCOLR = palette·256`, `CMDBSIZE = (width/8)<<8 | height`.
- Texture upload — `jo_dma_copy` (SCU DMA, cache-safe) по абсолютному
  адресу `VDP1_TEXTURE_BASE + vram_offset`; ошибка откатывает VRAM и CLUT
  (0 persistent-байт).
- Gouraud-таблица — **аддитивные offsets**, не абсолютные RGB555:
  нейтраль 16 в каждом канале (`0x4210`). Flat: база — цвет первой
  вершины; textured: база — белый, таблица работает как per-vertex тинт.
  Два набора по 32 таблицы, чётность кадра; дедупликация по 4 offsets;
  исчерпание → flat + счётчик.
- 256-высокая текстура: строки 0..254, `height_clamped` + WARNING.

### 12.4. Метрики и golden

VDP2-строка demo (S27–28): `[INFO][dbg] S27-28 f=<frames> c=<commands> o=<overdraw‰>`.
S29–30: `[INFO][dbg] S29-30 f=<frames> c=<commands> o=<overdraw‰> s=<vblank commits>`.

Golden — детерминированный скриншот pinned Mednafen 1.32.1 (crop сцены
побайтно совпал на 60/300 с). `rpe_saturn_vdp1_fb_hash(A/B)` — FNV-1a
32 по 320×224 словам VDP1 framebuffer; API читает VRAM без синхронизации,
поэтому вызывать только когда VDP1 не рисует (host-тест/QA), не каждый
кадр demo. `o` — суммарная площадь VDP1 quad'ов кадра в ‰ экрана
(320×224), fillrate-baseline S27–28: `c=6`, `o=40`.

### 12.5. Ручной чек-лист S27–28

- [x] `saturn-release` стартует в Mednafen 1.32.1 (Japanese BIOS 1.01);
- [x] виден текстурированный Gouraud-куб `CUBE.MSH` + `TEX0.TEX`;
- [x] цвет текстуры и градиент Gouraud корректны, без «поплывшей» UV;
- [x] golden-скриншот стабилен между прогонами (crop 320×200, sha256);
- [x] сцена не меняется на 60/300 с (статичная, deterministic);
- [x] порядок граней без Z-буфера стабилен (Painter's);
- [x] QA-строка `S27-28` читается (VDP2 log; VDP1-overlay перенесён в S31–32 из-за 20 FPS);
- [x] 5-минутный прогон без краша/зависания/сброса (exit 124);
- [x] скриншот сохранён в `screenshots/saturn_vdp1_s27_28.png`;
- [x] PC/PS1 сборки и тесты не регрессировали.

### 12.6. Ограничения S27–28

- Нет perspective-correct UV (аффинная интерполяция, как PS1).
- UV вне углов прямоугольника → flat-Gouraud, а не текстура.
- Максимум 6 уникальных CLUT (7 — VDP2 background); 256-высокая текстура теряет последнюю строку.
- Нет Z-буфера; равная глубина — submission ASC.
- Нет VDP1 RGBA readback (нет 307 KiB static); golden — `fb_hash`
  (host/QA) + детерминированный эмуляторный скриншот.
- Нет запуска эмулятора в CI; BIOS не вендорится.
- VDP1-overlay (`Render_OverlayText`) реализован, но в demo не
  включён: ~156 команд на строку → ~20 FPS; перенесён в S31–32.
- VDP2-фон/hello_triangle/3D-контент — S29–32.

## 13. VDP2: NBG1 bitmap background (S29–30)

> План: [`plans/sprint_29_30.md`](plans/sprint_29_30.md). Модуль:
> [`../src/hal/saturn/vdp2_backend.h`](../src/hal/saturn/vdp2_backend.h).

### 13.1. Карта VDP2 VRAM

| Область | Размер | Владелец |
|---|---:|---|
| NBG1 bitmap A0/A1 | 256 KiB | RPE background 1024×256, 8bpp |
| B1 (printf cell/map) | ~88 KiB | VDP2 log (NBG0) |
| прочее | остаток 512 KiB | Jo Engine/VDP2 |

Bitmap адресуется от `JO_VDP2_VRAM_A0` (`0x25E00000`), палитра — CRAM
блок 7 (`7×256..7×256+255`). VDP1 CLUT использует блоки 1..6; блок 7
зарезервирован за фоном.

### 13.2. Инициализация и кадр

1. `vdp2_backend_init()` → `jo_vdp2_set_nbg1_8bits_bitmap(7)` (1024×256)
   и порядок слоёв `NBG0 → VDP1 → NBG1 → RBG0 → NBG2 → NBG3`.
2. Загрузка `TEX1.TEX` (256×256 CI8+CLUT) → `vdp2_backend_upload_background()`:
   палитра в CRAM 7, тайлинг 4× по X через
   `jo_vdp2_draw_bitmap_nbg1_8bits_image()`.
3. `vdp2_backend_set_scroll(fixed x, fixed y)` только копит pending;
   `vdp2_backend_commit_vblank()` применяет `jo_vdp2_move_nbg1(px,px)` из
   VBlank-callback (детерминизм и отсутствие tearing).
4. `Render_Clear` при активном фоне ставит VDP1 `EWDR=0` (transparent):
   VDP2 NBG1 виден сквозь прозрачные пиксели, VDP1-куб — поверх.

### 13.3. Бюджет и baseline

| Метрика | Значение |
|---|---|
| NBG1 bitmap | 256 KiB VDP2 VRAM (50%) |
| Палитра 7 | 512 B CRAM |
| VDP1 visible commands | 6 (backface-culled cube) |
| VDP1 overdraw | 40‰ экрана (~2.9k px) |
| Scroll commits | ≈2/кадр (VBlank 60 Гц, game ~28–30 FPS) |
| Asset heap (transient) | 65 KiB (TEX1 до upload) |

Baseline: [`benchmarks/saturn_vdp2_s29_30.yaml`](benchmarks/saturn_vdp2_s29_30.yaml).
Политика: один NBG1-слой в v0.2.0, command list ≤ 511, overdraw demo
≤ 1000‰; переполнение VRAM/палитры — явный `false`/ERROR, silent drop
запрещён.

### 13.4. Ручной чек-лист S29–30

- [x] `saturn-release` стартует в Mednafen 1.32.1 (Japanese BIOS 1.01);
- [x] фон NBG1 виден за кубом, VDP1 поверх VDP2;
- [x] палитра/scroll не повреждены, scroll применяется в VBlank;
- [x] куб и фон статичны при отсутствии ввода (golden);
- [x] скриншоты `screenshots/saturn_vdp2_s29_30.png`,
      `screenshots/saturn_vdp2_scroll_s29_30.png`;
- [x] 10-минутный stress без tearing/hang (exit 124, f=17760 ≈29.6 FPS);
- [x] PC/PS1 сборки и host-тесты не регрессировали.

### 13.5. hello_triangle (S31–32)

Цель `hello_triangle_saturn` (алиас `retro_saturn`, байт-идентичные
артефакты `hello_triangle.*`):

```sh
cmake --preset saturn-release
cmake --build --preset saturn-release
mednafen -video.driver softfb -sound 0 -ss.bios_jp sega_101.bin \
  -ss.region_autodetect 0 -ss.region_default jp \
  build/saturn-release/src/hal/saturn/hello_triangle.cue
```

- Сцена: VDP2 NBG1 фон `TEX1.TEX` + VDP1 текстурированный Gouraud-куб
  `CUBE.MSH`/`TEX0.TEX`; Master+Slave получают по 16 треугольников
  workload каждый кадр (fallback — `saturn-single`).
- Управление 6-button pad: **D-pad** — yaw/pitch объекта; **A/B** —
  дистанция камеры; **C/X** — высота; **Y/Z** — орбита; **Start** —
  сброс. Без ввода сцена детерминирована (golden).
- Кадровый бюджет: 60 Гц (измерено 360 кадров / 6 с); VDP1 `c=6`,
  overdraw 40‰; VDP2 bitmap 256 KiB.
- Бюджеты/артефакты: [`benchmarks/saturn_hello_triangle_v0_2_0.yaml`](benchmarks/saturn_hello_triangle_v0_2_0.yaml);
  golden: `screenshots/hello_triangle_saturn_scene.png`
  (sha256 `b51b8847…`).
- Отличия от PC/PS1: 320×224 (crop 8), аффинные UV, triangle→quad,
  Painter's без Z-буфера; см. §12.6.

### 13.6. Ограничения S29–30

- Один NBG1-слой; параллакс/второй слой — post-v0.2.0.
- 8bpp bitmap 1024×256; source `.tex` тайлится, не масштабируется.
- Палитра фона фиксирована блоком 7; VDP1 CLUT 1..6.
- VDP2 log (NBG0) остаётся поверх VDP1; VDP1-overlay в demo не включён.
- Эмуляторный visual gate ручной; BIOS не вендорится.

## 14. Известные ограничения S23–30

- Нет запуска эмулятора в CI и нет BIOS в репозитории; S27–28 visual gate —
  ручной (pinned Mednafen 1.32.1).
- Нет PAL, 640×480, 3D pad, SCSP PCM, CD streaming/записи, мыши.
- VDP2-лог показывает последнюю строку (одна строка 27); история — только
  через эмуляторный debugger/скриншоты.
- VDP1: нет perspective-correct UV, Z-буфера, произвольных per-vertex UV;
  UV вне углов → flat; максимум 6 CLUT; 256-высокая текстура теряет
  последнюю строку. Детали — §12.6 и ADR-010.
- Command arena 16 KiB переиспользуется VDP1-списком (511 команд);
  переполнение — явный `false`/ERROR, не silent drop.
- Parallel workload S25–26 — clip-space prep без perspective divide/screen
  mapping; VDP1 command list и backface — S27–28. Slave в v0.2.0 не пишет
  VRAM (по ADR-009).
- Shared-регион S25–26 — 28 KiB reserved; текущие 3×128×8 B + batch
  ≈3.1 KiB. Увеличение workload требует пересчёта Work RAM-L ledger и
  benchmark-порога.
- `Render_TestReadbackPixels/Hash` на Saturn не работают (нет 307 KiB
  static RGBA); golden S23–30 — детерминированный цвет/`fb_hash` + ручной
  скриншот.
- VDP2: один NBG1 8bpp bitmap 1024×256 (256 KiB), палитра 7, без
  параллакса/второго слоя; scroll — целые пиксели из fixed 16.16.
- VDP1-overlay `Render_OverlayText` не включён в demo (156 команд → 20 FPS).
- 2048-ISO/CUE — capability pinned toolchain; raw 2352/CCD не выпускается.

## 15. Источники

### 15.1. Первоисточники Sega / Hitachi

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

### 15.2. Toolchain / эмулятор

- Jo Engine v2026.2: <https://github.com/johannes-fetz/joengine/releases/tag/v2026.2>
  (commit `5e99944238030821a48e17a40abefd77e81953c2`, MIT).
- Jo Engine site/docs: <https://www.jo-engine.org/>,
  <https://www.jo-engine.org/doxygen/>.
- Mednafen 1.32.1: <https://mednafen.github.io/releases/>;
  настройки Saturn — `src/ss/ss.cpp` (`ss.bios_na_eu`, `ss.bios_jp`,
  `ss.bios_sanity`, `ss.correct_aspect`, `ss.h_overscan`, `ss.h_blend`).

### 15.3. Secondary

- Rodrigo Copetti, *Sega Saturn Architecture*:
  <https://www.copetti.org/writings/consoles/sega-saturn/>
  (используется только для сверки/контекста; при расхождении приоритет у
  первоисточников Sega/Hitachi).
