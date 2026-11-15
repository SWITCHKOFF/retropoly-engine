# Ответы на бриф: эмулятор и SDK для RetroPolyEngine (Jaguar / Jaguar CD)

> **Статус:** инженерная оценка, **не юридическое заключение**. Atari
> Jaguar / Jaguar CD — **DEFERRED «до лучших времён»** ([ADR-016](../adr/ADR-016-jaguar-deferred.md))
> + **LEGAL HOLD**: документ отвечает на вопросы брифа и фиксирует связку
> на случай возобновления, но S47–52 не активируются. Перед
> возобновлением нужны явное личное разрешение maintainer начать Jaguar
> и юридическая проверка.
>
> **Дата среза:** 2026-11-15.
> **Бриф:** [`jaguar_emulator_sdk_brief.md`](jaguar_emulator_sdk_brief.md).
> **Аудит:** [`jaguar_legal_audit_v2.md`](jaguar_legal_audit_v2.md).
> **Hold:** [`jaguar_legal_hold.md`](jaguar_legal_hold.md).
>
> **Два ограничения, заданные maintainer:**
> 1. движок и поставляемый SDK остаются **MIT**;
> 2. **Atari Jaguar CD — основной target**, а не «фаза после картриджей».
>
> Поэтому ответы ниже — не выбор «лучшего готового кандидата», а проверка
> связки: **MIT-clean SDK + GPL-эмулятор как внешний процесс + собственный
> MIT CD-слой**. Ни один готовый кандидат не проходит все gate целиком.
>
> **Зафиксированное решение (2026-11-15, [ADR-016](../adr/ADR-016-jaguar-deferred.md)):**
> добавление поддержки платформы Atari Jaguar / Jaguar CD
> **откладывается «до лучших времён»**. Связка ниже зафиксирована как
> reference на случай возобновления; S47–52 не активируются, код,
> toolchain, ROM, CD-образы и payload-компоненты Jaguar не создаются.
> **Старт — только по явному личному разрешению maintainer начать Jaguar;
> снятие LEGAL HOLD само по себе не активирует.**
>
> **Связка:** VJ (GPL-3.0) — только внешний процесс, без линковки и без
> payload; SDK — MIT-профиль (`m68k-elf-gcc` + binutils + `lyxass` +
> собственный HAL/startup/linker + clean-room CD); CD — собственный MIT
> `jagcd` (boot track + `.cue/.bin` + manifest + size check).
> `jaguar-sdk`/Atari CD BIOS, `rmac`/`rln`, `cdi-maker` — не вендорить.

---

## 0. Короткий вывод

1. **Готового MIT/BSD-эмулятора Jaguar CD нет.** Лучший открытый
   CD-эмулятор — `libretro/virtualjaguar-libretro` (GPL-3.0; master
   `8543bbf`, README: v3.9.0): HLE CD по умолчанию, реальный BIOS опционально,
   `.cue/.bin/.cdi/.chd`, Memory Track, CD boot matrix, headless-режим,
   FNV-хэш кадра, GDB stub. Его можно использовать **только как внешний
   процесс**; в payload не включать.
2. **Появился MIT-эмулятор `HelgeSverre/jaguar` (`vjag`, MIT, commit
   `d327cd5`)**, но он **не умеет Jaguar CD**, save states и boot ROM.
   Это стратегический MIT-трек на будущее, не решение для v0.4.0.
3. **Готового MIT-clean Jaguar CD SDK нет.** `jaguar-sdk` без top-level
   LICENSE и с Atari CD BIOS/сканами; `jaglib` MIT, но `jagcore.h` —
   Atari «all rights reserved»; `rmvlib` LGPL-2.1; `rmac`/`rln` без
   лицензии; `cdi-maker` без LICENSE. Для CD нужен собственный
   clean-room boot/authoring-слой.
4. **Jaguar CD — главный блокер, а не второстепенная фаза.** Эмуляторная
   часть CD готова (VJ), SDK-часть CD — нет. Поэтому CD gate —
   **conditional/fail** до появления собственного MIT-слоя.
5. **Юридический gate — conditional.** У VJ есть red flags: встроенные
   Atari boot ROM/CD BIOS без отдельного разрешения — A06 и L08 (`[X]`),
   A12 (`[M]`, нет документированного разрешения). Они снимаются только
   одним из двух путей:
   - **(A)** VJ используется как внешний, пользователем установленный
     инструмент, не распространяется в payload; maintainer письменно
     принимает остаточный риск;
   - **(B)** собирается HLE-only форк VJ без BIOS-массивов и
     распространяется как отдельный GPL-3.0 процесс с source offer.
   Путь (B) — единственный, который делает *поставляемый* эмулятор
   BIOS-clean; его нужно проверить технически (D09).
6. **Зафиксированная связка ([ADR-016](../adr/ADR-016-jaguar-deferred.md)):**
   **VJ external + MIT-профиль SDK (`m68k-elf-gcc` + binutils + `lyxass`
   + собственный HAL/startup/linker + clean-room CD)**; `vjag` — MIT-трек
   на будущее; MAME — только трассировка; BigPEmu — только локальный
   необязательный ручной gate; commercial use ограничен ReadMe, в payload
   не включать.
7. **Статус: DEFERRED — «до лучших времён».** Связка зафиксирована, но
   S47–52 не активируются, Jaguar-код/toolchain/ROM/CD/payload не
   создаются. Возобновление — только по **явному личному разрешению
   maintainer начать Jaguar** и после условий §6. Снятие legal hold или
   закрытие лицензионных вопросов само по себе старт не активирует.

---

## 1. Кандидаты и режим использования

| ID | Кандидат | Версия/commit | Лицензия | Режим | Роль |
|---|---|---|---|---|---|
| **E1** | `libretro/virtualjaguar-libretro` | master `8543bbf`; README: v3.9.0 | **GPL-3.0** | external process, не линковать | основной CD-эмулятор для gate |
| **E2** | `HelgeSverre/jaguar` (`vjag`) | `d327cd5` | **MIT** | external process или linked (MIT) | cartridge-only fallback; стратегический MIT CD-трек |
| **E3** | MAME (`jaguar.cpp`) | `f52a78c` | **GPL-2.0** как целое; драйвер BSD-3 | external, trace only | отладка/трассировка, не acceptance |
| **E4** | BigPEmu | 1.221 | **custom restrictive** (ReadMe: no private financial gain, no modified exe, no redistribution without document) | external, optional manual | локальный ручной gate при письменном принятии риска; commercial use ограничен, не payload |
| **S1** | MIT-профиль: `m68k-elf-gcc` + binutils + `lyxass` + свой HAL/startup/linker + clean-room CD | GCC + `lyxass 1d67d1a` | **MIT/GPL-3.0 tools; output non-GPL** | build tool | **основной SDK** |
| **S2** | `toarnold/jaglib` | `b06b3fb` | **MIT**, кроме `jagcore.h` (Atari) | optional library | HAL-заготовка после замены заголовка |
| **S3** | `theRemovers/rmvlib` | `164026e` | **LGPL-2.1** + Atari `jaguar.inc` | optional, не default | спрайты/blitter/sound, только с LGPL-комплаенсом |
| **S4** | `cubanismo/jaguar-sdk` | `b806b6f` | **нет top-level LICENSE**; Atari samples/docs/CD BIOS | env-install only | legacy-референс, не вендорить |
| **C1** | собственный `jagcd` (boot track + cue/bin + manifest + size check) | план | **MIT** | build tool | **основной CD-authoring** |
| **C2** | `cubanismo/cdi-maker` | `324ef81` | **нет LICENSE** | optional external | `.cdi` только после разрешения автора или замены |

