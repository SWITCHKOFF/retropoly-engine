# Reference-план 47–48 — Jaguar: исследование, toolchain, backend skeleton и CI (reference-недели 93–96)

> **Статус (2026-11-15): REFERENCE ONLY — DEFERRED «до лучших времён» +
> LEGAL HOLD.** План **не активируется** и не является backlog к
> исполнению; Jaguar-код/toolchain/ROM/CD/payload не создаются.
> Решение — [`../adr/ADR-016-jaguar-deferred.md`](../adr/ADR-016-jaguar-deferred.md);
> условия возобновления — [`jaguar_legal_hold.md`](jaguar_legal_hold.md).
>
> **Активация — только по явному личному разрешению maintainer начать
> Jaguar.** Снятие LEGAL HOLD, закрытие лицензионных вопросов или наличие
> свободного времени сами по себе не активируют план. До разрешения
> чек-боксы ниже не выполняются, а «Неделя N» — reference-разбиение, не
> календарь.
>
> Reference-план (не активен). Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.4).
> Источник требований: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md)
> §2.4 (Atari Jaguar) и §4 «Будущие итерации» — Jaguar v0.4.0, S47+.
> Legacy SDK — **jaguar-sdk** (инженерная документация называет его
> open-source public-domain lineage; аудит это не подтвердил: top-level
> LICENSE отсутствует). Для возможного возобновления baseline — **MIT-профиль**:
> `m68k-elf-gcc`/binutils + `lyxass` + `new_bjl` (+ `jaglib` после
> замены Atari `jagcore.h`); `jaguar-sdk` — только optional env-install.
> Commit/лицензионный статус и связка зафиксированы в ADR-016;
> `VERSION.txt`/`LEGAL.md` обновляются только при возобновлении.
> Любая публикация релизов проекта отложена до v0.9.x
> ([`../adr/ADR-013-release-freeze.md`](../adr/ADR-013-release-freeze.md)).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> JTRM rev 8 подтверждает ключевые hardware-факты; `jaguar-sdk`/rmac/rln
> не имеют явной лицензии; MAME отклонён как acceptance-baseline.
> Полный аудит — [`jaguar_legal_audit_v2.md`](jaguar_legal_audit_v2.md).
> S45–46 (N64) закрыт локально; v0.3.0 остаётся draft.

## Активация (обязательный gate)

Пока этот gate не закрыт, план — **только reference**; задачи и
чек-боксы ниже не выполняются.

- [ ] **Maintainer лично и явно разрешил начать Jaguar** (дата, объём,
      сроки). Это отдельное решение, не равное снятию LEGAL HOLD.
- [ ] Выбран режим эмулятора: external VJ и/или HLE-only форк; BIOS-
      политика зафиксирована.
- [ ] Юридическая проверка/письменное принятие остаточного риска.
- [ ] Clean-room policy для CD/регистров подписана.
- [ ] Лицензионные уточнения (`cdi-maker`, `rmac`/`rln`, `jaglib`,
      `rmvlib`) закрыты или явно исключены из scope.
- [ ] Reference-даты заменены на реальные сроки/ресурсы; план принят
      maintainer к исполнению.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 47–48 |
| Недели | **не назначены**; reference: 93–96 (31.05–27.06.2027) — не commitment |
| Веха | v0.4.0 — **reference**, не текущая веха |
| Цель | Reference-цель (при активации): зафиксировать Jaguar-контракты и лицензии, поднять pinned toolchain, собрать bootable ROM и CI-контур без эмулятора |
| Входные критерии | **Личное разрешение maintainer начать Jaguar**; S45–46 закрыт; release freeze действует; DEFERRED и legal hold сняты; reference-факты Jaguar перепроверены; лицензионный статус jaguar-sdk/rmac/rln зафиксирован |
| На критическом пути | **Нет.** Вне текущего плана; после активации — да (первый Jaguar-спринт) |
| Выходные артефакты | ADR-017 (при возобновлении; ADR-016 уже accepted), `docs/JAGUAR.md` (draft), `jaguar-release`, `src/hal/jaguar/` skeleton, boot-ROM `.j64`/`.cof` + map, `build-jaguar.yml`, Jaguar-секция `ci/normative_contracts.yaml`, `scripts/verify_jaguar_rom.py` |
| Следующий спринт | **Не активен.** [`sprint_49_50.md`](sprint_49_50.md) — reference: HAL Jaguar (68000, Jerry I/O, ROM-ассеты, память, Tom video) |

