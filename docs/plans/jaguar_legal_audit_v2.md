# Atari Jaguar / Jaguar CD — повторный аудит лицензий и пригодности (2026-11-15)

> **Статус:** Jaguar S47–52 остаются в **LEGAL HOLD**. Этот документ —
> инженерная оценка лицензионных рисков, **не юридическое заключение**.
> Проверялись только первоисточники: тексты лицензий, README и исходники
> проектов, официальные сайты, Jaguar Technical Reference Manual rev 8 и
> тексты GNU/SPDX. Перед снятием hold нужна юридическая проверка.
> Для сравнения кандидатов используйте бриф
> [`jaguar_emulator_sdk_brief.md`](jaguar_emulator_sdk_brief.md) (183 вопроса).

## 1. Короткий вывод

1. **В рамках аудита не найдено полного open-source эмулятора
   Jaguar/Jaguar CD под MIT или BSD.** Рабочие открытые эмуляторы —
   GPL-2.0/GPL-3.0.
2. Лучший открытый CD-эмулятор — **`libretro/virtualjaguar-libretro`**
   (GPL-3.0): активный, поддерживает Jaguar CD, HLE-загрузку и отладку.
   Его можно использовать **только как внешний процесс**, не линковать
   в движок/UI и не включать в SDK/release payload без GPL-обязательств.
3. **GPL-эмулятор не делает MIT-движок GPL**, пока это отдельная
   программа, запускаемая пользователем/UI через CLI. Если слинковать
   GPL-ядро с UI или статически встроить — это уже производная работа
   под GPL; для проекта с требованием MIT это неприемлемо.
4. **MIT-совместимый SDK для картриджей возможен**, но не в виде
   `jaguar-sdk`: `lyxass` — MIT (GPU/DSP-ассемблер), `jaglib` — MIT
   (после замены Atari-заголовка `jagcore.h`), `new_bjl` — Unlicense,
   GCC/binutils — сборочные инструменты (их вывод не становится GPL,
   см. GCC Runtime Library Exception).
5. **Jaguar CD — главный блокер.** В `jaguar-sdk` лежит Atari CD BIOS
   v4.05 и Atari-библиотеки без лицензии; эмуляторы либо требуют
   BIOS, либо встраивают его. Для MIT-clean CD нужен собственный
   clean-room boot/CD-слой либо пользовательский BIOS; это отдельный
   исследовательский трек, а не зависимость.
6. **Рекомендация:** если Jaguar возобновляется — сначала cartridge-only
   на MIT-совместимом SDK; CD — отдельной фазой после юридической и
   технической проверки. `jaguar-sdk`, `rmac`, `rln`, Atari BIOS/ROM и
   `BigPEmu` не вендорить.

## 2. Критерии оценки

- **MIT-совместимые лицензии для кода, который линкуется в движок:**
  MIT, BSD-2/3-Clause, Apache-2.0 (с patent-оговоркой), ISC, Zlib,
  Unlicense/CC0. Требуется сохранять copyright/NOTICE.
- **LGPL-2.1:** можно линковать с MIT-кодом при выполнении условий
  LGPL (исходники библиотеки, возможность повторной линковки,
  уведомления). Для консольного статического ROM это создаёт
  практические обязанности; «просто MIT» не получается.
- **GPL-2.0/3.0:** нельзя включать в MIT-проект как библиотеку без
  перевода всей производной работы под GPL. Отдельный процесс —
  другой режим; границу должен подтвердить юрист.
- **Сборочные инструменты:** лицензия компилятора/ассемблера/линкера
  не переходит на его вывод; GCC дополнительно даёт Runtime Library
  Exception для libgcc/заголовков.
- **Эмулятор:** отдельный исполняемый файл/процесс не является частью
  движка; при распространении эмулятора действуют его лицензия и
  обязательства, но MIT-код движка остаётся MIT.

## 3. SDK и библиотеки