**Почему не выбран «чистый MIT» эмулятор:** `vjag` MIT, но в README
прямо перечислено, чего нет: **Jaguar CD**, save states, input recording,
boot ROM. Для CD-first v0.4.0 он не проходит B10–B13. Использовать его
для CD можно только после крупного собственного вклада в CD-контроллер
и HLE boot — это отдельный исследовательский трек, а не зависимость.

**Почему не BigPEmu:** технически лучший, но:
- исходников нет (FAQ: contractual obligations);
- ReadMe запрещает private financial gain и распространение изменённого
  executable;
- нет macOS-сборки (только Windows x64/ARM64 и Linux x64/ARM64);
- нет документированного headless/screenshot/frame-limit CLI;
- в поставке нет основного EULA-файла, только third-party licenses.
Для MIT-проекта и commercial-игр — **не кандидат**.

---

## 2. Коды доказательств

Ответ без доказательства по правилам брифа считался бы «Не знаю»;
поэтому ниже — коды, на которые ссылаются таблицы.

| Код | Источник | Что подтверждает |
|---|---|---|
| **VJ** | <https://github.com/libretro/virtualjaguar-libretro> (master `8543bbf`, v3.9.0) | LICENSE GPL-3.0, README, CD, HLE, Memory Track, save states, GDB, input |
| **VJ-BIOS** | <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/bios.md> | BIOS встроены; HLE по умолчанию; external override |
| **VJ-CD** | <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/cd-known-issues.md>; <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/cd-boot-matrix.md> | CD boot matrix, known issues, `.cue/.bin/.cdi/.chd`, `.iso` нет |
| **VJ-APP** | <https://github.com/libretro/virtualjaguar-libretro/blob/master/app/README.md> | `--headless`, `--frames`, `--screenshot`, `--data-dir`, FNV hash, smoke tests |
| **VJ-MAIN** | <https://github.com/libretro/virtualjaguar-libretro/blob/master/app/src/main.cpp> | CLI, `--help`, exit 2 на ошибках, `--expect-hash`, `--cd-bios` |
| **VJ-H** | <https://github.com/libretro/virtualjaguar-libretro/blob/master/test/harness/harness.c>; <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/agent/testing.md> | `--frames`, `--press`, `--json`, `--option`, `--bios` |
| **VJ-GDB** | <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/gdb-stub-guide.md> | GDB RSP, breakpoints, memory, 68000/GPU/DSP |
| **VJ-SEC** | <https://github.com/libretro/virtualjaguar-libretro/blob/master/SECURITY.md> | security policy, SHA256SUMS, minisign, SLSA, signed releases |
| **VJ-CI** | <https://github.com/libretro/virtualjaguar-libretro/blob/master/.github/workflows/c-cpp.yml> | CI, тесты, сборки |
| **VJAG** | <https://github.com/HelgeSverre/jaguar> (`d327cd5`) | LICENSE MIT; README: нет CD/save states/boot ROM; `docs/headless.md` |
| **MAME** | <https://github.com/mamedev/mame/blob/master/src/mame/atari/jaguar.cpp> (`f52a78c`) | `jaguar`/`jaguarcd` = `MACHINE_NOT_WORKING`; BIOS `jag_cd.bin`/`jagdevcd.bin` |
| **BIG** | BigPEmu 1.221 `ReadMe.txt` + <https://www.richwhitehouse.com/jaguar/> + FAQ/download | closed source; restrictive terms; Windows/Linux only; нет headless |
| **JSDK** | <https://github.com/cubanismo/jaguar-sdk> (`b806b6f`) | нет LICENSE; Atari samples/docs; `cdbios45.*`, `cdboot.rom` |
| **LYX** | <https://codeberg.org/42Bastian/lyxass> (`1d67d1a`) | MIT; GPU/DSP assembler |
| **JAGLIB** | <https://github.com/toarnold/jaglib> (`b06b3fb`) | MIT; `jagcore.h` Atari all rights reserved; `malloc`; нет CD |
| **RMV** | <https://github.com/theRemovers/rmvlib> (`164026e`) | LGPL-2.1; Atari `jaguar.inc` |
| **NEWBJL** | <https://github.com/42Bastian/new_bjl> (`443e37c`) | Unlicense/public domain; BJL, не CD |
| **CDIM** | <https://github.com/cubanismo/cdi-maker> (`324ef81`) | `.cdi` assembler; LICENSE отсутствует |
| **GCC** | <https://gcc.gnu.org/onlinedocs/gcc/M680x0-Options.html>; <https://www.gnu.org/licenses/gcc-exception-3.1.html>; <https://www.gnu.org/licenses/gpl-faq.html#WhatCaseIsOutputGPL> | m68k big-endian/`-m68000`/soft-float; output non-GPL |
| **CDDOC** | <https://asterick.github.io/jag-docs/cdrom/bios-api.html>; <https://asterick.github.io/jag-docs/cdrom/programming-guide.html> | CD BIOS API, boot track, TOC at `$2C00`, read flow |
| **RPE** | `LICENSE`, `LEGAL.md`, `docs/adr/ADR-013-release-freeze.md`, `docs/RELEASING.md`, `CMakePresets.json` | MIT проекта, release freeze, payload policy, presets |
| **AUDIT** | `docs/plans/jaguar_legal_audit_v2.md` | юридический разбор GPL/BIOS/headers, варианты A–G |
| **HOLD** | `docs/plans/jaguar_legal_hold.md` | условия снятия hold, политика env-install |

---

## 3. Ответы по секциям

Формат: **ID · тег · ответ · комментарий/доказательство**.
Полные 183 ответа даны для зафиксированной связки **E1 + S1 + C1**;
альтернативы оценены по блокирующим пунктам в §4.2, чтобы не дублировать
183×N таблиц. «Н/П» — вопрос не применим к Jaguar (например, Expansion
Pak).