## PERT-оценка (reference, не commitment)

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 55 | 85 | 56.7 | 8.3 | 73.3 | 40–60 ч | ⚠ Toolchain/лицензии — главный риск; при блокере режем boot-demo до clear-screen, Tom/Blitter не трогаем |

## Предварительный аудит (2026-11-15)

| Область | Источник | Вывод |
|---|---|---|
| Hardware | Jaguar Technical Reference Manual rev 8 (Tom & Jerry) | Подтверждены: 68000 — host; Tom = Object Processor + GPU + Blitter; Jerry = DSP + таймеры/I/O; GPU local 4 KiB; DSP local 8 KiB; 2 MiB DRAM; картридж до 6 MiB; Blitter Z/Gouraud только в 16-bit pixel mode; OP поддерживает 1/2/4/8/16/24 bpp. Тактовые частоты 13.295/26.59 МГц в просмотренном фрагменте JTRM не зафиксированы — сверить отдельно по hardware reference |
| jaguar-sdk | github.com/cubanismo/jaguar-sdk | top-level LICENSE отсутствует; README подтверждает Atari samples, сканы документации и DOS-бинарники; rmac/rln содержат только copyright, без явной лицензии. Вывод: **не вендорить**; использовать env-install; SDK payload не содержит jaguar-sdk, тулчейн и Atari docs; распространяются только собственные исходники/библиотеки |
| MAME | mamedev/mame, `src/mame/atari/jaguar.cpp` | Драйвер `jaguar` помечен `MACHINE_NOT_WORKING \| MACHINE_IMPERFECT_GRAPHICS \| MACHINE_IMPERFECT_SOUND`; TODO: proper blitter phrase mode, OP timer, uneven pixel width. **Не baseline** для visual/Z/Gouraud-гейта; допустим только как трассировщик/отладка |
| BigPEmu | richwhitehouse.com/jaguar, v1.221 Linux x64 | Deterministic (по заявлению автора), CLI, Linux x64; closed-source, free-of-charge, без явной лицензии на redistribution. Технический кандидат ручного gate: pin 1.221 + FNV `C1B241BBFA5135CB`; **юридический риск — высокий**, не в CI и не в payload |
| Virtual Jaguar | `djipi/Virtual-Jaguar-Rx` (GPL-3.0) | GPL-3.0 fallback; старый, Linux-сборку/точность проверить |
| libretro Virtual Jaguar | `libretro/virtualjaguar-libretro` (GPL-3.0) | Активный Jaguar/CD-эмулятор; HLE по умолчанию, но встроенные Atari BIOS — отдельный риск. Только внешний процесс, не линковать |

Полный аудит, матрица MIT-совместимости и варианты интеграции —
[`jaguar_legal_audit_v2.md`](jaguar_legal_audit_v2.md). Краткая политика
hold — [`jaguar_legal_hold.md`](jaguar_legal_hold.md).

## Задачи (reference; не выполнять до активации)

**Reference-этап 1 (неделя 93) — исследование и нормативные решения:**

