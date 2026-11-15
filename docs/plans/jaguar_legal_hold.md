# Atari Jaguar v0.4.0 — Legal hold + DEFERRED (2026-11-15)

> **Статус:** **DEFERRED — «до лучших времён»** (ADR-016). S47–52
> **не начинать**; планы сохранены как draft/reference; код, тулчейн,
> ROM, CD-образы и payload-компоненты Jaguar не создаются.
> LEGAL HOLD сохраняется: до возобновления нужна проверка юристом или
> явное принятие риска maintainer. **Старт работ — только после отдельного
> явного личного разрешения maintainer начать Jaguar; снятие hold само по
> себе план не активирует.**
> **Это инженерная оценка рисков, не юридическое заключение.**
> Детальный повторный аудит и варианты интеграции:
> [`jaguar_legal_audit_v2.md`](jaguar_legal_audit_v2.md). Бриф для
> сравнения кандидатов: [`jaguar_emulator_sdk_brief.md`](jaguar_emulator_sdk_brief.md).
> Заполненные ответы и связка: [`jaguar_emulator_sdk_answers.md`](jaguar_emulator_sdk_answers.md).
> Решение: [`../adr/ADR-016-jaguar-deferred.md`](../adr/ADR-016-jaguar-deferred.md).

## Почему hold

Предварительный аудит показал, что для Jaguar одновременно сходятся три
неопределённости:

1. **SDK/тулчейн.** У `jaguar-sdk` нет top-level LICENSE; в репозитории
   есть Atari samples, сканы документации и DOS-бинарники. `rmac`/`rln`
   содержат только copyright без явного лицензионного гранта.
2. **Эмулятор.** Лучший открытый CD-кандидат
   (`libretro/virtualjaguar-libretro`) — GPL-3.0 и со встроенными Atari
   BIOS; лучший технический (BigPEmu) — closed-source freeware с
   custom restrictive terms в ReadMe (без стандартной лицензии/EULA).
   MIT/BSD-эмулятора Jaguar CD не найдено.
3. **BIOS/ROM.** Для запуска могут потребоваться boot ROM/BIOS; их нельзя
   вендорить или включать в payload/CI-артефакты.

После аудита и 183-вопросного брифа maintainer зафиксировал связку
(ADR-016) и **отложил добавление поддержки платформы «до лучших времён»**.
LEGAL HOLD сохраняется как историческое основание и условие возобновления;
S47–52 не активируются, работы не начинаются. **Старт возможен только
после отдельного явного личного разрешения maintainer; снятие hold или
закрытие лицензионных вопросов само по себе не активирует план.**

## Что подтверждено

| Область | Источник | Результат |
|---|---|---|
| Hardware | Jaguar Technical Reference Manual rev 8 | 68000 — host; Tom = Object Processor + GPU + Blitter; Jerry = DSP + таймеры/I/O; GPU local 4 KiB; DSP local 8 KiB; 2 MiB DRAM; картридж до 6 MiB; Blitter Z/Gouraud только 16-bit; OP 1/2/4/8/16/24 bpp. Частоты 13.295/26.59 МГц в просмотренном фрагменте не зафиксированы — нужен отдельный hardware reference |
| `jaguar-sdk` | github.com/cubanismo/jaguar-sdk | top-level LICENSE отсутствует; README подтверждает Atari samples, сканы docs и DOS-бинарники; commit `b806b6f` (2026-03-30) |
| `rmac`/`rln` | tiddly.mooo.com Git, исходники | только copyright (Landon Dyer / Allan K. Pratt / Reboot & Friends), без явной лицензии |
| MAME | `src/mame/atari/jaguar.cpp` | драйвер `jaguar` = `MACHINE_NOT_WORKING \| MACHINE_IMPERFECT_GRAPHICS \| MACHINE_IMPERFECT_SOUND`; blitter phrase mode/OP timer — TODO |
| BigPEmu | richwhitehouse.com/jaguar, v1.221 Linux x64 | deterministic по заявлению автора, CLI; closed-source, free-of-charge, без явной лицензии на redistribution; FNV `C1B241BBFA5135CB` |
| Virtual Jaguar Rx | `djipi/Virtual-Jaguar-Rx`, GPL-3.0 | явная GPL-3.0; форк без крупных улучшений эмуляции; Linux-поддержку/точность проверить |
| libretro Virtual Jaguar | `libretro/virtualjaguar-libretro`, GPL-3.0 | активный Jaguar/CD-эмулятор; BIOS встроены (HLE по умолчанию); GPL и BIOS-риск → только внешний процесс |

## Зафиксированная связка (на случай возобновления)

- **Эмулятор:** `libretro/virtualjaguar-libretro` (GPL-3.0; master
  `8543bbf`, README: v3.9.0) — **только внешний процесс**, без линковки
  в engine/UI/SDK и без release payload. HLE CD — default; реальный
  BIOS — только пользовательский. Для поставки — HLE-only форк без
  BIOS-массивов, отдельный GPL-3.0 процесс с source offer.
- **SDK:** MIT-профиль — `m68k-elf-gcc` + binutils + `lyxass` (MIT) +
  собственный HAL/startup/linker + clean-room определения регистров.
  `jaglib` — optional после замены `jagcore.h`; `rmvlib` — optional
  LGPL, не default.
- **CD:** собственный MIT `jagcd` (boot track + `.cue/.bin` + manifest +
  size check); `cdi-maker` — только после разрешения автора или замены.