### A. Лицензии, юридические риски, MIT-совместимость

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| A01 | [M] | **Да** | MIT-движок, SDK и UI остаются MIT; GPL-эмулятор — отдельный процесс по CLI, не линкуется. `AUDIT` §5.1; `RPE` LICENSE. |
| A02 | [M] | **Частично** | У VJ есть GPL-3.0 `LICENSE`; у `lyxass` MIT; у `jaglib` MIT (кроме `jagcore.h`); у `rmvlib` LGPL-2.1 (+ Atari `jaguar.inc`). Но `jaguar-sdk`, `rmac`/`rln`, `cdi-maker` — без явной лицензии; third-party deps VJ (libchdr/zstd/lzma/miniz) надо проверить по NOTICE. В поставляемом MIT-профиле — только компоненты с явной лицензией. `JSDK`, `CDIM`, `AUDIT` §3. |
| A03 | [X] | **Нет** | GPL/LGPL-код не линкуется в engine/UI/runtime SDK. VJ — external; `rmvlib` — не default. `AUDIT` §5.1. |
| A04 | [M] | **Да** | MIT SDK и GCC-output разрешают commercial use; GPL-эмулятор не претендует на output игры. `GCC` (GPL FAQ, RLE); `RPE` LICENSE. Ограничения Atari IP на BIOS/ROM — отдельно (A12). |
| A05 | [M] | **Частично** | Наш MIT SDK можно распространять в payload; VJ — нельзя включать без GPL/BIOS-разбора. Политика: эмулятор отсутствует в payload (J03). `RPE` `docs/RELEASING.md`; `AUDIT` §5.1. |
| A06 | [X] | **Да (VJ и JSDK); Нет у MIT-профиля** | VJ содержит встроенные Atari boot ROM/CD BIOS (`VJ-BIOS`); `jaguar-sdk` — `cdbios45.*`, `cdboot.rom`, Atari samples (`JSDK`). Поэтому VJ/JSDK не проходят как распространяемый компонент; только external, либо HLE-only форк без BIOS-массивов. Это главный red flag. |
| A07 | [M] | **Да** | Режим «отдельный процесс» разрешён и выбран; VJ запускается по CLI, не линкуется. `AUDIT` §5.1; `VJ-APP`. |
| A08 | [M] | **Да** | VJ собирается из исходников без проприетарных зависимостей и регистрации (`VJ`); MIT-профиль — GCC/binutils/`lyxass` (`GCC`, `LYX`). `jaguar-sdk` — нет: DOS-бинарники, Atari docs/BIOS (`JSDK`). |
| A09 | [M] | **Да** | MIT/GPL build tools не претендуют на ROM/ассеты/output; GCC RLE прямо разрешает non-GPL Target Code. `GCC`; `RPE` LICENSE. |
| A10 | [M] | **Да** | При распространении GPL-эмулятора можно выполнить GPL-3.0: исходники, текст лицензии, source offer. Но выбран режим «не распространять». `VJ` LICENSE; `AUDIT` §5.1. |
| A11 | [X] | **Нет** | VJ, `vjag`, GCC, `lyxass` — без подписки/аккаунта/активации. BigPEmu — freeware, но с ограничениями (A14), не выбран. `VJ`, `VJAG`, `BIG`. |
| A12 | [M] | **Нет** | Документированного разрешения Atari на встроенные BIOS не обнаружено; VJ встраивает образы без отдельной лицензии (`VJ-BIOS`). Для HLE-only пути BIOS не нужен; для real-BIOS — только BIOS пользователя, не наш payload. |
| A13 | [M] | **Да** | MIT/BSD attribution и GPL notices можно выполнить, не раскрывая наш код: наш код MIT, GPL — только у внешнего компонента. `VJ` LICENSE; `RPE` `LEGAL.md`. |
| A14 | [M] | **Да (для MIT/GPL); Нет (BigPEmu)** | У MIT/GPL нет запрета commercial use/no-derivatives. BigPEmu ReadMe: «I forbid ... private financial gain», запрет modified exe и распространения без документа (`BIG`). BigPEmu отклонён. |
| A15 | [M] | **Частично** | Nominative use «Atari Jaguar / Jaguar CD» в документации и именах файлов допустим с оговоркой о товарных знаках, но это юридический вопрос, не лицензионный грант. Нужна проверка юриста; в docs — descriptive use + disclaimer. `RPE`; `AUDIT` §5. |
| A16 | [S] | **Не знаю** | Письменной позиции Atari/Hasbro по BIOS/заголовкам не обнаружено; `jaguar-sdk` без LICENSE, VJ без разрешения на BIOS. Нужен запрос/юридическая оценка. `JSDK`; `VJ-BIOS`. |

### B. Эмулятор: корректность Jaguar и Jaguar CD (E1, VJ)

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| B01 | [M] | **Да** | 68000 big-endian, исключения, прерывания; ядро `src/m68000`, GDB видит 68000. `VJ`, `VJ-GDB`. |
| B02 | [M] | **Да** | Tom GPU с локальной RAM 4 KiB; GPU emulated, GPU-only carts. `VJ`. |
| B03 | [M] | **Да** | Jerry DSP с локальной RAM 8 KiB; DSP emulated. `VJ`. |
| B04 | [M] | **Да** | Object Processor: object list, bitmap/branch, 1/2/4/8/16/24 bpp; 24bpp есть в `src/tom/op.c`. `VJ`; `VJ` `src/tom/op.c`. |
| B05 | [M] | **Да** | Blitter, 16-bit Z-buffer, Gouraud; SIMD blitter, true-color Gouraud option. `VJ` README; `VJ` `src/tom/blitter.c`. |
| B06 | [M] | **Да** | 2 MiB DRAM, cart map `$800000–$DFFFFF`, entry `$802000`; GDB boot PC `0x802000`. `VJ-GDB`; `VJ` memory map. |
| B07 | [M] | **Да** | 320×240 RGB16/CRY, NTSC ~60 Hz; PAL option 50 Hz. `VJ` README; libretro docs `virtualjaguar_pal`. |
| B08 | [M] | **Да** | VBlank, PIT/таймеры, IRQ Tom/Jerry; timing model and tests. `VJ` README; `VJ-H`. |
| B09 | [M] | **Да** | Оба порта, D-pad, A/B/C, Option/Pause, keypad, Team Tap, rotary, mouse, light gun. `VJ` README; `VJ-APP`. |
| B10 | [M] | **Да** | CD controller BUTCH, TOC, data read; CD stack `src/cd`. `VJ` README; `VJ-CD`. |
| B11 | [M] | **Да** | Jaguar CD boot через HLE (default) или real BIOS (optional). `VJ-BIOS`. |
| B12 | [M] | **Да** | Memory Track save/load; `.srm`. `VJ` README; `VJ-BIOS`. |
| B13 | [M] | **Да** | `.cue` + `.bin` — основной формат; `.iso` не поддерживается. `VJ` README; `VJ-CD`. |
| B14 | [S] | **Да** | `.cdi` и `.chd` поддерживаются; CHD требует свежий `chdman`. `VJ` README; `VJ-CD`. |
| B15 | [S] | **Да** | PAL и переключение NTSC/PAL (`virtualjaguar_pal`). libretro docs; `VJ`. |
| B16 | [S] | **Да** | External real-BIOS override (`jagboot.rom`, `jagboot_m.rom`, CD BIOS file). `VJ-BIOS`. |
| B17 | [S] | **Да** | Known issues CD и boot matrix документированы. `VJ-CD`. |
| B18 | [S] | **Частично** | CD boot matrix и harness есть, но публичный CI не гоняет copyrighted ROMs; matrix — локальный regression gate с приватным корпусом. `VJ-CD`; `VJ-CI`. |
| B19 | [M] | **Да** | Homebrew без пользовательского BIOS: HLE по умолчанию. `VJ-BIOS`. |
| B20 | [S] | **Да** | CD audio и VLM; audio-only Red Book идёт через real BIOS. `VJ` README; `VJ-BIOS`. |

