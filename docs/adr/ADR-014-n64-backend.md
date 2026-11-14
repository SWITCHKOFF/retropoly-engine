# ADR-014: Nintendo 64 backend — toolchain, libdragon, ROM и эмулятор

- **Статус:** Accepted (revision 2 — S41–42)
- **Дата:** 2026-11-14
- **Спринт:** 39–40 (недели 77–80), 41–42 (недели 81–84), веха v0.3.0
- **Связанные документы:** [`../plans/sprint_39_40.md`](../plans/sprint_39_40.md),
  [`../N64.md`](../N64.md), [`../BUILDING.md`](../BUILDING.md),
  [`../../VERSION.txt`](../../VERSION.txt), ADR-001 (compile-time HAL),
  ADR-007 (память), ADR-011 (SDK), ADR-013 (release freeze).

## Контекст

Nintendo 64 — следующая платформа после PC/PS1/Saturn. До начала
реализации нужно зафиксировать решения, которые дорого менять позже:
toolchain, версию libdragon, формат ROM, эмулятор для ручного gate,
debug-канал, memory map и границы v0.3.0. Reference-факты перепроверены
по первоисточникам (Nintendo 64 Programming Manual, VR4300 datasheet,
libdragon docs); hardware-факты и engine policy разделены в
[`../N64.md`](../N64.md).

## Решения

### 1. Compile-time HAL, без runtime dispatch

N64 — четвёртый compile-time backend: CMake-опция `PLATFORM_N64`, ровно
один из `PLATFORM_PC` / `PLATFORM_PS1` / `PLATFORM_SATURN` /
`PLATFORM_N64`. Публичный
[`../../include/retro_engine/hal/platform.h`](../../include/retro_engine/hal/platform.h)
не включает libdragon; libdragon-заголовки живут только в
`src/hal/n64/*.c/.cpp`. `src/CMakeLists.txt` выбирает `hal/n64/` на
этапе конфигурации (ADR-001).

**Следствие:** N64-код не протекает в общий слой; PC/PS1/Saturn сборки
не меняются.

### 2. Toolchain: mips64-elf-gcc 16.2.0 + libdragon (pinned commit)

Выбран официальный **libdragon-toolchain** (GCC 16.2.0 для
`mips64-elf`, Linux x86_64 deb) и **libdragon** (MIT) с immutable commit
`e356bf3f56f7afbf7e5246329562f145965cfdfc`.

| Компонент | Пин | Проверка |
|---|---|---|
| GCC 16.2.0 mips64-elf deb | SHA-256 `fac8e657…0a0a4d` | [`../../ci/n64-toolchain.sha256`](../../ci/n64-toolchain.sha256) |
| libdragon | commit `e356bf3f56f7afbf7e5246329562f145965cfdfc` | [`../../ci/n64-libdragon.commit`](../../ci/n64-libdragon.commit) + `git rev-parse HEAD` |
| libdragon license | MIT | `LEGAL.md` |

libdragon **не вендорится** в git: `N64_INST` указывает на
install-prefix (toolchain + `libdragon.a` + `n64tool`), `LIBDRAGON_ROOT`
— на git-checkout для проверки commit. CI скачивает deb, проверяет
SHA-256, клонирует libdragon на pinned commit, собирает
`make && make install && make tools-install`. Обновление toolchain или
libdragon — отдельная задача с ADR.

**Альтернативы:** Docker-образ libdragon отклонён для v0.3.0 (тяжёлый
CI-контур, скрытые слои); вендоринг libdragon submodule отклонён
(дублирование SDK и рост checkout). Env-install выбран как у PS1/Saturn.

### 3. ROM: `.z64`, IPL3 libdragon, header CRC не используется

Формат — big-endian `.z64`; `n64tool` (pinned libdragon) собирает
header + IPL3 + stripped/compressed ELF + TOC. libdragon IPL3 не читает
64-байтный header и не требует checksum (`boot/README.md`), поэтому
CRC1/CRC2 в header остаются нулевыми; `scripts/verify_n64_rom.py`
проверяет структурные поля (magic, entry `0x80000400`, title, category,
country, ELF32 BE MIPS/PT_LOAD) и принимает нулевой CRC как
`libdragon_ipl3_header_crc_unused`. Если CRC ненулевые — сверяется
стандартный N64 boot checksum; неверные CRC = FAIL.

ROM title — `RETRO POLYENGINE`, category `N`, region `E`. Артефакты:
`hello_n64.z64`, `hello_n64.map`, `hello_n64.z64.sha256`,
`retro_n64.elf`.