| Компонент | Лицензия (первоисточник) | Что это | Можно ли линковать в MIT-движок | Вывод |
|---|---|---|---|---|
| [`cubanismo/jaguar-sdk`](https://github.com/cubanismo/jaguar-sdk) | **Нет top-level LICENSE**; в репозитории Atari samples, сканы docs, DOS-бинарники; CD BIOS v4.05 | Полный исторический SDK: 68000-тулчейн, Atari libraries, CD BIOS | **Нет**: лицензия неясна, внутри Atari-код/BIOS | Только env-install пользователем; не вендорить, не включать в SDK payload |
| `rmac` / `rln` (submodules `jaguar-sdk`) | В исходниках только copyright, **нет явной лицензии** | Ассемблер 68000/GPU/DSP и линкер | Как инструменты — вывод не обязан быть GPL; но распространение самих инструментов неясно | Не вендорить; заменить на `lyxass`/GCC-путь или получить разрешение авторов |
| [`42Bastian/lyxass`](https://codeberg.org/42Bastian/lyxass) | **MIT** ([LICENSE](https://codeberg.org/42Bastian/lyxass/raw/branch/main/LICENSE)) | Ассемблер Lynx/Jaguar, GPU/DSP, выдаёт BJL/GPU/DSP-бинарник | **Да** (как build tool; код не линкуется) | Рекомендуемый MIT-совместимый ассемблер для RISC-кода |
| [`42Bastian/new_bjl`](https://github.com/42Bastian/new_bjl) | **Unlicense/public domain** ([LICENSE](https://github.com/42Bastian/new_bjl/blob/master/LICENSE)) | BJL-загрузчик/утилиты, 68k/GPU-примеры | **Да** (утилита, не runtime) | Можно вендорить с attribution; полезен для Skunkboard/BJL |
| [`toarnold/jaglib`](https://github.com/toarnold/jaglib) | **MIT** ([LICENSE](https://github.com/toarnold/jaglib/blob/master/LICENSE)), но [`jagcore.h`](https://github.com/toarnold/jaglib/blob/master/jagcore.h) — Atari copyright «all rights reserved» | Консоль, video/object list, blitter, GPU/DSP load, joystick, file I/O; использует `malloc` | **Частично**: после замены `jagcore.h` на clean-room-заголовок; иначе MIT-заявление конфликтует с Atari-заголовком | Лучшая MIT-основа для cartridge-HAL, но требует очистки и адаптации (no-heap) |
| [`theRemovers/rmvlib`](https://github.com/theRemovers/rmvlib) | **LGPL-2.1** ([LICENSE](https://github.com/theRemovers/rmvlib/blob/master/LICENSE)), но [`include/jaguar.inc`](https://github.com/theRemovers/rmvlib/blob/master/include/jaguar.inc) — Atari copyright | Спрайты, blitter, sound, joypads, display | **Только с LGPL-комплаенсом**; для статического ROM нужны relinkable objects/source | Не для «чистого MIT»; optional-модуль или замена |
| [`ericde45/JAG_Hively`](https://github.com/ericde45/JAG_Hively) | **MIT** ([LICENSE](https://github.com/ericde45/JAG_Hively/blob/master/LICENSE)), но `jaguar.inc` — Atari copyright | AHX/Hively replay для Jaguar | **После замены `jaguar.inc`** | Полезно для audio-фазы (v0.6+); не для v0.4.0 |
| [`ericde45/lsp_version_XW`](https://github.com/ericde45/lsp_version_XW) | **MIT** ([LICENSE](https://github.com/ericde45/lsp_version_XW/blob/master/LICENSE)), но `jaguar.inc` — Atari copyright | LSP replay + SFX | **После замены `jaguar.inc`** | То же; audio-фаза |
| GCC/binutils (`m68k-elf-gcc`) | **GPL-3.0+/GPL-2.0**; GCC Runtime Library Exception | Компилятор C и binutils для 68000 | **Да для вывода**: GCC exception разрешает non-GPL Target Code; GPL FAQ: вывод программы в общем случае не покрыт GPL | Build tools; не вендорить без GPL-комплаенса; использовать env-install/Docker |
| `vbcc` (jaglib build) | Лицензия не подтверждена по доступному первоисточнику | Альтернативный C-компилятор | Зависит от лицензии; не проверено | Не включать в MIT-профиль до аудита; GCC-путь предпочтительнее |

### 3.1. Ключевые оговорки по SDK

- **`jaguar-sdk` — не «public domain», а неуточнённый набор.** В нём есть
  Atari `jaguar/lib/gpustd.a`, `libvbcc.a`, CD BIOS и сканы документации.
  Даже если часть кода исторически public domain, это не подтверждено
  лицензией репозитория (top-level LICENSE отсутствует).
- **`jaglib` MIT-лицензия не очищает `jagcore.h`.** Заголовок прямо
  запрещает воспроизведение/распространение; его нужно заменить
  clean-room-определениями регистров (факты/адреса) или получить
  разрешение. То же с `rmvlib/include/jaguar.inc` и `jaguar.inc`
  в audio-репозиториях.  
- **`rmvlib` LGPL-2.1** — не «MIT-совместимая» библиотека в простом
  смысле. Для консольного статического ROM LGPL-2.1 §6 требует
  возможности повторной линковки (object files/source) и уведомлений;
  это усложняет release payload. Использовать только как optional
  external module или после смены лицензии.

### 3.2. Отдельно про Jaguar CD SDK

- В `jaguar-sdk` CD-часть — это примеры `cdboot`, `cdrom` и **бинарник
  Atari CD BIOS v4.05** (`jaguar/cdrom/cdbios45.tx`, `cdbios45.dta`);
  readme прямо описывает «Atari Jaguar CDROM BIOS». Лицензии на
  распространение этого BIOS в репозитории нет.
- `libretro/virtualjaguar-libretro` содержит **HLE CD BIOS** как часть
  GPL-3.0-эмулятора; это не SDK для сборки игр и не MIT-код.
- `Kitrinx/JaguarCDBIOS` в проверенном состоянии — пустой репозиторий
  без лицензии/README; как SDK не рассматривается.
- `ajgowans/jaguarcd` — это база MiSTer Downloader для CD-образов, **не
  SDK**; лицензия MIT относится к базе, а не к играм/BIOS.
- **Вывод:** отдельного открытого Jaguar CD SDK с MIT/BSD в аудите не
  найдено. Для CD остаются: пользовательский BIOS + собственный
  clean-room boot/CD-слой, либо GPL-эмулятор как внешний инструмент.

## 4. Эмуляторы Jaguar и Jaguar CD

| Эмулятор | Лицензия | Jaguar CD | BIOS | Техническая пригодность | Юридический режим |
|---|---|---|---|---|---|
| [`libretro/virtualjaguar-libretro`](https://github.com/libretro/virtualjaguar-libretro) | **GPL-3.0** ([LICENSE](https://github.com/libretro/virtualjaguar-libretro/blob/master/LICENSE)) | **Да**: `.cue/.bin/.cdi/.chd`, CD controller, Memory Track ([README](https://github.com/libretro/virtualjaguar-libretro#readme)) | **Не требуется**: boot ROM и CD BIOS встроены; HLE по умолчанию ([docs/bios.md](https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/bios.md)) | Высокая: активный проект, CD boot matrix, отладчик, SIMD blitter | **Только внешний процесс**; не линковать. Встроенные Atari BIOS — отдельный риск; для чистого билда нужен HLE-only fork без BIOS-массивов |
| [`djipi/Virtual-Jaguar-Rx`](https://github.com/djipi/Virtual-Jaguar-Rx) | **GPL-3.0** ([LICENSE](https://github.com/djipi/Virtual-Jaguar-Rx/blob/develop/LICENSE)) | Да по исходникам (`src/cdrom.cpp`), но старый и Windows-ориентированный | Встроенные BIOS-массивы (`src/jagcdbios.cpp`, `src/jagbios.cpp`) | Средняя/неизвестная; README не документирует CD так подробно | GPL; внешний процесс/fallback; BIOS-риск |
| [MAME](https://github.com/mamedev/mame) | **GPL-2.0** как целое ([COPYING](https://github.com/mamedev/mame/blob/master/COPYING)); Jaguar-драйвер — BSD-3-Clause | Да, но `jaguarcd` помечен `MACHINE_NOT_WORKING` | Требует `jag_cd.bin`/`jagdevcd.bin` ([jaguar.cpp](https://github.com/mamedev/mame/blob/master/src/mame/atari/jaguar.cpp)) | Низкая для acceptance: `NOT_WORKING`, `IMPERFECT_GRAPHICS`, `IMPERFECT_SOUND` | Только трассировка/отладка; не gate; BIOS не распространять |
| [BigPEmu](https://www.richwhitehouse.com/jaguar/) | **Closed-source freeware; публичной лицензии/EULA не обнаружено**; официальный FAQ: исходники не открываются из-за contractual obligations ([FAQ](https://www.richwhitehouse.com/jaguar/index.php?content=faq)) | Да, полная CD-библиотека, VLM, Memory Track | Не документировано как обязательный внешний BIOS; эмулятор поставляется как binary | Высокая: полная совместимость, deterministic по заявлению автора | **Высокий юридический риск**: нельзя вендорить/включать в payload; только локальный необязательный gate при принятии риска |

**Вывод по эмуляторам:** MIT/BSD-эмулятора Jaguar CD в аудите не найдено.
`MAME` юридически самый чистый по лицензии, но технически не работает для
CD. `libretro/virtualjaguar-libretro` — лучший открытый CD-вариант, но
GPL-3.0 и со встроенными Atari BIOS. `BigPEmu` — лучший технически, но
закрытый и без лицензии.

> Отдельно: файл MAME-драйвера Jaguar помечен `BSD-3-Clause`, но MAME
> как целое распространяется под GPL-2.0 и драйвер тесно связан с
> MAME-core. Выделить из него самостоятельный MIT-эмулятор — отдельный
> крупный проект, а не готовая зависимость.

## 5. Как сохранить MIT на движок

### 5.1. Разделение процессов

- **Движок, SDK, UI — MIT.** Эмулятор — отдельный исполняемый файл,
  запускаемый по CLI. UI хранит только профиль запуска (путь, аргументы,
  формат скриншота), не код эмулятора.
- **Не линковать** libretro core, MAME, Virtual Jaguar в UI/engine.
  GPL-3.0/GPL-2.0 в статической/динамической линковке создаёт
  производную работу; для проекта с MIT-требованием это не вариант.
- **Не включать** эмулятор в SDK/release payload. Если очень нужно
  распространять — отдельный installer/архив с GPL-текстом, исходниками
  и BIOS-политикой; это отдельный юридический проект.
- **BIOS/ROM** не распространять: пользователь использует свой BIOS
  или HLE-режим; для CI — только HLE/собственные тестовые образы.

### 5.2. Что можно линковать в MIT-движок

- `lyxass` — build tool, MIT.
- `new_bjl` — утилита, Unlicense.
- `jaglib` — MIT-код после замены `jagcore.h`; но в нём `malloc`, поэтому
  для no-heap HAL лучше брать только нужные части/переписать.
- Собственный HAL/asset/CD-layer — MIT.
- `rmvlib` — только LGPL-комплаенс или после разрешения автора.
- `jaguar-sdk`/Atari libs/BIOS — **нет**.

### 5.3. Сборочные инструменты

- `m68k-elf-gcc` + binutils: GPL, но **Target Code** не обязан быть GPL;
  GCC Runtime Library Exception разрешает non-GPL программы использовать
  libgcc/заголовки. Ссылки:
  [GCC Runtime Library Exception](https://www.gnu.org/licenses/gcc-exception-3.1.html),
  [GPL FAQ: output of a GPL program](https://www.gnu.org/licenses/gpl-faq.html#WhatCaseIsOutputGPL).
- `lyxass` MIT — для GPU/DSP.
- `rmac`/`rln` без лицензии — не включать в поставляемый toolchain;
  допустимы только как локальный env-install, если maintainer принимает
  риск.

## 6. Варианты встраивания/расширения выбора

### Вариант A — внешний emulator adapter (рекомендуется)

В `tools/ui_client` добавить `EmulatorProfile`:

```text
id, display_name, executable, args_template, screenshot_mode,
bios_policy, license_notice, source_url
```

UI только запускает процесс и собирает результат. Профили:
`retroarch` + `virtualjaguar_libretro`, `virtualjaguar`, `bigpemu`, `mame`.
Плюсы: MIT-код не смешивается с GPL/closed-source; пользователь выбирает
эмулятор; легко добавить новый. Минусы: нет in-process embedding;
нужны внешние бинарники.

Практическая схема:

```text
tools/emulators/profiles/
  retroarch-virtualjaguar.json   # GPL-3.0, external, bios=optional
  virtualjaguar-rx.json          # GPL-3.0, external, legacy
  bigpemu.json                   # closed, external, optional
  mame.json                      # GPL-2.0, trace only
```

```cpp
struct EmulatorProfile {
  std::string id;
  std::string executable;
  std::vector<std::string> args;   // {rom}, {core}
  std::string license_spdx;        // GPL-3.0-only и т.п.
  std::string source_url;
  bool requires_bios;
  bool allowed_in_ci;
};

class EmulatorAdapter {
public:
  virtual bool Detect() = 0;
  virtual bool Launch(const EmulatorProfile&, const std::string& rom) = 0;
  virtual bool CaptureScreenshot(const std::string& path) = 0;
};
```

Ни один профиль не попадает в `third_party/` и не линкуется; JSON-файл
содержит только метаданные и ссылку на лицензию. Это позволяет
расширять список эмуляторов без изменения MIT-кода.

### Вариант A2 — SDK/toolchain provider

Аналогичный реестр для SDK:

```text
tools/toolchains/profiles/
  jaguar-gcc.json        # m68k-elf-gcc + binutils + lyxass
  jaguar-legacy.json     # jaguar-sdk env-install, optional, not default
```

```cpp
struct ToolchainProfile {
  std::string id;
  std::string cmake_toolchain;   // cmake/jaguar-gcc-toolchain.cmake
  std::vector<std::string> programs; // m68k-elf-gcc, lyxass, ...
  std::string license_policy;    // MIT-clean | LGPL-optional | legacy
  bool vendored;                 // всегда false для GPL/unknown
};
```

CMake ищет `m68k-elf-gcc`/`lyxass` через `find_program`, проверяет
версии и не копирует их в SDK. `jaguar-sdk` может быть только
пользовательским legacy-профилем (`JAGUAR_SDK_ROOT`), не default.

### Вариант B — HLE-only fork libretro core (GPL, отдельный процесс)

Собрать `virtualjaguar-libretro` без встроенных BIOS-массивов, оставив
HLE-путь; запускать как external core/RetroArch. Это снижает BIOS-риск,
но код остаётся GPL-3.0. Подходит для CI/ручного gate, не для SDK.
Нужен upstream-патч/форк и проверка, что HLE CD boot работает без
встроенных образов.

### Вариант C — MIT-clean cartridge SDK

Собрать профиль `jaguar-gcc`:
`m68k-elf-gcc` + binutils + `lyxass` + `new_bjl` + опционально `jaglib`
(с clean-room `jagcore.h`). Свой startup/linker script/HAL — MIT.
CD не входит; это cartridge-only v0.4.0. Плюс: лицензионно чисто.
Минус: нет CD и готового SDK; больше собственного кода.

### Вариант D — LGPL `rmvlib` как optional module

Использовать для спрайтов/blitter/sound, но соблюдать LGPL-2.1:
исходники библиотеки, relinkable objects, уведомления. Для статического
ROM это burden; лучше держать вне default build и не обещать «MIT-only»
для сборок с rmvlib.

### Вариант E — clean-room CD layer

Движок вызывает CD BIOS на реальном железе (BIOS находится в CD-блоке,
не в игре), а для эмулятора пользователь предоставляет BIOS/HLE.
Собственный boot/TOC/ISO-слой — MIT; BIOS не распространяется.
Это единственный путь к MIT-clean CD, но требует отдельного
исследования и тестов; в v0.4.0 не обещать.

### Вариант F — dual-license / разрешения

- Попросить `libretro/virtualjaguar-libretro` сделать BIOS embedding
  опциональным и/или разрешить HLE-часть под MIT/BSD.
- Попросить `theRemovers` перелицензировать `rmvlib` под MIT/BSD.
- Попросить авторов `rmac`/`rln` подтвердить лицензию.
- Попросить `toarnold` заменить `jagcore.h` или подтвердить MIT на весь
  репозиторий.
Это может дать более широкий выбор без потери MIT.

### Вариант G — реальное железо

Jaguar + GameDrive/Skunkboard: нет лицензии эмулятора, нет BIOS в
payload; лучший финальный gate. Если железа нет — в релизных материалах
писать «emulator validation, not hardware».

## 7. Рекомендуемый план при снятии hold

1. **Не вендорить** `jaguar-sdk`, `rmac`/`rln`, Atari BIOS/ROM, BigPEmu.
2. **Cartridge-first:** MIT-профиль `jaguar-gcc` + `lyxass` + `new_bjl`;
   `jaglib` — только после замены `jagcore.h`; свой HAL.
3. **CD — отдельная фаза:** clean-room boot/CD layer или пользовательский
   BIOS; external libretro core для проверки; не включать в SDK.
4. **UI:** только external emulator adapter; libretro core — отдельный
   процесс; никакой линковки.
5. **Лицензионный манифест:** SPDX-заголовки, NOTICE, `THIRD_PARTY.md`;
   CI-проверка, что в `third_party/` нет GPL/LGPL/Unknown.
6. **Автоматизация:** `scripts/verify_third_party_licenses.py` —
   разрешённые SPDX (MIT/BSD/Apache-2.0/ISC/Zlib/Unlicense), запрет
   GPL/LGPL/NOASSERTION в default-сборке; отдельный `license-audit.yml`
   для PR. Для эмуляторов — проверка, что binary отсутствует в payload.
7. **Юридический gate:** maintainer/юрист подтверждает:
   - допустимость GPL-эмулятора как отдельного процесса;
   - статус встроенных BIOS в libretro core;
   - LGPL-комплаенс, если rmvlib используется;
   - отсутствие Atari-заголовков в MIT-коде.

## 8. Оценка связки: libretro Virtual Jaguar + lyxass + new_bjl

Шкала: **100 — лучший результат** (минимум юридического риска, простая
интеграция, максимум возможностей), **0 — непригодно**. Для юридического
критерия 100 = минимальный риск.

| Критерий | Оценка | Почему |
|---|---:|---|
| **1. Юридические риски** | **65/100** | `lyxass` — MIT, `new_bjl` — Unlicense; libretro core — GPL-3.0, но используется как отдельный процесс, поэтому MIT-движок не становится GPL. Минусы: libretro встраивает Atari boot ROM/CD BIOS (лицензия BIOS не подтверждена); `new_bjl` WIP, содержит Atari `68k_inc/jaguar.inc` и по README требует `rmac`/`rln` без явной лицензии. При **встраивании/бандлинге** оценка падает до ~25–35 |
| **2. Сложность интеграции в движок** | **30/100** | Это не SDK, а ассемблер + BJL-загрузчик. `lyxass` — ассемблер Lynx/GPU/DSP; для 68000 сам `new_bjl` README указывает `rmac`, которого в связке нет. `new_bjl` — WIP, зависит от `rmac`/`rln`, ориентирован на BJL/картриджи. Нет HAL, video/object list, input, memory, asset pipeline, CD API. Внешний emulator adapter — средняя сложность, но SDK-часть требует много собственного кода |
| **3. Объём возможностей для разработки CD-образов** | **10/100** | В связке нет сборщика CD-образов, TOC/ISO9660/Jaguar CD FS, CD BIOS bindings, boot-загрузчика, Memory Track API. Эмулятор умеет **запускать** `.cue/.bin/.cdi/.chd`, но не создавать их. `new_bjl` — про BJL/картриджи, не CD |
| **4. Глубина поддержки CD** | **30/100** | У libretro Virtual Jaguar CD-поддержка глубокая (CD controller, Memory Track, VLM, HLE/real BIOS, save states; форматы `.cue/.bin/.cdi/.chd`, `.iso` не поддерживается). Но у SDK она **нулевая**: можно тестировать готовый CD-образ, но нельзя пройти путь «исходники → CD-образ» без внешних инструментов/BIOS |
| **5. Активность поддержки сообществом** | **65/100** | libretro Virtual Jaguar — активный проект с релизами/CI и CD-матрицами; `lyxass` поддерживается (Codeberg, 2026); `new_bjl` — WIP и перенесён на Codeberg. Сообщество нишевое, но эмуляторная часть живая |

**Итог (равные веса): ≈ 40/100.** Это хороший **эмуляторный** компонент
(≈ 78/100 сам по себе), но слабый **SDK** (≈ 27/100): связка не покрывает
68000-toolchain, HAL и CD-authoring. Для cartridge-only с добавлением
GCC/binutils и `jaglib` оценка может подняться до ~55–60; для CD нужен
clean-room CD-слой или внешний CD-инструментарий.

Что поднимает оценку:

- добавить `m68k-elf-gcc` + binutils (build tools; вывод не GPL) и
  собственный startup/linker script;
- добавить `jaglib` (MIT, после замены `jagcore.h`) или собственный HAL;
- для CD — clean-room boot/TOC/ISO-слой + пользовательский BIOS;
- для UI — external emulator adapter, без линковки libretro core;
- для 68k-ассемблера — либо GCC-путь, либо `rmac`/`rln` только после
  подтверждения лицензии.

## 9. Первоисточники

- Jaguar Technical Reference Manual rev 8 (Tom & Jerry):
  <https://www.hillsoftware.com/files/atari/jaguar/jag_v8.pdf>
- `cubanismo/jaguar-sdk` (нет top-level LICENSE):
  <https://github.com/cubanismo/jaguar-sdk>
- `jaguar-sdk` CD BIOS notes:
  <https://github.com/cubanismo/jaguar-sdk/blob/master/jaguar/cdrom/readme.txt>
- `jaguar-sdk` CD boot:
  <https://github.com/cubanismo/jaguar-sdk/blob/master/jaguar/cdboot/cdboot.txt>
- `Kitrinx/JaguarCDBIOS` (пустой на момент аудита):
  <https://github.com/Kitrinx/JaguarCDBIOS>
- `ajgowans/jaguarcd` (MiSTer DB, не SDK):
  <https://github.com/ajgowans/jaguarcd>
- `rmac` submodule/source (без LICENSE):
  <https://github.com/cubanismo/jaguar-sdk/blob/master/.gitmodules>
- `lyxass` MIT:
  <https://codeberg.org/42Bastian/lyxass>
- `new_bjl` Unlicense + Atari `jaguar.inc`:
  <https://github.com/42Bastian/new_bjl>
  <https://github.com/42Bastian/new_bjl/blob/main/68k_inc/jaguar.inc>
- `new_bjl` README (WIP, rmac/rln):
  <https://github.com/42Bastian/new_bjl/blob/main/README.md>
- `jaglib` MIT + Atari `jagcore.h`:
  <https://github.com/toarnold/jaglib>
- `rmvlib` LGPL-2.1 + Atari `jaguar.inc`:
  <https://github.com/theRemovers/rmvlib>
- `JAG_Hively` MIT + Atari `jaguar.inc`:
  <https://github.com/ericde45/JAG_Hively>
- `lsp_version_XW` MIT + Atari `jaguar.inc`:
  <https://github.com/ericde45/lsp_version_XW>
- `libretro/virtualjaguar-libretro` (GPL-3.0, CD, embedded BIOS):
  <https://github.com/libretro/virtualjaguar-libretro>
- `libretro` BIOS policy:
  <https://github.com/libretro/virtualjaguar-libretro/blob/master/docs/bios.md>
- `Virtual Jaguar Rx` (GPL-3.0):
  <https://github.com/djipi/Virtual-Jaguar-Rx>
- MAME license:
  <https://github.com/mamedev/mame/blob/master/COPYING>
- MAME Jaguar driver:
  <https://github.com/mamedev/mame/blob/master/src/mame/atari/jaguar.cpp>
- BigPEmu (closed-source freeware, FAQ):
  <https://www.richwhitehouse.com/jaguar/>
  <https://www.richwhitehouse.com/jaguar/index.php?content=faq>
- GCC Runtime Library Exception:
  <https://www.gnu.org/licenses/gcc-exception-3.1.html>
- GNU GPL FAQ (output of GPL programs):
  <https://www.gnu.org/licenses/gpl-faq.html#WhatCaseIsOutputGPL>
- GNU GPL-3.0 / LGPL-2.1:
  <https://www.gnu.org/licenses/gpl-3.0.html>
  <https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>