### C. Эмулятор: автоматизация, наблюдаемость, воспроизводимость

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| C01 | [M] | **Да** | CLI с `--help`, стабильными длинными опциями (`--frames`, `--screenshot`, `--cd-bios`, `--option`). `VJ-MAIN`; `VJ-APP`. |
| C02 | [M] | **Да** | `--headless` без окна/GL, SDL dummy video/audio; работает под Xvfb/CI. `VJ-APP`. |
| C03 | [M] | **Частично** | Ошибки аргументов — exit 2; `--expect-hash` даёт fail; crash watchdog пишет сигнатуру. Но таймаут/зависание надо оборачивать внешним `timeout` и проверять код. `VJ-MAIN`; `VJ-APP`. |
| C04 | [M] | **Да** | `--frames N` завершает прогон; `--perf-frames` для метрик. Для wall-clock timeout — внешний wrapper. `VJ-MAIN`; `VJ-APP`. |
| C05 | [M] | **Да** | `--screenshot PNG`, `--screenshot-frame` (нужен GL/окно); headless-путь — miniretro `--dump-frames-every` и `cd_visual_verify` PPM. `VJ-APP`; `VJ-H`; `VJ` `test/regression_test.sh`. |
| C06 | [M] | **Да** | FNV-1a frame hash (`--expect-hash`), memory dump/`--dump`, harness JSON. `VJ-APP`; `VJ-H`. |
| C07 | [M] | **Да** | Scripted input: harness `--press FRAME:BUTTON[:HOLD]`, app `--press`, `--press-file`; keypad. `VJ-H`; `VJAG` headless. |
| C08 | [M] | **Да** | Save/load states, state roundtrip, per-game states. `VJ` README; `VJ-APP`. |
| C09 | [M] | **Да** | Memory read/write, breakpoints, trace, watchpoints; GDB stub; debugger windows. `VJ-GDB`; `VJ` README. |
| C10 | [S] | **Да** | GDB Remote Serial Protocol для 68000; GPU/DSP через `monitor`; LLDB 68K не умеет. `VJ-GDB`. |
| C11 | [M] | **Частично** | Логи и JSON harness стабильны на практике, но формальная schema/versioning не заявлены. `VJ-H`. |
| C12 | [M] | **Да** | `--data-dir`, per-run config, portable app; нет глобального состояния при изоляции. `VJ-APP`. |
| C13 | [M] | **Да** | Headless dummy audio/video; harness `--av-skip-video`; можно отключить звук. `VJ-H`; `VJ-APP`. |
| C14 | [M] | **Да** | Linux x64 сборки/CI; AppImage/deb; контейнер. `VJ` release matrix; `VJ-CI`. |
| C15 | [S] | **Да** | Несколько экземпляров с разными `--data-dir`; файлы saves/config раздельно. `VJ-APP`; `VJAG` data dir. |
| C16 | [M] | **Да** | Полностью offline; сеть нужна только для опциональных RA/thumbnails/netplay. `VJ` README; `VJ-SEC`. |
| C17 | [S] | **Да** | Harness `--json`; `vjag_test --report-json`; CD matrix генерирует Markdown/логи. `VJ-H`; `VJAG` `docs/headless.md`. |

### D. Эмулятор: поставка, сборка, эксплуатация

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| D01 | [M] | **Да** | GPL допускает внутреннее использование в CI; распространение не требуется. `VJ` LICENSE; `AUDIT` §5.1. |
| D02 | [M] | **Да** | Лицензия позволяет рекомендовать VJ как внешний инструмент пользователя; UI хранит только профиль запуска. `AUDIT` §5.1; `RPE` `docs/UI_CLIENT.md`. |
| D03 | [X] | **Нет** | Наш repo/payload не требует BIOS/ROM: HLE default, BIOS — optional external. Но сам VJ содержит встроенные BIOS (A06), поэтому не включаем его в payload. `VJ-BIOS`; `RPE` policy. |
| D04 | [M] | **Да** | Исходный код VJ доступен (GPL-3.0). `VJ` LICENSE. |
| D05 | [M] | **Частично** | Сборка из исходников воспроизводима как процесс; bit-for-bit reproducible binary не заявлен. Release provenance/SLSA есть, но это не reproducibility. `VJ-SEC`; `VJ-CI`. |
| D06 | [S] | **Да** | `SHA256SUMS.txt` + minisign, SLSA attestations, macOS notarization, Linux AppImage GPG. `VJ-SEC`. |
| D07 | [M] | **Да** | Linux x64, Windows x64, macOS (Apple Silicon/Intel), плюс ARM64/др. `VJ` README; release matrix. |
| D08 | [M] | **Да** | Теги/релизы, nightly, release notes; можно зафиксировать commit `8543bbf`. `VJ`; `VJ-SEC`. |
| D09 | [S] | **Частично** | HLE — default, но BIOS-массивы встроены; отдельной опции «собрать без BIOS» не задокументировано. Нужен форк/upstream patch (VJ issue/PR). `VJ-BIOS`; `AUDIT` §6 вариант B. |
| D10 | [M] | **Да** | Нет драйверов/админ-прав: portable zip/AppImage/deb. `VJ` README; `VJ-APP`. |
| D11 | [S] | **Да** | `--data-dir`, portable config, Windows portable zip. `VJ-APP`. |
| D12 | [X] | **Нет** | Драйверы/kernel extensions не устанавливаются. `VJ` README. |

### E. SDK: toolchain и сборка (S1)

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| E01 | [M] | **Да** | `m68k-elf-gcc` поддерживает 68000 ISA и big-endian (`-m68000`, `-mbig-endian`). `GCC`. |
| E02 | [M] | **Да** | `-msoft-float`/no-FPU; движок Q16.16. `GCC`; `RPE` math. |
| E03 | [M] | **Частично** | Свой linker script/ROM sections/vectors/BSS — план S47–48; у `jaglib` нет `.ld`, у `jaguar-sdk` — Atari startup/scripts. `JAGLIB`; `JSDK`; `AUDIT` §6 вариант C. |
| E04 | [M] | **Частично** | ROM header/checksum/entry generator — свой; готового MIT-инструмента нет. `AUDIT` §6; `JSDK` (Atari header как справка, не копировать). |
| E05 | [M] | **Да** | `lyxass` (MIT) — GPU/DSP assembler; 68k — GNU as/GCC. `LYX`; `GCC`. |
| E06 | [M] | **Частично** | Overlays/локальные 4 KiB GPU и 8 KiB DSP: runtime loader планируется; `jaglib` умеет `jag_gpu_load`/`jag_dsp_load`. `JAGLIB`; план S49–50. |
| E07 | [M] | **Частично** | Детерминированная сборка достижима (`SOURCE_DATE_EPOCH`, `-ffile-prefix-map`, сортировка), но CI-тест «same input → same ROM» ещё не сделан. `GCC`; план. |
| E08 | [M] | **Частично** | Timestamp/absolute path/random seed надо вычищать флагами и своими tools; тест в CI — план. `GCC`; план. |
| E09 | [M] | **Частично** | CMake/Ninja toolchain-файл и preset `jaguar-release` — план S47–48; сейчас есть только N64/Saturn/PS1 presets. `RPE` `CMakePresets.json`; план. |
| E10 | [M] | **Частично** | У GCC/`lyxass` есть CLI/exit codes; свой wrapper/диагностика SDK — план. `GCC`; `LYX`. |
| E11 | [M] | **Частично** | Linux-host — базовый сценарий; Windows/macOS host требуют cross-toolchain/CI-матрицы, которая ещё не сделана. `GCC`; `LYX`; план. |
| E12 | [M] | **Да** | User-local/env install без root; toolchain в `PATH`, SDK в user dir. `RPE` BUILDING; план. |
| E13 | [M] | **Да** | GNU ld `-Map`, ELF symbols; map/symbols для анализа размера. `GCC` binutils. |
| E14 | [M] | **Да** | GNU as `.incbin`; ассеты в ROM. `GCC` binutils. |
| E15 | [M] | **Частично** | Примеры из чистого checkout одной командой — план; `jaguar-sdk` это умеет, но не MIT-clean. `JSDK`; план. |
| E16 | [S] | **Да** | `-g` + GDB stub/m68k ELF; source-level debug. `VJ-GDB`; `GCC`. |
| E17 | [S] | **Да** | clang-format/static analysis/warnings-as-errors — политика проекта и CI. `RPE` `.clang-format`, CI. |
| E18 | [M] | **Да** | Проприетарная IDE/ключ не нужны: GCC/binutils/`lyxass`/CMake. `GCC`; `LYX`. |

