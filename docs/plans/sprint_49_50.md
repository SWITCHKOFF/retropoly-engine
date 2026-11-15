# Reference-план 49–50 — Jaguar: HAL, ROM-ассеты, память и Tom video (reference-недели 97–100)

> **Статус (2026-11-15): REFERENCE ONLY — DEFERRED «до лучших времён» +
> LEGAL HOLD.** План **не активируется** и не является backlog к
> исполнению; Jaguar-код/toolchain/ROM/CD/payload не создаются.
> **Активация — только по явному личному разрешению maintainer начать
> Jaguar** и после приёмки S47–48. Снятие hold само по себе не
> активирует план. До разрешения чек-боксы ниже не выполняются, а
> «Неделя N» — reference-разбиение, не календарь.
> Решение — [`../adr/ADR-016-jaguar-deferred.md`](../adr/ADR-016-jaguar-deferred.md);
> условия возобновления — [`jaguar_legal_hold.md`](jaguar_legal_hold.md).
>
> Reference-план (не активен). Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.4).
> Источник требований: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md)
> §2.4 (Atari Jaguar): 68000 host, Tom (GPU/Object Processor/Blitter),
> Jerry (DSP/I/O), 2 MiB DRAM, картридж до 6 MiB, отсутствие готового 3D API.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> Все hardware-значения ниже — reference material; окончательные цифры
> фиксируются в ADR-016 (deferral), а ADR-017 — только при возобновлении.
> Любая публикация релизов отложена до v0.9.x
> ([`../adr/ADR-013-release-freeze.md`](../adr/ADR-013-release-freeze.md)).

## Активация (обязательный gate)

- [ ] **Maintainer лично и явно разрешил начать Jaguar** (отдельное
      решение, не равное снятию LEGAL HOLD).
- [ ] S47–48 активирован и принят; его критерии выполнены.
- [ ] Reference-даты заменены на реальные сроки/ресурсы; план принят
      maintainer к исполнению.
