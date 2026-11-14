# ADR-008: Saturn backend — toolchain, разрешение и границы v0.2.0

- **Статус:** Accepted
- **Дата:** 2026-11-09
- **Спринт:** 21–22 (недели 41–44), веха v0.2.0
- **Связанные документы:** [`../plans/sprint_21_22.md`](../plans/sprint_21_22.md),
  [`../SATURN.md`](../SATURN.md), [`../BUILDING.md`](../BUILDING.md),
  [`../../VERSION.txt`](../../VERSION.txt), ADR-001 (compile-time HAL),
  ADR-003 (Painter's algorithm), ADR-007 (память).

## Контекст

Saturn — первая платформа v0.2.0 после MVP v0.1.0 (PC + PS1). Нужно
зафиксировать решения, которые дорого менять позже: toolchain, эмулятор,
разрешение, бюджет памяти, политику triangle→quad и границы первого
Saturn-спринта. Reference-факты Saturn перепроверены по первоисточникам
(см. [`../SATURN.md`](../SATURN.md)); этот ADR фиксирует **нормативные
решения движка**, а не пересказ hardware-справочника.

## Решения

### 1. Compile-time HAL, без runtime backend dispatch

Saturn — третий compile-time backend: CMake-опция `PLATFORM_SATURN`, ровно
один из `PLATFORM_PC` / `PLATFORM_PS1` / `PLATFORM_SATURN`. Публичный
[`../../include/retro_engine/hal/platform.h`](../../include/retro_engine/hal/platform.h) не включает SH-2/Jo
Engine; платформенные заголовки живут только в `src/hal/saturn/*.c/.cpp`.
`src/CMakeLists.txt` выбирает `hal/saturn/` на этапе конфигурации. Runtime
virtual dispatch в кадре не появляется (ADR-001).

**Следствие:** один бинарник не переключает платформу; PC/PS1 API не
ломаются, Saturn-код не протекает в общий слой.

### 2. Toolchain: Jo Engine v2026.2 + bundled sh-none-elf-gcc 8.2.0

Выбран **Jo Engine v2026.2** (MIT, commit `5e99944238030821a48e17a40abefd77e81953c2`)
вместе с его bundled GNU GCC 8.2.0 для `sh-none-elf` (big-endian SH-2) и
newlib-заголовками. Jo Engine не вендорится в git: SDK устанавливается в
`JO_ENGINE_ROOT` (env-install), CI клонирует immutable tag и проверяет
SHA-256 компилятора/cc1/cc1plus/as/ld/libc/libgcc по
[`../../ci/joengine-v2026.2.sha256`](../../ci/joengine-v2026.2.sha256).
Linker script — `Compiler/COMMON/jo.linker`; entry `___Start`, старт с
`0x06004000`.

Альтернативы (SATURN SDK/SGL, отдельный `sh-elf-gcc`, Makefile-обёртка)
отклонены на v0.2.0: Jo Engine — живой MIT-проект с bundled toolchain,
примерами и emulator-скриптами; второй toolchain не тянем (правило «один
pinned toolchain на платформу»).

**Следствие:** `saturn-release` воспроизводим с чистого checkout при
наличии `JO_ENGINE_ROOT`; обновление Jo Engine/компилятора — отдельная
задача с ADR.

### 3. Эмулятор: Mednafen 1.32.1, BIOS не вендорится

Pinned эмулятор для ручной приёмки — **Mednafen 1.32.1**
(SHA-256 в `VERSION.txt`). BIOS (`ss.bios_na_eu` / `ss.bios_jp`) —
собственность пользователя, в репозиторий/CI/артефакты не попадает.
CI собирает и валидирует образ, но **не запускает** эмулятор; smoke-тест
60 с и скриншот — ручной чек-лист [`../SATURN.md`](../SATURN.md).
Kronos/Beetle Saturn остаются альтернативой для сравнения, но не gate.

**Следствие:** CI-гейт не зависит от BIOS; воспроизводимость ручного
прогона обеспечивается версией эмулятора и настройками.

### 4. Разрешение: native 320×224 NTSC 60 Гц; reference 320×240 — crop

Target profile: **320×224, NTSC, 60 Гц, 6-button pad, CD-ROM**. Общий
reference-контракт движка остаётся 320×240 (PC/PS1, `RENDER_FB_WIDTH/HEIGHT`);
Saturn-бэкенд отображает **центральные 224 строки** reference-кадра
(строки 8..231), без нецелого масштабирования. Это policy **crop**;
letterbox и 320×240-режим не выбираются в v0.2.0. 320×240 остаётся
аппаратно возможным (VDP2 non-interlace 320×240), но не является target
profile и не меняет общий контракт без нового ADR.

**Следствие:** вертикальные 16 строк reference-кадра на Saturn не видны;
игры/примеры не должны размещать критичный HUD в строках 0..7 и 232..239.
Опция 320×240 может быть добавлена позже отдельным ADR.

### 5. Память: статические арены, ledger, без heap

Бюджет (reference-факты + engine ledger, `src/hal/saturn/saturn_memory.h`).
**Уточнение 2026-11-10 (ADR-009):** H = `0x06000000` (high-speed CPU bus),
L = `0x00200000` (low-speed shared bus) — в S23–24 таблица была подписана
наоборот; кодовые константы/документация исправлены в S25–26.

| Ресурс | Ёмкость | Использование v0.2.0 |
|---|---|---|
| Work RAM-H | 1 MiB (`0x06000000`) | код/данные/стек Master; program limit `0x060FBC00` |
| Work RAM-L | 1 MiB (`0x00200000`) | asset arena (mesh/CI8-текстуры/палитры) |
| Sound RAM | 512 KiB | SCSP-драйвер (post-MVP; не занимаем) |
| VDP1 VRAM | 512 KiB | команды/CLUT/текстуры (S27–28) |
| VDP1 frame buffers | 2×256 KiB | A/B page flip |
| VDP2 VRAM | 512 KiB | фоны/NBG (S29–30) |
| VDP2 Color RAM | 4 KiB | палитры RGB555 |
| SCU DSP SRAM | 32 KiB | SCU DSP (не бюджет движка) |

Runtime `new`/`delete`/`malloc`/`free` запрещены (ADR-007). Бюджет
проверяется `static_assert`-ами и linker script ASSERT; фактические
значения — MemoryProfiler (S23–24).

### 6. Triangle→quad: engine policy, не hardware claim

VDP1 рисует четырёхугольники (polygons/distorted sprites). Общий `.mesh` v1
остаётся triangle list. Высокоуровневая политика v0.2.0:

1. пара соседних индексов с общей стороной и одинаковым материалом
   объединяется в quad;
2. если пары нет — треугольник вырождается в quad (`v3 = v2` или
   `v3 = v0`) без изменения winding/UV;
3. clipping/culling выполняются до конвертации; порядок Painter's
   сохраняется.

Детали (какие пары, допуски, VDP1-команды) — S27–28. Отдельный Saturn-
формат ассетов не вводится; `.mesh`/`.tex` v1 не ломаются.

### 7. Input: 6-button pad → INPUT_PAD_*

D-pad → `INPUT_PAD_UP/DOWN/LEFT/RIGHT`; A→CROSS; B→CIRCLE; C→SQUARE;
X→TRIANGLE; Y→L1; Z→R1; L→L2; R→R2; START→START. Select на 6-button pad
отсутствует. Enum расширяется только перед `INPUT_PAD_COUNT` и только с
ADR/обновлением всех бэкендов. Раскладка документируется в
[`../SATURN.md`](../SATURN.md) и `docs/API.md`.

### 8. FileSystem: read-only ISO9660, 8.3, без host-путей

Образ — ISO9660 (2048-byte sectors, CUE MODE1/2048 — capability pinned
toolchain). Имена 8.3, upper-case, `NAME.EXT;1`; разделитель — `\`.
Host-пути, `..`, абсолютные пути и запись запрещены. Отсутствующий/
повреждённый файл → `{nullptr, 0}` и диагностика, без краша. Реализация —
S23–24; в S21–22 `FileSystem_LoadFile` — безопасная заглушка.

### 9. Границы v0.2.0 и S21–22

S21–22: research, toolchain, preset, HAL skeleton, bootable backdrop, CI.
S23–24: Master HAL lifecycle/input/CD-FS/render clear. S25–26: Master+Slave
SCU sync. S27–28: VDP1 polygons/CLUT/triangle→quad. S29–30: VDP2 planes/
compositing. S31–32: hello_triangle + release. Вне scope v0.2.0: полноценное
3D-демо, audio/SCSP PCM, CD streaming, BIOS, PAL, 640×480, 3D pad.

## Последствия

- PC/PS1 сборки и тесты не меняются; Saturn — отдельный preset/CI-контур.
- `saturn-release` требует внешний `JO_ENGINE_ROOT`; «чистый checkout» +
  documented fetch — часть gate.
- Появление BIOS/эмулятора в CI запрещено; ручной smoke-тест остаётся
  обязательным.
- Любое изменение toolchain, разрешения, памяти или triangle→quad policy —
  новый ADR (или revision этого ADR) с обновлением `docs/SATURN.md`.

## Риски и митигации

| Риск | Митигация |
|---|---|
| Зрелость Jo Engine/GCC 8.2.0 | pin tag+commit+sha256; минимальный SDK-пример в CI; fallback — собственный toolchain file (решение отложено) |
| Нет BIOS в CI/у разработчика | CI — только сборка/структурная валидация; ручной чек-лист с Mednafen 1.32.1; BIOS не вендорим |
| Расхождение reference-фактов | `docs/SATURN.md` отделяет verified hardware facts от engine policy; 28.6 MHz и 256×256 отмечены как уточнения |
| Scope creep | S21–22 только boot/backdrop; 3D/VDP1/VDP2 — отдельные спринты |

## Ссылки

Первоисточники и проверенные значения — в [`../SATURN.md`](../SATURN.md)
(раздел «Источники»). Ключевые: Sega VDP1 User's Manual ST-013-R3,
VDP2 User's Manual ST-058-R2, SCU User's Manual ST-097-R5, SMPC User's
Manual ST-169-R1, Hitachi SH7604 Hardware Manual; Jo Engine v2026.2
(README/LICENSE); Mednafen 1.32.1.