### F. SDK: runtime, HAL и ограничения движка

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| F01 | [M] | **Частично** | Startup/reset/vector table/stack — свой; `jaglib` имеет `libmain`, `jaguar-sdk` — Atari startup. `JAGLIB`; `JSDK`; план S47–48. |
| F02 | [M] | **Частично** | Video init/object list/320×240 RGB16: `jaglib` даёт console/video/OL; CD нет. `JAGLIB`; план S49–50. |
| F03 | [M] | **Частично** | Blitter helpers/register API: `jaglib` blitter/wait; Z/Gouraud 16-bit — свой слой. `JAGLIB`; план. |
| F04 | [M] | **Да** | Input API двух портов: `jag_read_stick0/1`, directions; свой wrapper. `JAGLIB`. |
| F05 | [M] | **Частично** | Timers/VBlank/IRQ API: `jaglib` interrupt handlers; свой HAL — план. `JAGLIB`; план. |
| F06 | [M] | **Частично** | Memory map/ledger 2 MiB — проектируется; готового MIT SDK нет. `RPE` budgets; план. |
| F07 | [M] | **Частично** | Safe asset loading (bounds/CRC/rollback) — свой слой; план. `RPE` ASSETS; план. |
| F08 | [M] | **Частично** | `jaglib` использует `malloc`; для no-heap runtime берём только нужные части/переписываем. `JAGLIB` `libmain.c`; план. |
| F09 | [M] | **Да** | Fixed-point Q16.16, no-FPU; компилятор soft-float. `RPE` math; `GCC`. |
| F10 | [M] | **Частично** | Детерминированный frame loop с фиксированным шагом — свой; движок уже fixed-step. `RPE`; план. |
| F11 | [M] | **Частично** | Debug overlay/log без emulator-only хаков — свой; VJ даёт GDB/лог. `VJ-GDB`; план. |
| F12 | [M] | **Да** | Host-only код не попадает в target: CMake-разделение host/target, проверки. `RPE` CMake; план. |
| F13 | [M] | **Частично** | Static check stack/heap/size — план CI (`size`, stack analysis). `RPE` budgets; план. |
| F14 | [M] | **Частично** | HAL совместим с ECS/renderer абстракциями — план адаптера; кода Jaguar ещё нет. `RPE` ARCHITECTURE; план. |
| F15 | [M] | **Частично** | Runtime в бюджет 2 MiB DRAM с reserve — план ledger; нет измерений. `RPE` budgets; план. |
| F16 | [M] | **Да** | Runtime-библиотеки MIT/BSD/Apache/ISC/Unlicense: свой код MIT, `jaglib` MIT после замены `jagcore.h`; LGPL не default. `JAGLIB`; `RPE` LICENSE. |
| F17 | [X] | **Нет** | В runtime нет Atari-заголовков/BIOS/проприетарных libs: `jagcore.h`/`jaguar.inc` не вендорятся, заменяются clean-room. `JAGLIB`; `AUDIT` §3.1. |
| F18 | [S] | **Частично** | Audio stub/API: `jag_audio_mute`, DSP; полноценный audio — v0.6+. `JAGLIB`; план. |
| F19 | [S] | **Н/П** | У Jaguar нет Expansion Pak/8 MiB; 2 MiB DRAM фиксированы. Вопрос скопирован из N64-контекста. `RPE` budgets. |
| F20 | [S] | **Частично** | `hello_triangle`/`rotating_cube` — план S51–52; пока нет Jaguar-примеров. `RPE` sprint plan; план. |

### G. SDK: Jaguar CD authoring и runtime (C1)

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| G01 | [M] | **Частично** | Сборка bootable CD из исходников/ассетов — свой `jagcd` (boot track + cue/bin); `cdi-maker` без LICENSE, `jaguar-sdk` не MIT. `CDIM`; `JSDK`; план S47–48. |
| G02 | [M] | **Частично** | TOC/directory/boot sector: формат boot track/TOC документирован; свой writer — план. `CDDOC`; план. |
| G03 | [M] | **Частично** | Jaguar CD filesystem/ISO9660: data track + boot track; свой слой — план. `CDDOC`; план. |
| G04 | [M] | **Частично** | Boot loader/boot code generation — свой clean-room stub; `cdboot.rom` — Atari binary, не использовать. `JSDK`; план. |
| G05 | [M] | **Частично** | Bindings/API `CD_init`, `CD_read`, `CD_getoc`, `CD_switch`: wrappers по документированным адресам/регистрам, без копирования Atari `CD.INC`. `CDDOC`; `JSDK` readme; план. |
| G06 | [M] | **Частично** | Memory Track save/load: эмулятор поддерживает; SDK API — план. `VJ` README; план. |
| G07 | [M] | **Частично** | Multi-disc/`CD_switch`: эмулятор поддерживает multi-disc; SDK wrapper — план. `CDDOC`; `VJ` README; план. |
| G08 | [S] | **Частично** | CD audio/XA: эмулятор CD-DA/VLM; SDK API — план. `VJ` README; `CDDOC`; план. |
| G09 | [M] | **Частично** | Детерминированный CD image output — свой writer; тест reproducibility — план. `CDIM` (beta); план. |
| G10 | [M] | **Да** | `.cue/.bin` совместим с VJ; `.iso` не поддерживается. `VJ` README; `VJ-CD`. |
| G11 | [M] | **Да** | BIOS-политика: пользовательский BIOS/HLE, без бандлинга; наш payload BIOS не содержит. `VJ-BIOS`; `HOLD`. |
| G12 | [M] | **Частично** | Контроль размера образа/бюджета — свой `jagcd` (manifest + limits); план. `RPE` budgets; план. |
| G13 | [M] | **Частично** | Streaming/read-speed API: CD BIOS read flow и модель скорости документированы; SDK wrapper — план. `CDDOC`; `VJ` README; план. |
| G14 | [M] | **Частично** | Упаковка ассетов/манифест для CD — свой формат; план. `RPE` ASSETS; план. |
| G15 | [M] | **Частично** | `hello CD`/загрузка с CD — план S51–52; пока нет. `RPE` sprint plan; план. |
| G16 | [X] | **Нет** | Наш MIT SDK не включает Atari CD BIOS и не требует его распространения. `jaguar-sdk` — включает (`JSDK`), поэтому не используется. `HOLD`; `RPE` policy. |
| G17 | [X] | **Нет** | Проприетарный/нелицензированный toolchain не требуется: GCC/binutils/`lyxass`/свой `jagcd`. `GCC`; `LYX`; план. |
| G18 | [S] | **Частично** | `.chd/.cdi` — эмулятор да; SDK primary `.cue/.bin`; `.cdi` — только после лицензии `cdi-maker` или свой writer. `VJ` README; `CDIM`; план. |