**Следствие:** «ROM header/checksum valid» — это контракт libdragon
IPL3, а не официального IPL3 Nintendo; отклонение явно задокументировано
и проверяется CI.

### 4. Эмулятор: gopher64 1.2.1, только ручной gate

Pinned baseline — **gopher64 1.2.1** (Linux x86_64, SHA-256 в
[`../../ci/n64-gopher64.sha256`](../../ci/n64-gopher64.sha256)).
Причины:

- **mupen64plus отклонён:** не загружает ROM с libdragon IPL3 r5+;
  dynarec падает `Compile at bogus memory address`, pure interpreter не
  доходит до user code (mupen64plus-core#1185, открыт на 2026-11-14).
  Наш ROM использует IPL3 r8/r10-эру — воспроизведено локально.
- **ares** загружает libdragon-rom, но upstream не публикует Linux-бинарь;
  сборка из исходников не входит в gate S39–40.
- **gopher64** загружает ROM, печатает ISViewer-лог в stdout, работает
  под Xvfb с программным Vulkan/llvmpipe; 900+ кадров за 15 с локально.

CI эмулятор **не запускает**: BIOS/ROM-образы не вендорятся; ручной gate
— 60 с, `scripts/run_n64.sh` (Linux) / `.bat` (Windows), скриншот
320×240 и heartbeat `frame=… time=…`. Эмулятор и ROM не входят в
репозиторий/артефакты.

### 5. Debug-канал: ISViewer + USB

Boot-лог и `Log_Message` идут через libdragon `debugf`:
`debug_init_emulog()` (ISViewer; gopher64 печатает строки в stdout) и
`debug_init_usblog()` (USB на железе). N64 Release-профиль собирается
**без `-DNDEBUG`**, чтобы `debugf` был активен в bring-up; это
осознанное решение S39–40, а не релизный профиль v0.3.0.

**Следствие:** CI видит boot self-test в логе без эмулятора? Нет — CI
только собирает ROM; строки `boot:` видны при ручном запуске в
gopher64.

### 6. Memory map (предварительный, уточняется S41–42)

| Регион | Размер/адрес | Использование v0.3.0 |
|---|---|---|
| RDRAM base | 4 MiB (`0x80000000`) | обязательный минимум |
| RDRAM + Expansion Pak | 8 MiB | reference gate (gopher64: Expansion Pak ON) |
| Код/данные/стек | `n64.ld` layout | ELF + libdragon + engine |
| Framebuffers | 2×320×240×2 B = 300 KiB | VI 16-bit double buffer |
| RDP TMEM | 4 KiB | текстуры/тайлы RDP (S43–44) |
| RDP Z | 18-bit internal | аппаратный Z (S43–44), не RDRAM |
| ROM/DFS | cartridge 4–64 MiB | read-only DFS, S41–42 |

Точные бюджеты (engine arenas, SDK reserve, stack) зафиксированы в
S41–42 (revision 2, §9) и в `ci/normative_contracts.yaml`
(§ `platforms.n64.memory`).

### 7. Границы v0.3.0 и S39–40

- **S39–40:** research, pinned toolchain/libdragon, `n64-release`, HAL
  skeleton, bootable `.z64` (VI 320×240 + RDP clear), CI без эмулятора,
  ADR/docs/pins.
- **S41–42:** VI/input/ROM-DFS/память, boot self-test с реальными
  ассетами.
- **S43–44:** RSP-микрокод, RDP, аппаратный Z, TMEM/текстуры.
- **S45–46:** hello_triangle/rotating_cube, W0/W1-бюджеты, N64 SDK/UI,
  draft v0.3.0.

Вне scope v0.3.0: audio, расширенная ФС/запись, PAL/480i как target,
64DD, Expansion Pak-зависимые игры, публикация релиза (release freeze
до v0.9.x, ADR-013).

### 8. Renderer S39–40: явная заглушка

`src/hal/n64/render_n64.cpp` возвращает `false` из всех bool-методов
backend'а; ядро рендера фиксирует диагностический код
(`RENDER_DIAG_NO_WINDOW`/`BACKEND_FAIL`), silent drop запрещён. Boot-demo
S39–40 очищает экран напрямую через RDPQ и рисует debug-текст, не вызывая
`Render_Init`.

### 9. Revision 2 — S41–42: VI/input/DFS/memory

S41–42 уточняет пункты 6–8 без изменения toolchain/libdragon/эмулятора:

**VI/framebuffer.** 320×240, 16-bit, NTSC 60 Hz, 2 buffers (300 KiB).
`Platform_Init`/`Shutdown` идемпотентны; повторный init после shutdown
переинициализирует joypad/DFS/шрифт. `Platform_GetTimeMs` —
`get_ticks_ms()` uint32-срез (wrap-safe, 10-минутный прогон).

**Input.** `joypad_poll()` ровно один раз за кадр в
`Platform_PumpEvents`; маппинг A/B/Z/C/D-pad/L/R/Start → `INPUT_PAD_*`
зафиксирован в `docs/N64.md` §7 и host-тестах; deadzone 8; Y-оси
инвертируются в SDL-конвенцию.

**Memory budget (4 MiB base).** Engine arenas — 2 MiB: asset 1.5 MiB,
frame 256 KiB, command 128 KiB, FS staging 128 KiB; stack 64 KiB;
framebuffers 300 KiB; code/data/SDK reserve 1 724 416 B. Ledger
проверяется `static_assert` + runtime `memory_ledger_check()`. Heap в
N64 HAL запрещён (`scripts/verify_n64_no_heap.py`); libdragon boot-heap
входит в SDK reserve. Expansion Pak детектируется, но арены не растут.

**Filesystem.** Read-only DragonFS (`mkdfs`), образ вставляется
`n64tool --align 16`; bare UPPERCASE-имена `CUBE.MSH`, `TEX0.TEX`,
`TRIANGLE.MSH`; максимум 128 KiB на файл; staging 128 KiB; точное
чтение, ошибки явные. `mkdfs` детерминирован (проверка двух прогонов в
сборке). Загрузка `.mesh`/`.tex` v1 — через существующие safe-загрузчики
ядра с rollback asset arena.

**Renderer.** S41–42 — skeleton: `Render_Init/320×240`,
`BeginFrame/Clear/EndFrame` через RDPQ; `draw_triangle`/texture/overlay/
readback возвращают `false` + диагностику. Bounded RDP command arena
128 KiB; overflow — явный `false`, без silent drop. RSP/RDP-геометрия,
Z/TMEM/текстуры — S43–44.

**Debug-канал.** ISViewer/USB + rate limit 120 строк/с; при
переполнении окна печатается счётчик dropped (диагностика не теряется
молча).

**Приёмка S41–42 (2026-11-14):** `n64_hal_tests` 46/46, ASan — чисто;
`n64-release` + ROM verifier PASS; DFS-ассеты извлечены из ROM и
побайтово совпали с `assets/build`; gopher64 1.2.1 — 60-с gate
(3600 кадров), 10-минутный stress (602 391 мс / 36 000 кадров) и
input-лог PASS; скриншот `screenshots/n64_boot_s41_42.png`.

## Последствия

- `n64-release` требует `N64_INST` (+ `LIBDRAGON_ROOT` для проверки
  commit); clean checkout + documented fetch — часть gate.
- CI `build-n64` собирает и структурно валидирует ROM; эмулятор,
  BIOS и ROM-образы в CI/артефактах отсутствуют.
- Любое изменение toolchain/libdragon/ROM-формата/эмулятора — новый ADR
  (или revision этого ADR) с обновлением `docs/N64.md` и `VERSION.txt`.

## Риски и митигации

| Риск | Митигация |
|---|---|
| libdragon/toolchain дрейфует | immutable commit + SHA-256 deb; CI cache; обновление только с ADR |
| Эмуляторы расходятся | pinned gopher64; app-side heartbeat/скриншот; CI не зависит от эмулятора |
| RSP/RDP сложнее оценки | S39–40 только boot/clear; RSP/RDP — S43–44; CPU-fallback как аварийный путь |
| ROM header/CRC ломается | `scripts/verify_n64_rom.py` в CI + negative-check битого ROM |
| N64 Release без `-DNDEBUG` | bring-up профиль; релизный профиль и размеры — S45–46 |

## Ссылки

- [`../N64.md`](../N64.md) — reference-факты и build/run.
- [`../plans/sprint_39_40.md`](../plans/sprint_39_40.md) — чек-лист.
- [Nintendo 64 Programming Manual](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro01/index.html).
- [libdragon](https://github.com/DragonMinded/libdragon) (MIT), `boot/README.md` (IPL3).
- [gopher64](https://github.com/gopher64/gopher64) v1.2.1.
- [mupen64plus-core#1185](https://github.com/mupen64plus/mupen64plus-core/issues/1185) — причина отказа от mupen64plus.