- [ ] [B] Перепроверить Jaguar-reference-факты по первоисточникам (Jaguar Technical Reference Manual, Tom & Jerry Software Reference, Atari Hardware Bugs & Warnings): 68000 13.295 МГц; Tom GPU 26.59 МГц + Object Processor + Blitter; Jerry DSP 26.59 МГц + таймеры/ввод; 2 MiB DRAM; GPU local 4 KiB; DSP local 8 KiB; картридж до 6 MiB ($800000–$DFFFFF, entry $802000 после 8 KiB security header); 320×240 RGB16/CRY; 16-bit Z/Gouraud blitter. Записать в `docs/JAGUAR.md` с метками **HW** / **policy**
- [ ] [B] ADR-016: Jaguar backend — compile-time `PLATFORM_JAGUAR`, **MIT-профиль** toolchain (`m68k-elf-gcc`/binutils + `lyxass` + `new_bjl`), legacy `jaguar-sdk` — только env-install пользователем; формат ROM `.j64` (raw) и `.cof` (dev), emulator policy (внешний процесс), границы при возобновлении: **CD-first** (HLE/пользовательский BIOS), без Atari BIOS в payload; cartridge — только smoke; audio/Jerry-DSP-звук — вне v0.4.0
- [ ] [B] Лицензионный аудит: подтверждено — top-level LICENSE у `jaguar-sdk` отсутствует, rmac/rln имеют только copyright без явной лицензии, в SDK есть Atari samples/сканы/DOS-бинарники и CD BIOS v4.05. Решение: **не вендорить**; `jaguar-sdk`/Atari libs/BIOS — только env-install; MIT-профиль — `lyxass` (MIT), `new_bjl` (Unlicense), `jaglib` (MIT после замены `jagcore.h`), GCC/binutils; `VERSION.txt`, `LEGAL.md`, `THIRD_PARTY.md`
- [ ] [B] Spike эмулятора: libretro Virtual Jaguar (GPL-3.0) — лучший открытый CD-кандидат, но только внешний процесс; Virtual Jaguar Rx — fallback; MAME — только трассировка (драйвер `jaguar` = `MACHINE_NOT_WORKING|IMPERFECT_GRAPHICS`); BigPEmu 1.221 — технически лучший, но high legal risk, только опциональный локальный gate. Зафиксировано в ADR-016 (deferral)
- [ ] [B] ROM-контур: 68000 vector table, 8 KiB security header/checksum, entry `$802000`, layout `.j64`/`.cof`, map-файл; минимальный ROM (clear screen) на собственном startup по JTRM, без копирования Atari-кода
- [ ] [H] Сверить Jaguar HAL с PC/PS1/Saturn/N64: какие интерфейсы переиспользуются, что специфично (Tom OP/Blitter, GPU/DSP local RAM, Jerry I/O, 68000 bus rules)
- [ ] [M] Предварительный memory map DRAM: код/данные, stack, video/Z, engine arenas, reserve; окончательные цифры — S49–50

**Reference-этап 2 (неделя 94) — toolchain, CMake и CI:**

- [ ] [B] `cmake/jaguar-toolchain.cmake` + preset `jaguar-release`; `PLATFORM_JAGUAR` в guard «ровно один backend»; `RPE_PLATFORM_NAME=jaguar`, arch `m68k` (добавить в mapping); imported targets toolchain
- [ ] [B] Toolchain discovery: `m68k-elf-gcc`/binutils + `lyxass`; pinned SHA-256/версии проверяются в CI. `JAGUAR_SDK_ROOT` — только optional legacy-профиль, не обязателен и не вендорится
- [ ] [B] `src/hal/jaguar/` skeleton: идемпотентные `Platform_Init`/`Shutdown`, `Platform_PumpEvents`, `Platform_GetTimeMs`; stub `hal_render_backend_*` возвращает `false` + диагностику (без silent drop)
- [ ] [B] Линковка 68000-цели (GCC/binutils) + GPU/DSP (`lyxass`); сборка `.j64`/`.cof`, map-файл, sha256 артефакта
- [ ] [B] `.github/workflows/build-jaguar.yml`: pinned toolchain, configure/build `jaguar-release`, ROM verify, upload `.j64` + map + sha256; эмулятор в CI **не запускается**
- [ ] [B] `ci/normative_contracts.yaml`: секция `platforms.jaguar` (toolchain pin, ROM extension, budget placeholders, `emulator_validation: manual`)
- [ ] [H] `docs/BUILDING.md` § Jaguar: MIT-профиль (`m68k-elf-gcc`, `lyxass`, `new_bjl`), optional legacy `jaguar-sdk` env-install; env vars, сборка/запуск
- [ ] [H] `README.md`: supported matrix — Jaguar (development), ссылка на `docs/JAGUAR.md`

**Reference-этап 3 (неделя 95) — boot, ROM и защитные проверки:**

- [ ] [B] Boot path: `Platform_Init` → Tom video init 320×240 RGB16 → object list → clear через GPU/Blitter → main loop → `Platform_Shutdown`; повторный init/shutdown не ломает процесс
- [ ] [B] Boot self-test: версия/target/time/heartbeat; лог через экран или документированный debug-канал, без зависимости от emulator-only фич
- [ ] [B] CI negative-checks: неверный toolchain, отсутствует `m68k-elf-gcc`/`lyxass`, нет `.j64` на выходе, включено больше одного `PLATFORM_*`, отсутствует entry `$802000`/неверный checksum
- [ ] [B] `scripts/verify_jaguar_rom.py`: header/magic, checksum, size, entry `$802000`, sha256; PASS/FAIL с диагностикой
- [ ] [H] `scripts/run_jaguar.sh`/`.bat`: запуск pinned эмулятора с документированными настройками; BIOS/ROM не вендорятся
- [ ] [M] Минимальный `hello_jaguar` (clear + heartbeat), без треугольника — доказательство boot-контура