### H. SDK: лицензии и supply chain

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| H01 | [M] | **Да** | Линкуемые библиотеки MIT/BSD/Apache/ISC/Unlicense: свой runtime MIT, `jaglib` MIT после замены `jagcore.h`; GPL/LGPL не линкуются. `JAGLIB`; `RPE` LICENSE. |
| H02 | [X] | **Нет** | GPL/LGPL runtime в target нет; `rmvlib` LGPL — только optional/non-default, с комплаенсом. `RMV`; `AUDIT` §3.1. |
| H03 | [M] | **Частично** | SPDX-заголовки и единый license manifest — надо добавить для Jaguar SDK; у VJ GPL-3.0. `RPE` `LEGAL.md`; план. |
| H04 | [M] | **Частично** | THIRD_PARTY/NOTICE с полными текстами — план; сейчас есть общий `LEGAL.md`. `RPE` `LEGAL.md`; план. |
| H05 | [M] | **Да (механизм)** | Если GPL-инструменты поставляются, source offer делается; мы их не поставляем (env-install). `GCC`; `HOLD`. |
| H06 | [X] | **Нет** | Atari-код/заголовки в MIT-профиле отсутствуют: `jagcore.h`/`jaguar.inc` заменяются clean-room. `JAGLIB`; `RMV`; `AUDIT` §3.1. |
| H07 | [X] | **Нет** | BIOS/ROM в SDK/payload нет; только пользовательский BIOS/HLE. `HOLD`; `VJ-BIOS`. |
| H08 | [M] | **Да — ограничений нет** | Royalty/ограничений на commercial игры нет: MIT SDK, GCC output non-GPL. `GCC`; `RPE` LICENSE. |
| H09 | [M] | **Не знаю** | Export-control/trademark аспекты не проверены; nominative use требует юридической оценки. `AUDIT` §5; план legal review. |
| H10 | [M] | **Частично** | У VJ есть `SECURITY.md`; у нашего SDK — надо создать. `VJ-SEC`; план. |
| H11 | [S] | **Частично** | VJ подписан (minisign/SLSA); SDK-артефакты — план подписи. `VJ-SEC`; план. |
| H12 | [M] | **Да** | GCC/binutils/`lyxass` maintained/доступны; `cdi-maker` beta не используем, `rmac/rln` не вендорим. `GCC`; `LYX`; `CDIM`. |

### I. Интеграция в RetroPolyEngine

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| I01 | [M] | **Да** | Fixed-point Q16.16/no-FPU политика движка; GCC soft-float. `RPE` math; `GCC`. |
| I02 | [M] | **Да** | Обязательного soft-float/FPU в hot path нет; fixed-point. `RPE` math. |
| I03 | [M] | **Частично** | В target runtime нет heap/malloc hot path: свой no-heap HAL; `jaglib` malloc не используем в hot path. `JAGLIB`; план. |
| I04 | [M] | **Частично** | Ledger 2 MiB DRAM (asset/frame/command/stack/reserve) — план; нет измерений. `RPE` budgets; план. |
| I05 | [M] | **Да** | 320×240 RGB16; CRY/RGB16 на Jaguar. `VJ`; `RPE` renderer. |
| I06 | [M] | **Частично** | W0 (5 кейсов) и W1 (4 куба/48 tri) — план S51–52; Jaguar backend ещё не написан. `RPE` sprint plan; план. |
| I07 | [M] | **Да** | Framebuffer FNV hash и детерминированное сравнение; `--expect-hash`. `VJ-APP`; `VJ-H`. |
| I08 | [M] | **Частично** | FPS/frame time/память: harness JSON/timing probes; env metadata и SDK benchmarks — дополнить. `VJ-H`; `RPE` benchmarks. |
| I09 | [M] | **Частично** | Канонический CMake preset `jaguar-release` — план; сейчас нет. `RPE` `CMakePresets.json`; план. |
| I10 | [M] | **Да** | Host-часть на PC Linux/Windows/macOS: проект уже кросс-платформенный. `RPE` BUILDING. |
| I11 | [M] | **Частично** | Упаковка SDK/UI по release-контракту — план; Jaguar SDK ещё нет. `RPE` `docs/RELEASING.md`; план. |
| I12 | [M] | **Да** | Release freeze draft-only до v0.9.x соблюдается. `RPE` ADR-013. |
| I13 | [X] | **Нет** | BIOS/ROM/ключи/build-каталоги в payload не попадают; CI-проверка — план/политика. `RPE` release policy; `HOLD`. |
| I14 | [M] | **Да** | `.mesh`/`.tex` v1 без изменений; asset pipeline host-only. `RPE` ASSETS. |
| I15 | [M] | **Да** | Asset converter host-only, не MIPS/68k. `RPE` ASSETS; ARCHITECTURE. |
| I16 | [S] | **Частично** | Совместимость с PC/PS1/Saturn/N64 golden — план W0/W1 cross-backend; Jaguar ещё нет. `RPE` benchmarks; план. |

### J. CI, релиз, распространение

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| J01 | [M] | **Частично** | SDK должен собираться в CI без запуска эмулятора; build job отдельный — план. `RPE` CI; план. |
| J02 | [M] | **Частично** | Эмулятор можно запускать отдельной headless-джобой; политика — не в release gate. `VJ-APP`; `RPE` CI; план. |
| J03 | [M] | **Да** | Эмулятор отсутствует в release payload. `RPE` release policy; `HOLD`. |
| J04 | [M] | **Да** | В артефактах нет BIOS/ROM/ключей; проверка payload. `RPE` `docs/RELEASING.md`; `HOLD`. |
| J05 | [M] | **Частично** | CI-проверка лицензий/SPDX/third_party — план (`verify_third_party_licenses.py`); для Jaguar ещё нет. `AUDIT` §7; план. |
| J06 | [M] | **Частично** | Checksums/подписи есть у VJ; reproducible artifacts SDK — план. `VJ-SEC`; план. |
| J07 | [M] | **Частично** | Versioned SDK manifest — план. `RPE` `VERSION.txt`; план. |
| J08 | [M] | **Да** | Draft-only release workflow (ADR-013) работает. `RPE` ADR-013. |
| J09 | [M] | **Частично** | Пакеты для host-ОС: SDK/UI — план; toolchain env-install. `RPE` BUILDING; план. |
| J10 | [M] | **Частично** | SBOM/third-party отчёт — план. `AUDIT` §7; план. |
| J11 | [S] | **Частично** | VJ подписан; SDK-артефакты — план. `VJ-SEC`; план. |
| J12 | [M] | **Частично** | Rollback/known-issues/совместимость: VJ документирует; SDK — план. `VJ-CD`; план. |

### K. Поддержка и сообщество

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| K01 | [M] | **Да** | VJ активен: коммиты/релизы 2026, v3.9.0; `vjag` — 2026. `VJ`; `VJAG`. |
| K02 | [M] | **Да** | Issue tracker/PR/discussions активны; есть ответы maintainer в issues/PR. `VJ` README; `VJ-CI`. |
| K03 | [M] | **Да** | Регулярные теги/релизы/nightly. `VJ` README; `VJ-SEC`. |
| K04 | [M] | **Да** | Документация актуальна: BIOS, CD, GDB, app, testing. `VJ` docs. |
| K05 | [M] | **Да** | Тесты/CI, harness, CD matrix; не только бинарники. `VJ-CI`; `VJ-CD`. |
| K06 | [M] | **Да** | Compatibility matrix/known issues/CD boot matrix. `VJ-CD`. |
| K07 | [M] | **Да** | `SECURITY.md`, private vulnerability reporting. `VJ-SEC`. |
| K08 | [S] | **Да** | Discussions, AtariAge, libretro community; `vjag` — GitHub. `VJ`; `VJAG`. |
| K09 | [S] | **Частично** | У VJ несколько контрибьюторов; `vjag` — один автор; bus factor требует вклада. `VJ`; `VJAG`. |
| K10 | [S] | **Частично** | Roadmap/issues/release notes есть; формальная deprecation policy — не заявлена. `VJ` README; план. |

### L. Безопасность, приватность, red flags