- [ ] Clean-room и BIOS-политика остаются в силе.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 49–50 |
| Недели | **не назначены**; reference: 97–100 (28.06–25.07.2027) — не commitment |
| Веха | v0.4.0 — **reference**, не текущая веха |
| Цель | Reference-цель (при активации): поднять HAL на 68000, read-only ROM-ассеты, memory ledger 2 MiB и видеовыход через Object Processor/Blitter с GPU-кодом в 4 KiB local RAM |
| Входные критерии | **Личное разрешение maintainer начать Jaguar**; S47–48 принят; ADR-017 (при возобновлении); pinned MIT-профиль toolchain; bootable ROM, `jaguar-release`, `build-jaguar` |
| На критическом пути | **Нет.** Вне текущего плана; после активации — да (HAL и video — фундамент renderer'а) |
| Выходные артефакты | `src/hal/jaguar/` (lifecycle/input/time/FS/memory/video), ROM asset pack, `scripts/verify_jaguar_budgets.py`, boot/asset self-test, `docs/JAGUAR.md` (memory/video/FS), ADR-017 |
| Следующий спринт | **Не активен.** [`sprint_51_52.md`](sprint_51_52.md) — reference: renderer, W0/W1, SDK/UI и draft v0.4.0 |

## PERT-оценка (reference, не commitment)

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 38 | 60 | 92 | 61.7 | 9.0 | 79.7 | 40–60 ч | ⚠ Видео/Blitter и 2 MiB ledger — главный риск; CRY-текстуры и DSP-помощь режем первыми |

## Задачи (reference; не выполнять до активации)

**Reference-этап 1 (неделя 97) — HAL lifecycle, ввод и время:**

- [ ] [B] 68000 startup: vector table, stack, data/bss init, Tom/Jerry init, interrupt mask; идемпотентные `Platform_Init`/`Shutdown` + rollback при ошибке
- [ ] [B] VBlank-прерывание от Tom; счётчик кадров и flip; монотонное время через таймер Jerry; без дрейфа на 60 Гц display
- [ ] [B] Ввод: два порта через Jerry; D-pad, A/B/C, Option/Pause, keypad; deadzone; ровно один poll на кадр; стабильный API `Platform_Input*`
- [ ] [H] Debug overlay: FPS/frame time, peak DRAM, GPU/DSP load, число объектов в object list, Z-статистика
- [ ] [H] Логирование: экран + host-тестируемый backend; без emulator-only каналов
- [ ] [M] Проверить поведение при отсутствии второго контроллера/нажатых кнопках

**Reference-этап 2 (неделя 98) — картридж, ROM-ассеты и память:**

- [ ] [B] ROM layout: 8 KiB security header, 68000-код, read-only asset pack с 16-byte alignment и directory (offset/size/crc32); детерминированный packer
- [ ] [B] Безопасная загрузка `.mesh`/`.tex` v1 из ROM: bounds/version/CRC, arena rollback при ошибке; полный набор malformed-кейсов
- [ ] [B] `asset_converter --platform jaguar`: детерминированная конвертация 64×64 в platform-native RGB16 (CRY — опция, решение ADR-017); `.mesh`/`.tex` v1 не меняются
- [ ] [B] Memory ledger 2 MiB: code/data/object lists ≤ 320 KiB, stack 64 KiB, framebuffers 300 KiB, Z 150 KiB, arenas 704 KiB (asset 448, frame 128, command 64, FS staging 64), reserve 448 KiB, headroom ≥ 62 KiB; GPU/DSP local 4/8 KiB — вне DRAM
- [ ] [B] Запрет heap в engine-коде (static check); SDK/libc-аллокации минимизированы и задокументированы
- [ ] [B] `scripts/verify_jaguar_budgets.py`: DRAM ledger, ROM/asset pack ≤ 4 MiB, stack, Z/framebuffer, no-heap; CI-гейт
- [ ] [H] Asset pack ≤ 4 MiB; проверить загрузку `CUBE.MSH`/`TEX0.TEX` из ROM на реальном размере

**Reference-этап 3 (неделя 99) — Tom video, GPU и Blitter:**

- [ ] [B] Object Processor: object list из двух branch-объектов + bitmap, 320×240 RGB16, double buffer, VBlank flip без tearing
- [ ] [B] GPU-код в 4 KiB local RAM: загрузка/запуск/остановка через `G_CTRL`, overlay-схема, long-only доступ 68000; никаких `move.w`/`move.b` к GPU/DSP
- [ ] [B] GPU-ядро: clear, обновление object list, базовый span-fill через Blitter; bounded command ring, overflow → `false` + диагностика
- [ ] [B] Blitter: phrase mode, 16-bit Z compare/write, Gouraud; регистровая карта и host-тесты кодирования
- [ ] [H] Jerry DSP: таймеры и ввод, 8 KiB local; звук не реализуется, DSP простаивает или используется для вспомогательной математики
- [ ] [H] ADR-017: Tom video/render foundation — RGB16 vs CRY, Z-формат, Blitter-режимы, object list, границы texture mapping
- [ ] [M] Проверить PAL/NTSC-режимы; v0.4.0 gate — NTSC 320×240, PAL — задокументированное ограничение

**Reference-этап 4 (неделя 100) — интеграция и приёмка:**

- [ ] [B] Render skeleton `BeginFrame`/`Clear`/`EndFrame` через Tom; command buffer 64 KiB, overflow → явная ошибка
- [ ] [B] Boot/asset self-test: инициализация, загрузка `CUBE.MSH`/`TEX0.TEX` из ROM, проверка CRC, отрисовка тестового кадра/clear, 60 с без краша
- [ ] [B] 10-минутный stress; MemoryProfiler; без утечек и деградации
- [ ] [B] Host-тесты: memory ledger, FS/directory, input mapping, object list, Blitter/Z-encoding; ASan для host-части
- [ ] [B] `docs/JAGUAR.md`: memory map, video/object list, ROM FS, input, ограничения
- [ ] [H] Скриншот `screenshots/jaguar_boot_s49_50.png`; benchmark-метаданные окружения (G12)
- [ ] [M] CI artifacts: `.j64`, map, sha256; negative-checks для бюджета и ROM layout

## Критерии приёмки спринта 49–50 (если активирован)

- [ ] `jaguar-release` собирается с pinned MIT-профилем; ROM проходит header/checksum/entry; boot/asset self-test 60 с PASS
- [ ] `Platform_Init`/`Shutdown` идемпотентны; VBlank/время/ввод работают без дрейфа; ровно один poll ввода на кадр
- [ ] ROM-ассеты загружаются безопасно; malformed-файлы отвергаются, арена откатывается
- [ ] Object Processor выдаёт 320×240 RGB16 double buffer без tearing; GPU-код ≤ 4 KiB local; Blitter Z/Gouraud кодируется корректно
- [ ] Memory ledger 2 MiB соблюдён, heap в engine-коде отсутствует; ROM/asset pack ≤ 4 MiB
- [ ] 10-минутный stress PASS; host-тесты и ASan зелёные; `docs/JAGUAR.md`/ADR-017 согласованы

## Ручной чек-лист приёмки (если активирован)

- [ ] Запустить boot/asset self-test в pinned эмуляторе: изображение, heartbeat, 60 с без краша
- [ ] Проверить ввод с двух портов (D-pad, A/B/C, Option/Pause, keypad), отсутствие «залипания»
- [ ] Проверить overlay: FPS/frame time, DRAM peak, GPU/DSP load, object list, Z
- [ ] Загрузить битый `.mesh`/`.tex` — приложение не падает, арена не течёт
- [ ] Снять скриншот, проверить разрешение/цвета/отсутствие tearing
- [ ] 10-минутный stress и повторный init/shutdown без деградации

## Риски

- **68000 bus starvation и long-only доступ к Tom/Jerry.** Митигация: 68000 — только host/control, тяжёлая работа на GPU/Blitter; long-only, документированные правила доступа.
- **GPU local 4 KiB мал для кода.** Митигация: компактные ассемблерные ядра, overlay-чunks, загрузка по кадрам; никакого большого C-кода на GPU.
- **Blitter плохо документирован и имеет hardware bugs.** Митигация: документированные режимы, Hardware Bugs & Warnings, app-side self-test, libretro Virtual Jaguar (GPL-3.0) как внешний процесс; Virtual Jaguar Rx — fallback; BigPEmu — optional local; MAME — только трассировка; gate на реальном железе — после появления.
- **2 MiB DRAM не хватает.** Митигация: фиксированный ledger, no-heap, 320×240 RGB16, маленькие текстуры, scope reduction CRY/DSP.
- **Эмуляторная точность Tom/Jerry.** Митигация: pinned baseline, app-side счётчики/hash, ручной gate; эмулятор не доказательство реального тайминга.
- **ROM asset pack ломает boot.** Митигация: packer + CRC/offset-проверки в CI, clean-room прогон, лимит 4 MiB.
