# Panasonic 3DO (Opera) — аудит open-source эмуляторов и SDK (2026-11-15, архив)

> **Статус:** 3DO **исключён из roadmap** (решение maintainer, 2026-11-15).
> Поддержка 3DO не добавляется; S53+ не используются для 3DO; backend/SDK/сборка
> дисков, подпись и optional/experimental target исключены до отдельного
> пересмотра решения. Аудит сохранён как справочный материал на случай
> «лучших времён». Этот документ — инженерная оценка лицензионных рисков,
> **не юридическое заключение**.
> Проверялись только первоисточники: тексты лицензий, README и исходники
> проектов, ARM EULA, каталог патентов и публичные заявления о товарном знаке.
> Формальный patent FTO и проверка цепочки прав на активы 3DO не выполнялись.

## 1. Короткий вывод

1. **Полного MIT/BSD-clean SDK для 3DO в аудите не найдено.** Оригинальные
   Portfolio SDK/Toolkit, библиотеки, заголовки, документация и компилятор
   Norcroft — проприетарные. Публикация исходников Portfolio OS на
   Archive.org/GitHub **не является лицензией**: в самих файлах стоит
   «unpublished work … all rights reserved … confidential».
2. **Лучший открытый эмулятор — `libretro/opera-libretro`**, но его нельзя
   считать OSI-open-source: ядро унаследовано от FreeDO и распространяется под
   **modified GNU LGPL с запретом коммерческого использования**; сверху
   встроен `opera-boot` под **GPL-2.0-only**. Это источник лицензионного
   конфликта, а не «просто LGPL».
3. **`4DO` и `5DO` используют тот же FreeDO-core.** У `4DO` верхний LICENSE —
   GPL-2.0, но файлы ядра несут FreeDO-уведомление с non-commercial
   ограничением; у `5DO` это прямо заявлено в LICENSE. GPL-ярлык не отменяет
   ограничения FreeDO.
4. **`CrownParkComputing/retro3do-core` — MIT-ядро, но не clean-room.** Автор
   прямо пишет, что читал FreeDO/Opera и что часть routines исторически была
   близким портом; FreeDO-уведомление запрещает коммерческое использование
   «knowledge obtained by studying» исходников. MIT покрывает только copyright
   Crown Park и не снимает чужие права.
5. **`trapexit/3do-devkit` — юридически самый опасный компонент экосистемы.**
   В нём одновременно: Norcroft/ARM SDT под EULA без права перераспространения,
   оригинальные 3DO libraries/headers/docs, проприетарный OS/kernel и
   `boot_code` (пропатченный Game Guru), логотип 3DO и `3dt`, который умеет
   подписывать диски retail-ключами. Вендорить/использовать нельзя.
6. **BIOS/ROM 3DO — проприетарные.** `panafz1.bin`, `panafz10.bin`, Kanji ROM
   и особенно `-norsa`-варианты нельзя включать в репозиторий, CI, SDK или
   release payload. `-norsa` — это производная от retail BIOS с отключённой
   RSA-проверкой, то есть ещё и риск anti-circumvention.
7. **`opera-boot` — полезная открытая альтернатива retail BIOS, но GPL-2.0-only
   и только для эмулятора.** Он не содержит Portfolio OS, не проверяет RSA,
   загружает OS с диска; совместимость с реальным FZ-1 не подтверждена. В
   MIT-SDK/релиз он не входит.
8. **RSA-подпись дисков — отдельный критический риск.** `3dt` содержит
   retail private exponents для ключей `3do`/`app` и умеет `sign`/`sign-file`.
   Подпись произвольных дисков retail-ключами — это не «homebrew-инструмент»,
   а обход технической меры защиты и потенциальное присвоение секрета.
9. **Товарный знак 3DO жив.** По публичным заявлениям 2026 г., консольный
   бренд 3DO, дизайны железа и домен `3do.com` контролирует **Throwback
   Entertainment**; Empire Interactive получила только publishing-класс
   «The 3DO Company» и отказалась от проекта. Использовать «3DO»/«Panasonic» в
   названии продукта, логотипы и BIOS-splash — высокий риск.
10. **Патенты, вероятнее всего, истекли, copyright — нет.** Идентифицированные
    патенты 3DO — подачи 1993–1997 гг. (плюс публикация 2003 г.); по 20-летнему
    сроку они истекли. Но BIOS/OS/SDK/документация охраняются десятилетиями
    (US: до 95 лет с публикации для works made for hire), и «abandonware» — не
    юридическая защита.
11. **Решение:** **3DO исключён из roadmap; поддержка не добавляется.**
    S53+ не используются для 3DO, backend/SDK/сборка дисков/подпись не начинаются,
    optional/experimental target не добавляется.
    Если решение когда-либо будет пересмотрено, возврат возможен только через
    clean-room-трек и письменное юридическое заключение. Audio/FS развиваются
    отдельно и не начинаются автоматически «вместо 3DO».

## 2. Критерии оценки

- **MIT-совместимые лицензии для кода, который линкуется/копируется в движок:**
  MIT, BSD-2/3-Clause, Apache-2.0 (с patent-оговоркой), ISC, Zlib,
  Unlicense/CC0. Требуется сохранять copyright/NOTICE.
- **LGPL-2.1:** можно использовать при выполнении условий LGPL (исходники
  библиотеки, возможность повторной линковки, уведомления). Для статического
  консольного ROM это практические обязанности; «просто MIT» не получается.
- **GPL-2.0/3.0:** нельзя включать в MIT-проект как библиотеку без перевода
  производной работы под GPL. Отдельный процесс — другой режим; границу должен
  подтвердить юрист.
- **Non-commercial / source-available:** лицензия, запрещающая коммерческое
  использование, **не является OSI-open-source** и несовместима с MIT/GPL для
  коммерческого продукта. Даже если код лежит на GitHub — это не open source.
- **«Knowledge»-оговорки:** формулировки вида «any knowledge obtained by
  studying the sources» могут распространяться шире, чем copyright на код.
  Они особенно опасны для clean-room: чтение такого кода загрязняет
  независимую реализацию.
- **Сборочные инструменты:** лицензия компилятора/ассемблера/линкера не
  переходит на его вывод; GCC дополнительно даёт Runtime Library Exception для
  libgcc/заголовков. Но проприетарный компилятор может запрещать
  перераспространение самого toolchain.
- **Эмулятор:** отдельный исполняемый файл/процесс не является частью движка;
  при распространении эмулятора действуют его лицензия и обязательства, но
  MIT-код движка остаётся MIT. Non-commercial/GPL-эмулятор нельзя
  «протаскивать» в SDK/release payload.
- **Ассеты и BIOS:** ROM/BIOS/OS/логотипы/документация — отдельные объекты
  авторского права и товарных знаков; availability на 3dodev/Archive.org не
  равна лицензии.