| ID | Тег | Ответ | Комментарий и доказательство |
|---|---|---|---|
| L01 | [X] | **Нет** | Телеметрии/аналитики без opt-in нет; RA/Discord — opt-in. `VJ` README; `VJ-SEC`. |
| L02 | [X] | **Нет** | Сеть для сборки/запуска не требуется; offline. `VJ` README; `VJ-SEC`. |
| L03 | [X] | **Нет** | Аккаунт/логин/облако не обязательны; RA — опционально. `VJ` README. |
| L04 | [X] | **Нет** | Root/админ/драйверы не нужны. `VJ` README. |
| L05 | [X] | **Нет** | Системные файлы/registry не модифицируются; данные в `--data-dir`. `VJ-APP`. |
| L06 | [M] | **Да** | Sandbox/container без доступа к home: `--data-dir`, headless, чтение ROM. `VJ-APP`. |
| L07 | [M] | **Да** | Полностью offline. `VJ` README. |
| L08 | [X] | **Да (VJ); Нет у payload** | VJ включает ROM/BIOS в поставку (embedded); поэтому не в payload. Для HLE-only форка — нет. `VJ-BIOS`; `HOLD`. |
| L09 | [M] | **Да** | Checksums/подписи: SHA256SUMS, minisign, SLSA, macOS notarization. `VJ-SEC`. |
| L10 | [M] | **Частично** | Модель угроз для недоверенных ROM/образов формально не описана; эмулятор не должен давать host code execution. `VJ-SEC`; план threat model. |
| L11 | [X] | **Нет** | Данные о проекте/пользователе не собираются; RA/Discord opt-in. `VJ` README. |
| L12 | [S] | **Частично** | Воспроизводимое окружение: CI/AppImage/deb; Docker/Nix image — план. `VJ-CI`; план. |

---

## 4. Сводные таблицы

### 4.1. Зафиксированная связка E1 + S1 + C1 (на случай возобновления)

| Параметр | Значение |
|---|---|
| Кандидат / версия / commit | VJ master `8543bbf` (README: v3.9.0) + MIT-профиль GCC/`lyxass 1d67d1a` + `jagcd` (план) |
| Режим использования | external process (VJ); build tool (SDK); linked MIT (свой HAL) |
| Кол-во `[M]` — да / нет / частично | **78 / 1 / 57**; ещё 1 «Не знаю» (H09) |
| Кол-во `[X]` — нарушений | **2**: A06 (BIOS в VJ/JSDK) и L08 (BIOS в поставке VJ); корень один — embedded BIOS, поэтому HLE-only форк снимает оба. A12 — отдельный `[M]`-риск (нет разрешения на BIOS) |
| Кол-во `[S]` — да / нет | **13 / 0**; 12 частично, 1 «Не знаю» (A16), 1 Н/П (F19) |
| Юридический gate (A) | **conditional**: MIT сохраняется; BIOS/Atari headers требуют исключения/форка |
| Эмуляторный gate (B–D) | **pass** для CD-функций; **conditional** для поставки (BIOS/D09) |
| SDK gate (E–H) | **conditional**: toolchain есть, HAL/CD/ROM header — собственные, не готовы |
| CD gate (G) | **conditional/fail**: эмулятор CD pass, SDK CD authoring нет |
| Интеграция (I–J) | **conditional**: планы/пресеты/CI ещё не сделаны |
| Поддержка (K–L) | **pass** (VJ активен, security policy есть) |
| Итог | **DEFERRED — «до лучших времён»** (ADR-016): связка зафиксирована, S47–52 не активируются, Jaguar-компоненты не создаются |

### 4.2. Альтернативы: где именно они падают

Точные счётчики `[M]`/`[S]` для альтернатив не считались: каждая
отклонена по конкретным блокирующим пунктам, и полный счёт не изменит
verdict. Полные 183 ответа — в §3 для зафиксированной связки.

| Кандидат | Ключевые pass-факты | Блокирующие `[M]`/`[X]` | Итог |
|---|---|---|---|
| **VJ (E1)** | CD, HLE, BIOS override, `.cue/.bin/.cdi/.chd`, Memory Track, headless, hash, GDB, active | A06, L08 (`[X]`, embedded BIOS); A12 (`[M]`, нет разрешения); D09 — `[S]`, нет HLE-only build | **conditional**, external only |
| **vjag (E2)** | MIT, 68000/GPU/DSP/OP/blitter, headless, JSON, input replay, tests | B10, B11, B12, B13 (нет CD), C08 (нет save states); B16 — `[S]`, нет boot ROM | **reject for v0.4.0**; MIT R&D track |
| **MAME (E3)** | GPL-2.0, CLI/debugger/trace, source | B10/B11 (CD `NOT_WORKING`), B19 (BIOS required); B18 — `[S]`, нет CD CI | **trace only** |
| **BigPEmu (E4)** | full retail CD, deterministic, Memory Track, VLM | A05/A14 (commercial/redistribution restrictions), C02/C04/C05 (no documented headless), D07 (no macOS), A02 (no standard license/EULA) | **reject**; optional local manual only |
| **S1 MIT-профиль** | MIT, GCC/`lyxass`, no Atari headers, no BIOS | E03/E04/E06/E07/E09 (свой startup/linker/ROM header/repro), F01–F15 (HAL ещё нет), G01–G15 (CD ещё нет) | **conditional**; build it |
| **S2 jaglib** | MIT (except header), video/OL/input/blitter/GPU/DSP load | A02/F17 (`jagcore.h` Atari), F08 (`malloc`), G01–G15 (no CD) | optional after header replacement |
| **S3 rmvlib** | sprites/blitter/sound, mature | A03/H02 (LGPL-2.1 in target), Atari `jaguar.inc` | optional only with LGPL compliance |
| **S4 jaguar-sdk** | complete historical SDK, CD samples, CD BIOS | A02/A06/A12/H07 (no license, Atari BIOS/docs), A05 (no redistribution) | **env-install only**; not vendored |
| **C2 cdi-maker** | simple `.cdi` assembler, beta | A02/A14 (no LICENSE) | optional after permission/replacement |

### 4.3. Подсчёт ответов (автоматически по таблицам §3)

Подсчёт по 183 строкам §3 (183 ID, без пропусков и дублей):

| Тег | Всего | Да | Нет | Частично | Не знаю | Н/П |
|---|---:|---:|---:|---:|---:|---:|
| `[M]` | 137 | 78 | 1 (A12) | 57 | 1 (H09) | 0 |
| `[S]` | 27 | 13 | 0 | 12 | 1 (A16) | 1 (F19) |
| `[X]` | 19 | 2 (A06, L08) | 17 | 0 | 0 | 0 |

**Правило интерпретации:**
- если строка помечена «Да (VJ)» или «Да (механизм)», она считается `Да`;
- **H08** сформулирован негативно («Нет ли royalty…?»), поэтому ответ
  записан как «Да — ограничений нет» и считается прохождением;
- «Н/П» не входит в `[M]`/`[S]` счётчики;
- «Не знаю» считается непройденным must-have;
- 57 «Частично» — это в основном ещё не написанные части SDK/CD/HAL,
  а не найденные дефекты готового кандидата.

---

## 5. CD-first: план на случай возобновления

> **Не активируется сейчас.** Платформа DEFERRED (ADR-016); раздел
> фиксирует, как должна выглядеть работа, если Jaguar будет возвращён.
> Это reference, не backlog: старт — только по явному личному разрешению
> maintainer, а не автоматически после снятия hold.

Аудит v2 предлагал **cartridge-first**, CD — отдельной фазой. Для
заданного ограничения «активно сотрудничать именно с Atari Jaguar CD»
это не подходит: CD должен быть основным target, иначе v0.4.0 снова
получится «Jaguar без CD». Картриджный профиль остаётся только
вспомогательным smoke-тестом (быстрая проверка 68000/видео), а не
релизным gate. Предлагаемая замена:

### 5.1. S47–48 — MIT-профиль + CD boot, без BIOS

- `m68k-elf-gcc` + binutils + `lyxass`; CMake toolchain и preset
  `jaguar-release`; `find_program`, без вендоринга GPL-тулчейна.