- **Не вендорить:** `jaguar-sdk`, `rmac`/`rln`, Atari CD BIOS/ROM/docs,
  `jagcore.h`, `jaguar.inc`, DOS-бинарники.

## Сравнение эмуляторов по юридическому риску

| Эмулятор | Лицензия | Юридический риск | Техническая пригодность |
|---|---|---|---|
| **MAME** | GPL-2.0 как целое; файл Jaguar-драйвера — BSD-3-Clause | **Низкий** для локального использования; при распространении — GPL-обязательства | Низкая: `NOT_WORKING`, неполный blitter phrase mode; только трассировка/отладка |
| **Virtual Jaguar Rx** | GPL-3.0 | **Низкий**: явная лицензия; при распространении — source/GPL-обязательства | Средняя/неизвестная: старый эмулятор, точность Z/Gouraud требует проверки; Linux-сборку проверить |
| **libretro Virtual Jaguar** | GPL-3.0 | **Средний/высокий из-за встроенных Atari BIOS**: GPL-код можно использовать внешне, но BIOS-данные требуют отдельной проверки | **Высокая**: активный Jaguar/CD, HLE, отладчик; лучший открытый CD-кандидат |
| **BigPEmu 1.221** | Closed-source freeware, custom restrictive terms в ReadMe (без стандартной лицензии/EULA) | **Высокий**: commercial/redistribution restrictions; нельзя вендорить/включать в payload; локальное использование — на риск maintainer | Высокая: полная совместимость, deterministic, Linux x64 |

**Вывод:** для MIT-проекта GPL-эмулятор допустим только как **внешний
процесс** (не линковать, не включать в payload). Лучший открытый
CD-кандидат — **libretro Virtual Jaguar** (GPL-3.0, BIOS-риск);
Virtual Jaguar Rx — fallback; MAME — трассировка; BigPEmu — только
необязательный локальный gate при явном принятии риска. Технически
лучший вариант (BigPEmu) и юридически наиболее предсказуемый
(GPL-эмулятор как отдельная программа) не совпадают.

## Политика до возобновления

- `jaguar-sdk`, `rmac`/`rln`, Atari docs и DOS-бинарники **не вендорятся**;
  допустим только env-install пользователем.
- SDK/ROM payload не содержит тулчейн, BIOS, чужие ROM и сканы docs.
- Эмулятор не входит в CI, SDK, UI-пакет и release payload.
- Любые публикации метрик — с пометкой «emulator validation, not hardware».
- Планы S47–52 остаются draft/reference; S49–52 не активируются без S47–48.
- Пока действует DEFERRED (ADR-016), Jaguar-компоненты не создаются и не
  добавляются в default build/payload.

## Условия возобновления (если/когда платформа будет возвращена)

> Пока действует DEFERRED, эти условия не применяются. Они фиксируют,
> что должно быть выполнено до активации S47–52.

1. **Maintainer лично и явно разрешает начать Jaguar** — отдельным
   сообщением/решением, с датой и объёмом работ. Это разрешение не
   заменяется снятием LEGAL HOLD, закрытием лицензионных вопросов или
   наличием свободного времени.
2. Maintainer письменно фиксирует политику `env-install only` либо
   получает разрешение правообладателей на vendoring.
3. Выбран baseline-эмулятор: GPL-эмулятор — только внешний процесс;
   BigPEmu — только локальный необязательный инструмент; для libretro
   Virtual Jaguar подтверждён статус встроенных BIOS или собран
   HLE-only билд без них.
4. Для SDK выбран MIT-путь (`lyxass`, `new_bjl`, `jaglib` с заменённым
   `jagcore.h`, GCC/binutils) или получено разрешение на LGPL/иную
   альтернативу; `jaguar-sdk`/Atari libs/BIOS не вендорятся.
5. Подтверждено отсутствие BIOS/ROM/Atari docs в репозитории и payload.
6. ADR-016 (deferral) создан; ADR-017 (детальная интеграция) создаётся
   только при возобновлении. `LEGAL.md`, `VERSION.txt` и планы S47–52
   обновляются тогда же.
7. Если условия не выполнены — Jaguar остаётся DEFERRED; 3DO исключён из
   roadmap, а audio/FS не начинаются автоматически вместо Jaguar.

## Источники

- Jaguar Technical Reference Manual rev 8: <https://www.hillsoftware.com/files/atari/jaguar/jag_v8.pdf>
- jaguar-sdk: <https://github.com/cubanismo/jaguar-sdk>
- MAME Jaguar driver: <https://github.com/mamedev/mame/blob/master/src/mame/atari/jaguar.cpp>
- BigPEmu: <https://www.richwhitehouse.com/jaguar/>
- Virtual Jaguar Rx: <https://github.com/djipi/Virtual-Jaguar-Rx>
- libretro Virtual Jaguar: <https://github.com/libretro/virtualjaguar-libretro>
- `jaglib` (MIT + Atari header): <https://github.com/toarnold/jaglib>
- `lyxass` (MIT): <https://codeberg.org/42Bastian/lyxass>
- `new_bjl` (Unlicense): <https://github.com/42Bastian/new_bjl>
- Подробный аудит: [`jaguar_legal_audit_v2.md`](jaguar_legal_audit_v2.md)
- Ответы по брифу и связка: [`jaguar_emulator_sdk_answers.md`](jaguar_emulator_sdk_answers.md)
- Решение о deferral: [`../adr/ADR-016-jaguar-deferred.md`](../adr/ADR-016-jaguar-deferred.md)