**Reference-этап 4 (неделя 96) — документы и приёмка:**

- [ ] [B] `docs/JAGUAR.md` (draft): reference-факты, memory map, build/run, ограничения, план gate
- [ ] [B] ADR-017 accepted (при возобновлении); `VERSION.txt`/`LEGAL.md` согласованы с пинами
- [ ] [B] Приёмка: `jaguar-release` собирается с чистого checkout; `.j64` проходит header/checksum; boot-demo доходит до main loop в pinned эмуляторе 60 с
- [ ] [B] Отсутствие чужих BIOS/ROM, ключей и build-каталогов в репозитории и артефактах
- [ ] [M] Обновить roadmap/статус S47–48 в планах

## Критерии приёмки спринта 47–48 (если активирован)

- [ ] `cmake --preset jaguar-release` собирается с pinned MIT-профилем (`m68k-elf-gcc`/binutils + `lyxass`); guard «ровно один backend» включает Jaguar
- [ ] CI `build-jaguar` работает: configure/build, ROM verify, artifacts; эмулятор в CI не участвует
- [ ] ROM-заголовок/checksum/entry валидны; boot-demo стабильно работает 60 с в pinned эмуляторе
- [ ] `Platform_Init`/`Shutdown` идемпотентны; stub renderer явно возвращает ошибку, без silent drop
- [ ] Лицензионный аудит завершён; ADR-017, `docs/JAGUAR.md`, `VERSION.txt`, `LEGAL.md`, `ci/normative_contracts.yaml` согласованы
- [ ] В репозитории/артефактах нет чужих BIOS, ROM-образов игр, ключей и build-каталогов

## Ручной чек-лист приёмки (если активирован)

- [ ] Установить MIT-профиль toolchain по `docs/BUILDING.md` на чистую машину и собрать preset; optional legacy `jaguar-sdk` — отдельно
- [ ] Запустить `.j64` в pinned эмуляторе: clear screen, heartbeat, 60 с без краша
- [ ] Проверить ROM header/checksum/entry и размер; сравнить с CI-артефактом
- [ ] Проверить пины `VERSION.txt`/SHA-256, лицензии и отсутствие чужих ROM/BIOS в git
- [ ] Переключиться между PC/PS1/Saturn/N64/Jaguar пресетами — остальные сборки не сломаны

## Риски

- **jaguar-sdk старый, нестандартный и без единой лицензии.** Митигация: pinned commit, Docker/env-install, per-component лицензионный аудит, rmac/rln из исходников; **не вендорить** SDK/тулчейн/Atari docs; отказ от проприетарных бинарников.
- **Toolchain долго ставится и дрейфует.** Митигация: immutable commit + SHA-256, CI-кэш, документированный env-install; обновление — отдельная задача с ADR.
- **Эмуляторы Jaguar различаются по точности и юридическому риску.** Митигация: libretro Virtual Jaguar (GPL-3.0) — внешний baseline-кандидат; Virtual Jaguar Rx — fallback; BigPEmu 1.221 — только опциональный локальный gate, не в CI/payload; MAME — трассировка (NOT_WORKING/IMPERFECT_GRAPHICS); CI не зависит от эмулятора.
- **68000/GPU/DSP bus rules и hardware bugs.** Митигация: long-only доступ 68000 к GPU/DSP, запрет остановки RISC извне, документированные workaround'ы; проверка на эмуляторе и позже на железе.
- **ROM header/boot ломается на чистом checkout.** Митигация: `verify_jaguar_rom.py`, checksum/entry-проверки в CI, документированный layout.
- **Scope creep (Jaguar CD, audio, 3DO).** Митигация: границы ADR-016; при возобновлении CD — основной target, audio — вне v0.4.0; 3DO исключён из roadmap (v0.5.0 не планируется).