- Свой startup/reset/vector table/linker script/ROM header/checksum.
- Свой `jagcd`: boot track, TOC, `.cue/.bin`, manifest, size/budget
  check, deterministic output.
- External VJ adapter: профиль запуска, `--headless`, `--frames`,
  `--screenshot`, `--expect-hash`, `--press`, `--data-dir`.
- **Никакого Atari BIOS/`cdboot.rom`/`jagcore.h`/`jaguar.inc` в repo.**
- Gate: `hello_cd` homebrew boot в VJ HLE; hash; лог; no BIOS.

### 5.2. S49–50 — HAL и CD runtime

- 68000/Tom/Jerry HAL: video/object list, blitter, input, timers/IRQ,
  memory ledger 2 MiB.
- CD API wrappers по документированным вызовам (`CD_init`, `CD_read`,
  `CD_getoc`, `CD_switch`, `CD_mode`, `CD_stop`) — clean-room, без
  копирования Atari `CD.INC`.
- Memory Track save/load; asset packaging; read-speed/streaming model.
- Gate: homebrew читает данные с CD, Memory Track save/load, W0.

### 5.3. S51–52 — W0/W1, hello CD, draft v0.4.0

- W0 5 кейсов, W1 4 куба/48 tri; FPS/frame time/memory; screenshot/hash.
- `hello_triangle`/`rotating_cube`/`hello_cd`; UI profile; draft release.
- Real hardware — опциональный ручной gate (GameDrive/Jagtopia,
  пользовательский BIOS), с пометкой «emulator validation, not hardware».

### 5.4. Параллельный MIT-трек (если/когда Jaguar будет активирован)

- Внести CD-контроллер/HLE boot в `vjag` (MIT) — единственный путь к
  MIT-эмулятору CD. Это большой исследовательский проект; не обещать
  в v0.4.0.
- Upstream в VJ: build option «без встроенных BIOS» (HLE-only),
  CD homebrew boot matrix, JSON schema, `--max-seconds`, exit codes.
- Лицензионные запросы: `cdi-maker` (LICENSE), `rmac`/`rln`
  (подтверждение), `jaglib` (`jagcore.h` replacement), `rmvlib`
  (MIT/BSD relicense), `jaguar-sdk` (top-level LICENSE/CD parts).
- Atari/Hasbro: письменная позиция по BIOS/ROM и nominative use.

---

## 6. Условия возобновления (если платформа будет возвращена)

> Пока действует **DEFERRED** (ADR-016), эти условия не применяются и
> S47–52 не активируются. Раздел — чек-лист на случай отдельного
> **личного разрешения maintainer** начать Jaguar.

1. **Maintainer лично и явно разрешает начать Jaguar** — отдельным
   решением, с датой и объёмом работ. Это не заменяется снятием LEGAL
   HOLD, закрытием лицензионных вопросов или наличием времени.
2. Maintainer письменно выбирает один режим:
   - **external-only**: VJ устанавливается пользователем, не входит в
     payload/CI-артефакты; остаточный BIOS-риск принят;
   - **HLE-only fork**: VJ собирается без BIOS-массивов, распространяется
     как отдельный GPL-3.0 процесс с source offer; риск снят технически.
3. Юридическая проверка: GPL/external boundary, встроенные BIOS,
   Atari headers, nominative use/trademark, export control.
4. Подписан clean-room policy для CD: только документация/спецификации;
   никаких Atari headers/source/BIOS; разделение «читатель спеки» и
   «писатель кода».
5. Лицензионные вопросы: `cdi-maker`, `rmac`/`rln`, `jaglib`
   `jagcore.h`, `rmvlib`; до решения — не вендорить.
6. CI/payload: проверка, что в артефактах нет BIOS/ROM/эмулятора;
   SPDX/third-party manifest; reproducible build test.
7. Real-hardware gate: GameDrive/Jagtopia + пользовательский BIOS;
   документировать, что это не входит в payload.

---

## 7. Что осталось «Не знаю» / открытые вопросы

> Пока платформа DEFERRED (ADR-016), эти вопросы не блокируют другие
> платформы и не требуют немедленного решения. Они — чек-лист на случай
> возобновления.

1. **A16** — письменная позиция Atari/Hasbro по BIOS/ROM/headers.
2. **H09** — export-control/trademark-анализ.
3. **D09** — техническая возможность HLE-only сборки VJ без BIOS-массивов
   (нужен форк/PR и проверка CD boot).
4. **B18** — публичный CI CD boot с лицензионно чистыми homebrew-образами
   (сейчас matrix — локальный, с приватным корпусом).
5. **G02/G03/G09/G13** — точный формат boot track/TOC/ISO и
   детерминированный writer; нужен прототип `jagcd`.
6. **L10** — формальная threat model для недоверенных ROM/CD-образов.
7. **F15/I04** — реальный ledger 2 MiB и замеры после появления HAL.
8. **K09** — bus factor MIT-трека (`vjag` — один автор).

---

## 8. Источники (полные ссылки)

- Jaguar Technical Reference Manual rev 8:
  <https://www.hillsoftware.com/files/atari/jaguar/jag_v8.pdf>
- VJ: <https://github.com/libretro/virtualjaguar-libretro>
  - BIOS: <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/bios.md>
  - CD issues: <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/cd-known-issues.md>
  - CD matrix: <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/cd-boot-matrix.md>
  - App/headless: <https://github.com/libretro/virtualjaguar-libretro/blob/master/app/README.md>
  - GDB: <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/gdb-stub-guide.md>
  - Security: <https://github.com/libretro/virtualjaguar-libretro/blob/master/SECURITY.md>
- `vjag` MIT: <https://github.com/HelgeSverre/jaguar>
  - Headless: <https://github.com/HelgeSverre/jaguar/blob/main/docs/headless.md>
- MAME: <https://github.com/mamedev/mame/blob/master/src/mame/atari/jaguar.cpp>
- BigPEmu: <https://www.richwhitehouse.com/jaguar/>
  - FAQ: <https://www.richwhitehouse.com/jaguar/index.php?content=faq>
  - Download: <https://www.richwhitehouse.com/jaguar/index.php?content=download>
- `jaguar-sdk`: <https://github.com/cubanismo/jaguar-sdk>
  - CD boot notes: <https://github.com/cubanismo/jaguar-sdk/blob/master/jaguar/cdboot/cdboot.txt>
  - CD readme: <https://github.com/cubanismo/jaguar-sdk/blob/master/jaguar/cdrom/readme.txt>
- `lyxass`: <https://codeberg.org/42Bastian/lyxass>
- `jaglib`: <https://github.com/toarnold/jaglib>
- `rmvlib`: <https://github.com/theRemovers/rmvlib>
- `new_bjl`: <https://github.com/42Bastian/new_bjl>
- `cdi-maker`: <https://github.com/cubanismo/cdi-maker>
- GCC RLE: <https://www.gnu.org/licenses/gcc-exception-3.1.html>
- GPL FAQ output: <https://www.gnu.org/licenses/gpl-faq.html#WhatCaseIsOutputGPL>
- Jaguar CD developer reference:
  <https://asterick.github.io/jag-docs/cdrom/bios-api.html>,
  <https://asterick.github.io/jag-docs/cdrom/programming-guide.html>
- Локальные документы: `docs/plans/jaguar_legal_audit_v2.md`,
  `docs/plans/jaguar_legal_hold.md`, `docs/RELEASING.md`,
  `docs/adr/ADR-013-release-freeze.md`, `LEGAL.md`, `LICENSE`.