- **Anti-circumvention:** обход RSA-проверки, подпись retail-ключами и
  распространение модифицированного BIOS — отдельный риск по DMCA §1201 и
  аналогам в ЕС, даже если сама консоль снята с производства.

## 3. Эмуляторы 3DO

| Эмулятор | Лицензия (первоисточник) | 3DO CD / BIOS | Техническая пригодность | Юридический режим |
|---|---|---|---|---|
| [`libretro/opera-libretro`](https://github.com/libretro/opera-libretro) | **FreeDO modified LGPL + non-commercial** для ядра; LGPL-2.1+ для вклада trapexit; встроенный `opera-boot` — **GPL-2.0-only** ([LICENSE](https://github.com/libretro/opera-libretro/blob/master/LICENSE), [opera_boot.h](https://github.com/libretro/opera-libretro/blob/master/opera_boot.h)) | CD: `.cue/.bin`, CHD; BIOS не обязателен — по умолчанию встроенный `Opera Boot`; retail BIOS опционален ([README](https://github.com/libretro/opera-libretro#readme)) | Высокая: активный проект, CD, HLE/BIOS, отладка, libretro | **Только внешний процесс**, некоммерческая проверка; не линковать, не вендорить, не включать в SDK/release. Лицензии внутри конфликтуют |
| [`tordona/4DO`](https://github.com/tordona/4DO) (modernized fork; оригинал — SourceForge) | Верхний **GPL-2.0** ([LICENSE](https://github.com/tordona/4DO/blob/vs2026/LICENSE)), но `FreeDOCore/FreeDO/*` несут **FreeDO modified LGPL + non-commercial** | CD; требует retail BIOS | Средняя: Windows-only форк, ядро FreeDO; неактивен как развитие | GPL-ярлык **не чистый**: ограничения FreeDO остаются. Только внешний процесс/legacy, не вендорить |
| [`purelogiccode/5DO`](https://github.com/purelogiccode/5DO) | **FreeDO modified LGPL + non-commercial**, source-available ([LICENSE](https://github.com/purelogiccode/5DO/blob/master/LICENSE)) | CD; BIOS не входит, пользовательский | Высокая для C#/.NET: активный, deterministic, Avalonia/CLI; но зрелость и точность требуют проверки | **Commercial use запрещён**; не OSI-open-source. Не вендорить; 4DO game DB имеет неразрешённый статус (см. их `THIRD-PARTY-NOTICES.md`) |
| [`CrownParkComputing/retro3do-core`](https://github.com/CrownParkComputing/retro3do-core) | **MIT** ([LICENSE](https://github.com/CrownParkComputing/retro3do-core/blob/cpc/LICENSE)), но с раскрытым **non-clean-room** provenance ([PROVENANCE](https://github.com/CrownParkComputing/retro3do-core/blob/cpc/docs/PROVENANCE.md)) | CD/CHD; требует BIOS; есть project-owned demo ROM | Ранняя: создан 2026-09, ~254 теста; ARM60/MADAM/CLIO/DSP/SPORT/VDLP/XBUS/CHD | MIT-ярлык **не означает clean**. FreeDO «knowledge»-оговорка и исторические порты → только после отдельного legal review; не копировать в движок |
| [MAME](https://github.com/mamedev/mame) | **GPL-2.0** как целое ([COPYING](https://github.com/mamedev/mame/blob/master/COPYING)); 3DO-драйвер — **BSD-3-Clause** ([3do.cpp](https://github.com/mamedev/mame/blob/master/src/mame/misc/3do.cpp)) | CD; требует retail BIOS (`panafz*.bin` и т.п.) | Низкая для acceptance: консольные драйверы `MACHINE_NOT_WORKING`, `MACHINE_IMPERFECT_SOUND`, `MACHINE_IMPERFECT_TIMING` | Только трассировка/отладка; BIOS не распространять. Выделение драйвера в MIT-эмулятор — отдельный проект |
| [Phoenix](http://www.arts-union.ru/node/23) | **Closed-source freeware**; публичной лицензии нет; 3dodev прямо пишет «closed source» ([3dodev](https://3dodev.com/software/emulation)) | CD; требует BIOS | Средняя/высокая по отзывам, но закрытый | **Не вендорить, не включать в payload**; только локальный необязательный инструмент при принятии риска |
| [`arcadenea/3doh`](https://github.com/arcadenea/3doh), `3Doh!`/`3DeeOH` | **Нет LICENSE** у `3doh`; `guaripolo/3DeeOH` — GPL-2.0, но репозиторий-заглушка | CD; BIOS | Низкая: старые Android/Linux порты, не поддерживаются | Не использовать: нет ясной лицензии/поддержки |
| FreeDO (оригинал) | **FreeDO modified LGPL + non-commercial**; исходники 2010 г. | CD; BIOS | Низкая: исторический core, не поддерживается | Только как источник знаний/истории; commercial use запрещён |

### 3.1. Лицензионная ловушка Opera/FreeDO

FreeDO LICENSE — не обычная LGPL. Текст прямо говорит:

- владельцы/авторы FreeDO имеют право делать closed-source производные;
- любое **non-commercial** использование исходников или «knowledge obtained by
  studying or reverse engineering» должно сопровождаться полными кредитами;
- любое **commercial** использование исходников или таких знаний **строго
  запрещено** без разрешения владельцев;
- эти условия имеют приоритет над GNU LGPL при конфликте.

Это не OSI-open-source (non-commercial restriction) и не GPL-совместимая
лицензия. `opera-libretro` добавляет LGPL-2.1+ только для вклада trapexit, но
не может перелицензировать унаследованный FreeDO-код. Встроенный
`opera-boot` — GPL-2.0-only; GPL требует свободы коммерческого использования,
а FreeDO-условия её запрещают. Итог: **распространение комбинированного
бинаря Opera юридически противоречиво**; нужна отдельная проверка, а не
надежда на «LGPL».

Для проекта это значит:

- Opera можно использовать как **внешний процесс** для некоммерческой
  разработки/проверки, с кредитами и без вендоринга;
- если проект коммерческий, даже QA-использование Opera может попасть под
  «commercial use» — нужен письменный ответ/разрешение;
- нельзя включать Opera/4DO/5DO в SDK, installer, Docker-образ релиза, CI
  artifact или release payload;
- нельзя копировать код Opera/FreeDO в MIT-движок или в «clean-room» ветку.

### 3.2. 4DO и 5DO

`4DO` заявляет GPL-2.0 на весь репозиторий, но базовые файлы `FreeDOCore`
содержат FreeDO-уведомление с non-commercial ограничением. Это не «GPL-2.0 с
исключением», а **конфликт лицензий**: GPL-2.0 не допускает дополнительных
ограничений на коммерческое использование. Поэтому 4DO нельзя считать
«юридически чистым GPL-эмулятором»; его GPL-ярлык не расширяет права,
которые дал FreeDO.

`5DO` честнее: LICENSE прямо воспроизводит FreeDO modified LGPL и
non-commercial условия; README называет проект source-available. Дополнительно
их собственный `THIRD-PARTY-NOTICES.md`/`docs/ReleaseLegalReview.md` признаёт,
что лицензионный статус 4DO game database не разрешён. Это хороший пример
того, почему нельзя импортировать «базу совместимости» или метаданные без
отдельной проверки.

### 3.3. `retro3do-core`: MIT, но не clean-room

Это единственное найденное ядро с MIT-лицензией, но его `docs/PROVENANCE.md`
прямо говорит:

- проект **не clean-room**;
- при разработке читались FreeDO-производные исходники Opera и
  `4DO-Android`, некоторые routines были близким портом;
- позже routines переписали, но историческая экспозиция остаётся;
- FreeDO notice «purports to restrict commercial use of knowledge gained from
  its source»;
- `NOTICE` говорит, что MIT покрывает только copyright Crown Park и **не**
  даёт прав третьих лиц.

Практический вывод: **MIT-файл не делает ядро MIT-clean**. Если использовать
его как внешний эмулятор для некоммерческой проверки — риск ниже; если
копировать код, линковать или встраивать в коммерческий продукт — нужен
отдельный legal review и, вероятно, разрешение FreeDO-авторов. Также проект
очень ранний (создан в сентябре 2026), поэтому техническая зрелость
не подтверждена.

### 3.4. MAME

MAME как целое — GPL-2.0; файл 3DO-драйвера помечен BSD-3-Clause. Это не
означает, что драйвер можно вырвать и сделать MIT-эмулятор: он тесно связан с
MAME-core и устройствами (`3do_clio`, `cr560b`, `3do_madam` и т.д.).
Технически консольные драйверы 3DO всё ещё `MACHINE_NOT_WORKING` и требуют
retail BIOS. MAME годится для трассировки/сравнения поведения, но не как
acceptance-эмулятор и не как источник кода для MIT-движка.

### 3.5. Закрытые и нелицензированные эмуляторы

- **Phoenix** — closed-source freeware; по 3dodev «source code: none». Нельзя
  вендорить/включать в payload; локальное использование — на риск maintainer.
- **3Doh/3DeeOH** — старые порты без ясной лицензии/поддержки; не
  рассматривать.
- **FreeDO** — исторический core; non-commercial; не поддерживается.

### 3.6. «MIT-обёртки» и dev-kits

- [`geekychris/3do-dev`](https://github.com/geekychris/3do-dev) — MIT-обёртка,
  но `setup.sh`/`scripts/fetch-bios.sh` скачивают retail BIOS (включая
  `panafz10-norsa.bin`) с 3dodev.com, а toolchain тянет `3do-devkit` и
  патченный Opera. MIT-лицензия репозитория прямо исключает скачиваемые
  компоненты; это не делает связку легальной.
- [`webrcade/webrcade-app-retro-opera`](https://github.com/webrcade/webrcade-app-retro-opera)
  — Apache-2.0 обёртка, но core — Opera/FreeDO с non-commercial условиями.
  Apache-лицензия обёртки не меняет лицензию ядра.
- `libretro` core info помечает Opera как `LGPL/Non-commercial` — это
  подтверждает, что проект не считает её обычной LGPL.

## 4. SDK и toolchain

| Компонент | Лицензия (первоисточник) | Что это | Можно ли использовать в MIT-проекте | Вывод |
|---|---|---|---|---|
| 3DO Portfolio 2.5 SDK / Toolkit 1.5 | **Проприетарная**, лицензия не найдена; оригинальный SDK распространялся по коммерческой лицензии | Компилятор, библиотеки, заголовки, OS, tools, документация | **Нет** | Не вендорить, не включать в SDK/release |
| [`trapexit/3do-devkit`](https://github.com/trapexit/3do-devkit) | **Нет top-level LICENSE** (GitHub API: null); внутри — ARM EULA, оригинальные 3DO libraries/headers/docs | «Turnkey» devkit: Norcroft ARM C/C++, 3DO libs, OS/kernel, examples, `3dt` | **Нет** | Юридически самый опасный bundle; не вендорить, не использовать в CI/release |
| [`trapexit/portfolio_os`](https://github.com/trapexit/portfolio_os) | **Нет LICENSE**; в файлах — «Copyright (C) 1995, an unpublished work by The 3DO Company. All rights reserved… confidential» ([fix_copyright.c](https://github.com/trapexit/portfolio_os/blob/master/fix_copyright.c)) | Исходники Portfolio OS (Opera), опубликованы EagleSoft в 2022 | **Нет** | Publication ≠ license. Не читать/не копировать в clean-room; только историческая справка |
| 3DO SDK images на [3dodev.com/software/sdks](https://3dodev.com/software/sdks) | Лицензия на образы не указана; вики-текст — CC BY-SA 4.0, но бинарники — «except where otherwise noted» | Portfolio 1.3/2.5, Toolkit 1.3–1.5, ARM SDT 2.50/2.51, M2 SDK | **Нет** | Availability ≠ license; не скачивать в CI/не вендорить |
| [`XProger/3do-docs`](https://github.com/XProger/3do-docs) | **Нет LICENSE** | Зеркало 3DO SDK documentation | **Нет** для копирования текста | Факты можно перепроверять; текст/картинки не копировать |
| [`vs-sr-dev/3dokit`](https://github.com/vs-sr-dev/3dokit) | **MIT** ([LICENSE](https://github.com/vs-sr-dev/3dokit/blob/main/LICENSE)) | ARM60 static recompiler, OperaFS, Portfolio-runtime `pfboot`, CEL/DataStream/audio | **Ограниченно**: это toolkit для PC-портов, не 3DO SDK; `sdk_tables.py` сгенерирован из SDK headers/libraries; `pf_os` повторяет поведение OS | Не использовать как SDK/OS для 3DO; возможный derivative/clean-room риск |
| [`gameblabla/exp3DOsdk`](https://github.com/gameblabla/exp3DOsdk) | **MIT** для bridge-тулинга, но `LICENSES.md` относит `sdk/include`, `sdk/lib`, `sdk/build`, `sdk/src`, `sdk/takeme`, `sdk/iso` к отдельной категории «compatibility exceptions» | GCC 8.5 + старые 3DO libraries; «huge mess» | **Нет**: MIT не покрывает SDK-входы | Не считать MIT-clean; не вендорить |
| `3dt` / `3it` / `3at` / `3ct` / `modbin` | **ISC** (3dt, 3it, 3at, 3ct, modbin) | Дисковые/аудио/компрессионные утилиты; `3dt` умеет `sign` | **3dt — нет** из-за retail RSA-ключей; остальные — только после проверки | Инструменты ISC, но `3dt sign` — критический риск |
| [`trapexit/opera-boot`](https://github.com/trapexit/opera-boot) | **GPL-2.0-only** ([LICENSE](https://github.com/trapexit/opera-boot/blob/master/LICENSE)) | Открытый boot ROM 128 KiB, загружает OS с диска, без RSA | **Нет** для MIT-SDK/релиза; можно как внешний CI-артефакт с GPL-комплаенсом | Полезен для эмуляторной проверки; не для retail hardware |
| Norcroft ARM C/C++ / ARM SDT 2.51 | **Проприетарная ARM EULA** ([license.txt](https://github.com/trapexit/3do-devkit/blob/master/docs/compilers/license.txt)) | Компилятор/ассемблер/линкер | **Нет**: EULA запрещает sublicense/передачу и ограничивает копирование | Не вендорить; только локальная установка по отдельной лицензии |
| GCC/binutils (GCC 8.5 path) | **GPL-3.0+/GPL-2.0** + GCC Runtime Library Exception | Компилятор C/C++ | **Да для вывода**; сам toolchain — по GPL | Build tool; output не становится GPL; но нет clean libs/OS |
| `trapexit/3do-libc`, `3do-cpplib`, `3do-svc-funcs`, `3do-svc-mem-device`, `3do-example-folio` | **Нет LICENSE** (GitHub API: null) | Замены C/C++ stdlib/сервисные библиотеки | **Нет** до получения лицензии | Не считать MIT; спросить автора |

### 4.1. Оригинальный SDK и Portfolio OS

Оригинальный 3DO SDK — это не только «заголовки». В `3do-devkit` видны:

- `include/3do/*.h` — оригинальные заголовки (graphics, audio, filesystem,
  cplusswi, hardware и т.д.);
- `lib/3do/*.lib` — оригинальные библиотеки (`graphics.lib`, `audio.lib`,
  `filesystem.lib`, `operamath.lib`, `lib3do.lib`, `clib.lib` и др.);
- `docs/3dosdk` — полный Developer's Documentation Set;
- `examples/original/Portfolio 1.2/2.5` — оригинальные примеры;
- `takeme/System/Kernel` — OS/kernel/boot code;
- `takeme/Art/3do_logo_unpacked.cel` — логотип 3DO.

Ни у одного из этих компонентов нет лицензии, разрешающей перераспространение.
Оригинальный SDK был проприетарным; публикация на 3dodev/Archive.org не
меняет этого. **Нельзя** брать из него заголовки «как факты», библиотеки «для
совместимости» или OS «для тестового диска» без письменного разрешения
правообладателя.

### 4.2. `3do-devkit`: почему это критический риск

`3do-devkit` — удобный, но юридически собранный из чужих прав bundle:

- **Norcroft/ARM SDT** (`bin/compiler/linux/armcc`, `armcpp`, `armasm`,
  `armlink`, Windows-версии). ARM EULA: non-exclusive, non-transferable,
  no sublicense; копирование ограничено одним компьютером/backup; нельзя
  распространять документацию; распространять можно только производный
  binary code, встроенный в приложение. Перераспространение компилятора в
  devkit по тексту EULA не разрешено. То, что ARM когда-то предоставила
  копии автору devkit, не даёт прав третьим лицам.
- **Оригинальные 3DO libraries/headers/docs** — без лицензии.
- **OS/kernel/boot_code** — проприетарный Portfolio OS; `boot_code` —
  пропатченный Game Guru bootstrap (два NOP для снятия boot-drive
  restriction), то есть производная от чужого кода.
- **3DO logo/art** — товарный знак/авторское право; `takeme/Art/3do_logo_unpacked.cel`.
- **`3dt`** — ISC, но содержит retail RSA private exponents и умеет
  подписывать ISO для retail-консолей. `3do-devkit` вызывает `3dt pack --sign`
  в default `make`.
- **`download-retroarch-opera-roms`** — скачивает `panafz1.bin` и
  `panafz1j-kanji.bin` с 3dodev.com, то есть retail BIOS/Kanji ROM.

Даже если не использовать `3dt sign`, сам факт наличия ключей и патченного
boot code в зависимом devkit создаёт риск для репозитория и релиза. **Не
вендорить, не добавлять в CI-образ, не устанавливать как dependency.**

### 4.3. Portfolio OS source: publication ≠ license

`portfolio_os` README говорит, что это копия исходников Portfolio OS,
опубликованных EagleSoft на Archive.org 2022-01-07. Но в исходниках стоит
явное уведомление The 3DO Company:

> «Copyright (C) 1995, an unpublished work by The 3DO Company. All rights
> reserved. This material contains confidential information that is the
> property of The 3DO Company. Any unauthorized duplication, disclosure or use
> is prohibited.»

Это не public domain и не «открытый исходник». Публикация могла быть
несанкционированной; права на активы 3DO фрагментированы. Для clean-room это
означает: **не читать этот код** исполнителям clean-room-реализации, иначе
будет трудно доказать независимость. Использовать можно только публичные
патенты, hardware-документацию и наблюдаемое поведение.

### 4.4. `3dokit`: MIT, но не SDK

`3dokit` — MIT-инструментарий для reverse engineering и PC-портов:

- `sdk_tables.py` сгенерирован из заголовков и библиотек SDK 1.2/1.3/2.5;
  автор утверждает, что это «numbers and names, no code». API-факты могут
  быть неохраняемыми, но генерация из проприетарных заголовков — риск.
- `runtime/pf_os.cpp` и другие файлы реализуют Portfolio OS surface; README
  говорит, что runtime «answers every OS call the way the console's own OS
  code does». Это может быть как самостоятельная реализация по поведению, так
  и производная от дизассемблированного OS-кода; provenance не раскрыт.
- Это **не** SDK для сборки 3DO-дисков: он не даёт компилятор, AIF/AOF
  toolchain, OS для диска, CEL/MADAM HAL. Он полезен для портирования
  существующих игр на PC, а не для backend-а движка.

Если использовать `3dokit`, то только как внешний инструмент/референс, с
отдельной проверкой provenance; не копировать `pf_os`/`sdk_tables` в движок.

### 4.5. `exp3DOsdk`: MIT-обёртка вокруг проприетарного SDK

`exp3DOsdk` — попытка собрать GCC 8.5 toolchain для ARMv3/ARM60. MIT-лицензия
покрывает только bridge-тулинг (скрипты, `aof2elf.py`, `elf2aif.py`,
`patch_*`), но `LICENSES.md` прямо перечисляет «embedded 3DO SDK compatibility
inputs»: `sdk/include`, `sdk/lib`, `sdk/build`, `sdk/src`, `sdk/takeme`,
`sdk/iso/helloworld.iso`. Это оригинальные 3DO headers/libraries/startup
objects/ISO. Они не становятся MIT от того, что лежат рядом.

README сам называет проект «huge mess» и не рекомендует его для реальных
проектов. Для аудита: **не считать MIT-clean, не вендорить, не использовать
как доказательство существования открытого SDK.**

### 4.6. `3dt` и retail RSA-ключи

`3dt` — ISC-утилита, но в `src/tdo_keys.cpp` лежат:

- `M1_RETAIL_3DO_N/D` — modulus и private exponent системного ключа 3DO;
- `M1_RETAIL_APP_N/D` — modulus и private exponent application-ключа;
- `docs/signing.md` публикует эти значения и описывает формат подписи;
  документ ссылается на `portfolio_os/src/dipir/*` как на источник описания,
  что усиливает вопрос происхождения ключей.

Это позволяет подписывать произвольные ISO так, чтобы retail BIOS их принял.
Юридические риски:

- **anti-circumvention**: подпись/обход проверки — потенциальный DMCA §1201
  и аналоги в ЕС;
- **trade secret / misappropriation**: private key — не «просто число»;
  его происхождение и право на использование не подтверждены;
- **piracy facilitation**: инструмент позволяет создавать диски, неотличимые
  для консоли от лицензионных;
- **license scope**: ISC покрывает copyright на код, но не даёт прав на
  ключевой материал и не решает вопрос anti-circumvention.

**Не использовать `3dt sign`/`sign-file`, не вендорить `3dt`, не хранить
ключи в репозитории, CI или релизе.** Для homebrew-сборок без подписи
использовать эмулятор/`opera-boot`; для реального железа — только законно
приобретённые инструменты/разрешение правообладателя.

### 4.7. `opera-boot`: открытый boot ROM, но GPL-2.0

`opera-boot` — открытая альтернатива retail boot ROM:

- 128 KiB, big-endian ARM60/ARMv3, MADAM/CLIO/MEI CD;
- не содержит Portfolio OS; загружает OS с диска и передаёт управление;
- нет RSA/signature verification, нет Kanji/audio/photo-CD;
- physical FZ-1 compatibility не установлена;
- лицензия — **GPL-2.0-only**.

Плюс: для эмулятора не нужен retail BIOS, и это снижает BIOS-риск. Минус:
GPL-2.0 не MIT-совместима; нельзя включать в MIT-SDK/релиз без GPL-
обязательств. Для CI можно собирать из source в отдельной job и не публиковать
бинарник; для acceptance — только внешний ROM. `opera-libretro` уже встраивает
его, но это создаёт конфликт с FreeDO non-commercial условиями.

### 4.8. Компиляторы: Norcroft vs GCC

- **Norcroft/ARM SDT** — проприетарный, EULA ограничивает перераспространение;
  вывод компилятора, вероятно, можно использовать в своих приложениях, но сам
  toolchain — нет. Для проекта это неприемлемо как dependency.
- **GCC/binutils** — GPL, но GCC Runtime Library Exception и GPL FAQ
  подтверждают, что вывод программы не обязан быть GPL. Это позволяет
  использовать GCC для сборки MIT-кода. Но для 3DO нужны AIF/AOF, startup,
  библиотеки и OS; GCC-путь `exp3DOsdk` тащит проприетарные SDK-входы.
- **Clean-room toolchain** (свой linker/AIF converter + GCC) — возможен, но
  это отдельный крупный проект; 4–6 спринтов на backend его не покрывают.

## 5. BIOS, ROM, OS и ассеты

- **Retail BIOS** (`panafz1.bin`, `panafz10.bin`, `panafz1j.bin`,
  `goldstar.bin`, `sanyotry.bin`, `3do_arcade_saot.bin` и др.) — copyrighted
  firmware; лицензии на распространение нет. Не вендорить, не включать в CI,
  SDK, Docker, release payload.
- **Kanji ROM** (`panafz1j-kanji.bin`, `panafz10ja-anvil-kanji.bin`) — тоже
  copyrighted; не распространять.
- **`-norsa` BIOS** — модифицированный retail BIOS с отключённой RSA-проверкой.
  Это производная работа + potential anti-circumvention. Не использовать, не
  скачивать автоматически, не публиковать.
- **Portfolio OS на диске** — каждый bootable 3DO-диск, собранный с SDK,
  содержит `system/kernel/os_code`, `boot_code`, `misc_code` и audio DSP.
  Это проприетарный OS-код. Поэтому нельзя коммитить/публиковать тестовые
  3DO-диски, собранные через `3do-devkit`, даже если игра/движок — MIT.
- **3DO logo/art** — товарный знак/авторское право. В `3do-devkit` есть
  `takeme/Art/3do_logo_unpacked.cel` и `BannerScreen`; не использовать.
- **Game ISO/ROM** — только пользовательские, локально; не в CI, не в
  release, не в публичных артефактах.
- **3dodev.com/Archive.org** — availability не равна license. Вики-текст
  может быть CC BY-SA 4.0, но hosted SDK/BIOS/ROM images — «except where
  otherwise noted» и не покрыты этой лицензией.
- **`opera-boot`** — не retail BIOS и не OS; он не решает проблему
  проприетарного Portfolio OS на диске.

## 6. Товарные знаки, copyright, патенты, anti-circumvention

### 6.1. Товарный знак 3DO

По публичным заявлениям 2026 г.:

- **Throwback Entertainment** владеет товарным знаком 3DO для консоли/железа,
  связанными дизайнами и доменом `3do.com`; активы не проданы/не
  лицензированы Empire Interactive;
- **Empire Interactive** заявила о покупке «The 3DO Company» brand
  (publishing-класс), но после спора отказалась от 3DO-проекта;
- значит, для консольного backend релевантен именно Throwback, а не
  publishing-марка Empire.

Практические правила:

- не использовать «3DO»/«Panasonic» в названии продукта, SDK, компании или
  домена;
- не использовать логотип 3DO, BIOS splash, «Panasonic 3DO» trade dress;
- описательное «3DO-compatible»/«for 3DO» — возможный nominative fair use, но
  требует юридической проверки и disclaimer;
- «Opera» — одновременно кодовое имя 3DO и товарный знак Opera Software;
  не использовать как имя продукта/backend;
- в README/маркетинге: «RetroPolyEngine не связан с Panasonic, The 3DO Company,
  Throwback Entertainment; 3DO — товарный знак соответствующего владельца».

### 6.2. Copyright

- 3DO BIOS/OS/SDK/документация созданы в 1992–1996 гг. как works made for
  hire; в США срок — до 95 лет с публикации (17 U.S.C. §302(c)), в ЕС — 70 лет
  (для анонимных/служебных произведений — с публикации). В 2026 г. они **не**
  в public domain.
- «Abandonware», «консоль снята с производства», «правообладатель исчез» — не
  юридические основания для копирования/распространения.
- Права на активы 3DO фрагментированы (банкротство 2003 г., разные
  приобретатели игрового каталога, товарного знака, SDK/OS). Даже если найти
  одного «владельца», может потребоваться цепочка прав. Это повышает риск
  orphan works и неопределённости.

### 6.3. Патенты

- На 3dodev.com/documentation/patents собраны патенты 3DO: WO 94/10636,
  WO 94/10641, WO 94/10643, WO 94/10644, WO 94/16382, WO 94/16383,
  US 5,502,462, US 5,572,235, US 6,009,458, US 6,005,561 и др.
- Основные подачи — 1993–1997 гг.; по 20-летнему сроку с даты подачи они
  истекли к ~2017 г. Публикация 2003 г. (`US20030195907`) — вероятно, истекла
  к ~2022–2023 г.
- **Активных патентов, блокирующих эмуляцию/backend, в аудите не найдено.**
  Но это не формальный FTO: возможны continuation/divisional, foreign
  counterparts и патенты, не попавшие в список. Для коммерческого релиза
  нужен отдельный patent search.
- Истёкшие патенты — хороший источник публичной документации для clean-room
  реализации CEL/MADAM/VDLP; использовать их можно, но не копируя текст
  SDK-документации.

### 6.4. Anti-circumvention и подпись

- **Retail RSA private keys** в `3dt` и подпись дисков — главный
  anti-circumvention риск. DMCA §1201 (17 U.S.C.) запрещает обход
  технологических мер защиты; аналоги есть в ЕС (Directive 2001/29/EC, Art. 6)
  и других юрисдикциях. Исключения для preservation/interoperability узкие и
  требуют юридической оценки.
- **`-norsa` BIOS** — модификация retail firmware для отключения проверки;
  это и derivative work, и обход.
- **`opera-boot`** не проверяет RSA; это открытый boot ROM, но его
  использование для запуска неподписанных дисков всё равно может
  рассматриваться как обход. Для эмулятора риск ниже, для реального железа —
  выше; нужна юридическая проверка.
- **Не предлагать пользователям** workflow «собери ISO → подпиши retail-ключом
  → запиши на CD/R»; это прямое вовлечение проекта в обход.
- Для CI/acceptance использовать только `opera-boot`/demo ROM и
  пользовательские образы; не распространять подписанные retail-совместимые
  диски.
- Прецеденты reverse engineering для interoperability (`Sega v. Accolade`,
  1992; `Sony v. Connectix`, 2000) и API-решения (`Google v. Oracle`, 2021)
  помогают защитить независимую реализацию интерфейса/поведения, но **не**
  легализуют копирование кода, распространение BIOS/OS или обход защиты.
  DMCA-исключения для preservation (библиотеки/архивы) не покрывают
  коммерческий продукт или публичный CI.

### 6.5. Clean-room policy

Если 3DO backend когда-либо будет реализован:

- нельзя читать `portfolio_os`, оригинальные SDK headers/libs, `3do-devkit`,
  `3dokit`/`retro3do-core` код, дизассемблер OS/BIOS;
- можно использовать публичные патенты, hardware-мануалы, наблюдаемое
  поведение, собственные тесты;
- нужно вести provenance log: кто, что и когда читал; для clean-room
  исполнителей — письменное подтверждение;
- `retro3do-core` и `3dokit` **не** являются clean-room источниками; их
  MIT-лицензии не заменяют независимую разработку.

## 7. Реестр рисков

| Риск | Уровень | Триггер | Митигация |
|---|---|---|---|
| Вендоринг Opera/4DO/5DO в SDK/release | **Критический** | GPL + FreeDO non-commercial конфликт | Только внешний процесс; не включать в payload; legal review |
| Копирование `retro3do-core`/`3dokit` кода | **Высокий** | Non-clean-room provenance, FreeDO «knowledge» clause | Не копировать; отдельный legal review; clean-room log |
| Вендоринг `3do-devkit` | **Критический** | ARM EULA, SDK libs/docs, OS, boot code, logo | Запретить; не добавлять в CI/релиз |
| `3dt sign` / retail RSA keys | **Критический** | Anti-circumvention, trade secret | Не использовать/не вендорить; сканировать repo на ключи |
| BIOS/ROM/`-norsa` в repo/CI/payload | **Критический** | Copyright, derivative, circumvention | Пользовательский BIOS локально; verifier; no CI artifacts |
| Логотип 3DO/Panasonic в UI/маркетинге | **Высокий** | Trademark | Свои ассеты; descriptive use + disclaimer |
| Копирование SDK docs/wiki | **Средний/высокий** | Copyright; CC BY-SA share-alike | Только факты; парафраз; не копировать HTML/PDF |
| Тестовые диски с Portfolio OS | **Высокий** | Proprietary OS в артефакте | Не коммитить/не публиковать; clean-room OS |
| CI скачивает BIOS/SDK | **Высокий** | Автоматическая дистрибуция | Запретить сетевые fetch; pinned local/user-provided |
| Commercial use of Opera для QA | **Средний/высокий** | FreeDO non-commercial clause | Письменное разрешение или некоммерческий режим |
| Патенты | **Низкий** | Истекли, но FTO неполный | Formal FTO перед коммерческим релизом |
| Provenance вкладов | **Высокий** | Код из copyleft/non-clean-room | DCO/CLA, provenance log, license scan |

## 8. Варианты интеграции

> **3DO исключён из roadmap.** Варианты ниже — не план работ и не обещание
> поддержки; они сохранены как анализ на случай пересмотра решения.

### Вариант A — external emulator adapter

Добавить в UI/инструменты профиль внешнего эмулятора, без линковки:

```text
tools/emulators/profiles/
  retroarch-opera.json        # Opera/FreeDO, non-commercial, external
  retro3do-core.json          # MIT, но provenance risk; optional
  phoenix.json                # closed-source, optional, manual only
  mame-3do.json               # GPL-2.0, trace only, not acceptance
```

```cpp
struct EmulatorProfile {
  std::string id;
  std::string executable;
  std::vector<std::string> args;   // {rom}, {core}
  std::string license_spdx;        // LGPL-2.1+/Non-commercial, GPL-2.0, ...
  std::string source_url;
  bool requires_bios;
  bool allowed_in_ci;              // false для всех 3DO-профилей по умолчанию
};
```

UI только запускает процесс и собирает результат. Ни один профиль не попадает
в `third_party/`, не линкуется и не входит в SDK/release. BIOS — только
пользовательский, вне репозитория.

### Вариант B — CI/acceptance без retail BIOS

- Собрать `opera-boot` из source в отдельной GPL-job (не публиковать бинарник).
- Использовать `retro3do-core` demo ROM для smoke (но provenance риск;
  только если maintainer принимает).
- Либо запускать внешний Opera с пользовательским BIOS локально, без CI.
- Не использовать commercial game ISO в CI/артефактах.
- Все отчёты помечать «emulator validation, not hardware».

### Вариант C — clean-room backend

- Реализовать ARM60/MADAM/CLIO/VDLP/CEL/DSP/SPORT/XBUS по публичным патентам и
  hardware-документации, без SDK headers/libs/OS.
- Свой HAL, свой AIF/AOF toolchain или GCC-путь без SDK-входов.
- Свой минимальный runtime/OS для тестовых дисков; либо emulator-only
  demo ROM.
- Это отдельный исследовательский трек на много месяцев; 4–6 спринтов
  недостаточно.

### Вариант D — SDK/toolchain

Чистого пути нет:

- `3do-devkit` — нельзя;
- `exp3DOsdk` — MIT только для bridge, SDK-входы проприетарные;
- `3dokit` — не SDK;
- GCC сам по себе не даёт 3DO libs/OS/AIF;
- `3dt sign` — нельзя.

Если 3DO SDK нужен, начинать с clean-room OS/runtime и собственного
AIF/AOF-тулинга, а не с вендоринга исторического SDK.

### Вариант E — разрешения правообладателей

- **Throwback Entertainment** — товарный знак/дизайны; запросить
  compatibility/license statement.
- **FreeDO authors** — письменное разрешение на commercial use или
  перелицензирование в MIT/BSD.
- **ARM** — разрешение на redistribution Norcroft/ARM SDT (маловероятно).
- **3DO rights holders** — лицензия на SDK/BIOS/OS (сложно из-за
  фрагментации прав).
- **trapexit** — ISC-инструменты можно использовать, но он не владеет
  Portfolio OS/SDK; его разрешение не очищает чужие права.

### Вариант F — реальное железо

- Пользовательский 3DO с ODE/модом; BIOS/SDK не в payload.
- Проверка только на законно приобретённых дисках/своих homebrew.
- Без `3dt sign` и `-norsa`.
- В релизных материалах: «emulator validation, not hardware», если железа нет.

### Вариант G — исключить 3DO (выбрано)

Наиболее реалистичный вариант при текущих ограничениях: **3DO исключён из
roadmap**. Поддержка не добавляется, SDK/сборка дисков не обещаются, S53+ не
используются для 3DO. Аудит и reference-материалы сохраняются как справка на случай
пересмотра. Audio/FS (v0.6–0.8) развиваются отдельно, не «вместо» 3DO.

## 9. Решение и условия возможного возврата

**Решение (2026-11-15): 3DO исключён из roadmap; S53+ не используются для 3DO.**
Ниже — не план работ, а фиксация условий, при которых решение может быть
пересмотрено в будущем.

1. **S53+ не использовать для 3DO.** Backend, SDK, сборка дисков и подпись
   не начинаются. Возврат — только через отдельное письменное решение maintainer.
2. **Не вендорить и не включать в CI/release payload:**
   `3do-devkit`, `portfolio_os`, оригинальные SDK images, 3DO libraries/headers/
   docs, Norcroft/ARM SDT, BIOS/ROM (включая `-norsa`), 3DO logo/art,
   `3dt`/RSA keys, `3dokit`/`retro3do-core` без отдельного review.
3. **Границы:** аудит и reference-материалы можно хранить в `docs/` как
   справку; никаких 3DO backend/SDK/эмуляторных профилей, BIOS/ROM, тестовых
   дисков и CI-зависимостей в проекте не появляется. 3DO не входит в default
   build, SDK payload, UI-профили и release-контракт.
4. **Условия возможного возврата к 3DO:**
   - maintainer письменно фиксирует пересмотр решения и возврат 3DO в roadmap;
   - maintainer/юрист письменно подтверждает допустимость non-commercial
     Opera для выбранного режима (dev/CI/commercial);
   - подтверждён статус `opera-boot` GPL-2.0 в CI и отсутствие конфликта с
     FreeDO-условиями;
   - подтверждён provenance `retro3do-core`/`3dokit` или они исключены;
   - оформлена trademark-политика (descriptive use, disclaimer, no logo);
   - оформлена anti-circumvention-политика (no `3dt sign`, no `-norsa`, no
     retail keys);
   - принят clean-room policy и provenance log;
   - CI-проверки: нет BIOS/ROM/SDK/3do-devkit/3DO logo/RSA keys в repo и
     payload; license scan блокирует GPL/LGPL/non-commercial в default build;
     `scripts/verify_release_contract.py`/payload verifier дополнены 3DO-
     правилами;
   - обновлены `LEGAL.md`, `VERSION.txt`, ADR и планы 3DO (при пересмотре).
5. **Если условия не выполнены:** 3DO остаётся исключённым из roadmap;
   audio/FS развиваются отдельно и не начинаются автоматически «вместо 3DO».

## 10. Оценка связки

Шкала: **100 — лучший результат** (минимум юридического риска, простая
интеграция, максимум возможностей), **0 — непригодно**. Для юридического
критерия 100 = минимальный риск.

| Критерий | Оценка | Почему |
|---|---:|---|
| **1. Юридические риски** | **30/100** | Opera/FreeDO non-commercial; `opera-boot` GPL-2.0; SDK/OS/BIOS проприетарные; RSA-ключи; trademark. Для external non-commercial emulator-only режима — выше (~55), для SDK/backend — ниже (~20) |
| **2. Сложность интеграции в движок** | **25/100** | Нет clean SDK/toolchain/OS; `3do-devkit` нельзя; GCC-путь без libs/OS; external emulator adapter прост, но backend — большой clean-room проект |
| **3. Объём возможностей для разработки CD-образов** | **10/100** | Нет легального сборщика дисков: SDK/OS проприетарные; `3dt` умеет sign, но это нельзя; homebrew-диск с Portfolio OS нельзя распространять |
| **4. Глубина поддержки CD** | **55/100** | Opera хорошо запускает CD (cue/bin/CHD, built-in boot); `retro3do-core` CHD; MAME trace; но создание/подпись/retail-проверка — юридически закрыты |
| **5. Активность сообщества** | **55/100** | Opera активна, 3dodev/devkit развиваются, есть homebrew; но ниша узкая, а legal-clean пути почти нет |

**Итог (равные веса): ≈ 35/100.** Это хороший **эмуляторный** компонент
(≈ 65/100 для external non-commercial validation), но очень слабый **SDK/
backend** (≈ 20–25/100). Для сравнения: Jaguar-связка в аудите получила ≈ 40/100
именно потому, что там был хотя бы MIT-совместимый ассемблер/загрузчик; для
3DO даже такого clean-пути нет.

Что подняло бы оценку при пересмотре решения:

- письменное разрешение FreeDO-авторов на commercial use/перелицензирование;
- clean-room OS/runtime и toolchain (AIF/AOF) без SDK-входов;
- легальный способ сборки/подписи дисков (лицензия правообладателя или
  собственная схема, не retail-ключи);
- MIT-clean emulator core с подтверждённым provenance (не `retro3do-core` в
  текущем виде);
- trademark-политика и external emulator adapter без вендоринга.

## 11. Что проверено

| Область | Источник | Результат |
|---|---|---|
| Opera license | `libretro/opera-libretro/LICENSE`, README | FreeDO modified LGPL + non-commercial; LGPL-2.1+ для вклада trapexit; embedded `opera-boot` GPL-2.0 |
| 4DO | `tordona/4DO` LICENSE + `FreeDOCore/FreeDO/*.cpp` headers | top-level GPL-2.0, но core — FreeDO modified LGPL/non-commercial |
| 5DO | `purelogiccode/5DO` LICENSE/README | source-available, FreeDO modified LGPL; game DB licensing unresolved |
| retro3do-core | `CrownParkComputing/retro3do-core` LICENSE/NOTICE/PROVENANCE/THIRD_PARTY | MIT, но non-clean-room; FreeDO «knowledge» clause; MAME BSD-фрагменты; libchdr BSD |
| MAME | `src/mame/misc/3do.cpp` | driver BSD-3-Clause; console drivers `MACHINE_NOT_WORKING`; MAME GPL-2.0 |
| 3dodev.com | `/software/emulation`, `/software/sdks`, `/software/roms`, `/documentation/patents` | Opera active; Phoenix closed; SDK/BIOS/ROM hosted without license; patents list |
| 3do-devkit | GitHub tree/README; `docs/compilers/license.txt`; `takeme/*`; `bin/tools` | no top-level LICENSE; ARM EULA; original SDK libs/headers/docs; OS/boot code/logo; `3dt` signing; BIOS downloader |
| Portfolio OS | `trapexit/portfolio_os` README + `fix_copyright.c` | source published 2022, but header says all rights reserved/confidential |
| 3dokit | `vs-sr-dev/3dokit` README/LICENSE/`sdk.py`/`sdk_tables.py` | MIT; generated from SDK headers/libs; PC-port runtime; not 3DO SDK |
| exp3DOsdk | `gameblabla/exp3DOsdk` README/LICENSES | MIT bridge only; embedded SDK headers/libs/src/ISO excluded |
| 3dt | `trapexit/3dt` LICENSE/docs/signing/`src/tdo_keys.cpp` | ISC code; retail RSA private exponents; `sign` for retail |
| opera-boot | `trapexit/opera-boot` README/LICENSE; `opera_boot.h` | GPL-2.0-only; 128 KiB; no RSA; loads OS from disc; FZ-1 compatibility not established |
| Trademark | Time Extension/GamesRadar/Engadget 2026 | Throwback owns console mark/designs/3do.com; Empire only publishing brand; dispute |
| Copyright/patents | 17 U.S.C. §302(c); 3dodev patents list | copyright persists; identified patents 1993–2003, likely expired; no formal FTO |

## 12. Первоисточники

- Opera LICENSE:
  <https://github.com/libretro/opera-libretro/blob/master/LICENSE>
- Opera README:
  <https://github.com/libretro/opera-libretro#readme>
- Opera embedded boot ROM:
  <https://github.com/libretro/opera-libretro/blob/master/opera_boot.h>
- 4DO LICENSE:
  <https://github.com/tordona/4DO/blob/vs2026/LICENSE>
- 4DO FreeDO core header:
  <https://github.com/tordona/4DO/blob/vs2026/FreeDOCore/FreeDO/_3do_sys.cpp>
- 5DO LICENSE:
  <https://github.com/purelogiccode/5DO/blob/master/LICENSE>
- 5DO README:
  <https://github.com/purelogiccode/5DO/blob/master/README.md>
- retro3do-core:
  <https://github.com/CrownParkComputing/retro3do-core>
- retro3do-core provenance:
  <https://github.com/CrownParkComputing/retro3do-core/blob/cpc/docs/PROVENANCE.md>
- retro3do-core third-party notices:
  <https://github.com/CrownParkComputing/retro3do-core/blob/cpc/docs/THIRD_PARTY.md>
- retro3do-core NOTICE:
  <https://github.com/CrownParkComputing/retro3do-core/blob/cpc/NOTICE>
- MAME 3DO driver:
  <https://github.com/mamedev/mame/blob/master/src/mame/misc/3do.cpp>
- MAME license:
  <https://github.com/mamedev/mame/blob/master/COPYING>
- Phoenix (closed source) / 3DO emulation list:
  <https://3dodev.com/software/emulation>
- 3DO SDK images:
  <https://3dodev.com/software/sdks>
- 3DO ROM/BIOS images:
  <https://3dodev.com/software/roms>
- 3DO patents:
  <https://3dodev.com/documentation/patents>
- 3DO original source code:
  <https://3dodev.com/software/original_source_code>
- 3do-devkit:
  <https://github.com/trapexit/3do-devkit>
- ARM EULA (Norcroft/ARM C++ 1.11):
  <https://github.com/trapexit/3do-devkit/blob/master/docs/compilers/license.txt>
- Portfolio OS source:
  <https://github.com/trapexit/portfolio_os>
- Portfolio OS copyright tool:
  <https://github.com/trapexit/portfolio_os/blob/master/fix_copyright.c>
- 3do-libc / replacement libraries (no LICENSE):
  <https://github.com/trapexit/3do-libc>
- 3dokit:
  <https://github.com/vs-sr-dev/3dokit>
- 3dokit SDK tables:
  <https://github.com/vs-sr-dev/3dokit/blob/main/sdk.py>
- exp3DOsdk:
  <https://github.com/gameblabla/exp3DOsdk>
- exp3DOsdk licenses:
  <https://github.com/gameblabla/exp3DOsdk/blob/main/LICENSES.md>
- 3dt:
  <https://github.com/trapexit/3dt>
- 3dt signing docs:
  <https://github.com/trapexit/3dt/blob/master/docs/signing.md>
- 3dt keys (не копировать значения):
  <https://github.com/trapexit/3dt/blob/master/src/tdo_keys.cpp>
- opera-boot:
  <https://github.com/trapexit/opera-boot>
- geekychris/3do-dev (BIOS download/patched Opera):
  <https://github.com/geekychris/3do-dev>
- 3DO trademark dispute (2026):
  <https://www.timeextension.com/news/2026/07/empire-interactive-withdraws-from-3do-project-due-to-likelihood-of-prolonged-legal-disputes>
  <https://www.gamesradar.com/hardware/retro/33-years-later-the-3do-could-make-an-unexpected-comeback-but-the-90s-console-revival-is-already-being-branded-as-not-legitimate/>
  <https://www.engadget.com/2206578/3do-revival-empire-interactive/>
- GCC Runtime Library Exception:
  <https://www.gnu.org/licenses/gcc-exception-3.1.html>
- GNU GPL FAQ (output of GPL programs):
  <https://www.gnu.org/licenses/gpl-faq.html#WhatCaseIsOutputGPL>
- Sega v. Accolade, 977 F.2d 1510 (9th Cir. 1992); Sony v. Connectix,
  203 F.3d 596 (9th Cir. 2000); Google v. Oracle, 593 U.S. 1 (2021).
- DMCA §1201:
  <https://www.law.cornell.edu/uscode/text/17/1201>
- EU Directive 2001/29/EC, Art. 6:
  <https://eur-lex.europa.eu/eli/dir/2001/29/oj>
- U.S. Copyright term (works made for hire):
  <https://www.copyright.gov/title17/92chap3.html>
