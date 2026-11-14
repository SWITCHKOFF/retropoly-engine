# Спринт 27–28 — VDP1: полигоны и текстуры (недели 53–56)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13).
> Источник задач: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md) §4 «Будущие итерации» — Saturn, S21–32.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-11):** **закрыт**. Реализация (VDP1 backend, VRAM/CLUT,
> triangle→quad, ADR-010) и эмуляторный visual gate в Mednafen 1.32.1
> (Japanese BIOS 1.01) выполнены; 5-минутный прогон без краша; golden
> детерминирован (см. «Результаты»).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 27–28 |
| Недели | 53–56 — 08.11.2027 – 05.12.2027 |
| Веха | v0.2.0 — Sega Saturn (итерация 3) |
| Цель | VDP1-backend: textured Gouraud-примитивы, политика triangle→quad, VRAM-текстуры/CLUT, command list и порядок без Z-буфера; golden-тесты |
| Входные критерии | Спринт 25–26 завершён (Master+Slave, синхронизация, fallback) ([`sprint_25_26.md`](sprint_25_26.md)) |
| На критическом пути | Да |
| Выходные артефакты | `vdp1_backend.cpp`, `triangle_to_quad`, VRAM-аллокатор, texture upload, golden-тесты, ADR-010 |
| Следующий спринт | [`sprint_29_30.md`](sprint_29_30.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 40 | 60 | 90 | 61.7 | 8.3 | 78.3 | 40–60 ч | ⚠ Выше номинала: quad-конверсия и fillrate — главные риски; часть [M] переносить в S29–30 |

## Задачи

**Неделя 53 — VDP1-фундамент:**

- [x] [B] Проверить и зафиксировать карту VDP1 VRAM: framebuffer, command table, texture area, CLUT area; точные размеры/выравнивание — по первоисточникам
- [x] [B] Command list: связанные команды, end-маркер, проверка переполнения; детерминированный порядок; silent drop запрещён
- [x] [B] Texture upload: 8bpp CI8 + 256-entry CLUT; 4bpp — опционально; максимум 256×256; DMA или CPU-копия с корректным выравниванием
- [x] [B] ADR-010: политика triangle→quad. Определить детерминированное преобразование (например, вырожденный quad или явное правило упаковки), артефакты и тесты; не менять `.mesh` v1
- [x] [B] Depth/order: Painter's algorithm, без Z-буфера; порядок команд и tie-breaker для равной глубины

**Неделя 54 — Растеризация:**

- [x] [B] `hal_render_backend_draw_triangle` → VDP1 quad: экранные координаты, fixed-point, Gouraud-цвета, текстура
- [x] [B] Clip/cull: near/far во view-space, границы экрана, backface; reject с диагностикой, без тихого отбрасывания
- [x] [B] VDP1 framebuffer readback или детерминированный скриншот эмулятора → golden-хэш
- [x] [B] Тесты W0-подобных кейсов: visible, backface, near, far, invalid depth; переполнение command list
- [x] [B] Проверить координатную систему и winding Saturn; зафиксировать отличия от PC/PS1

**Неделя 55 — Текстуры, палитры, производительность:**

- [x] [B] Форматы текстур и палитр: CI8/CLUT, UV-квантование, texture window, переключение CLUT между примитивами
- [x] [B] Fillrate/overdraw: измерить бюджет команд и заполнения; зафиксировать baseline и политику деградации
- [x] [B] Malformed-ассеты и переполнение VRAM: rollback, `false` + диагностика, 0 байт persistent-памяти при ошибке
- [x] [H] `Render_OverlayText` для Saturn (VDP2/текст) — если нужен для QA; иначе перенести
- [x] [B] Документация: `docs/ASSETS.md` (Saturn-путь), `docs/SATURN.md` (VDP1-ограничения), CHANGELOG

**Неделя 56 — Интеграция и гейт:**

- [x] [B] Текстурированный Gouraud-примитив в эмуляторе; скриншот и golden
- [x] [B] Сравнить с PC/PS1 reference; задокументировать ожидаемые отличия (320×224, quad-конверсия, порядок)
- [x] [B] CI: `build-saturn.yml` собирает образ; PC/PS1 тесты зелёные
- [x] [B] Гейт: треугольник/quad виден, текстура и Gouraud корректны, порядок без Z-буфера стабилен

## Критерии приёмки спринта 27–28

- [x] Textured Gouraud-примитив отображается в pinned-эмуляторе Saturn
- [x] Политика triangle→quad задокументирована в ADR-010 и покрыта тестами
- [x] Порядок без Z-буфера детерминирован; переполнение command list/VRAM возвращает ошибку
- [x] Golden стабилен между прогонами; отличия от PC/PS1 объяснены и задокументированы
- [x] PC/PS1 сборки и тесты не регрессировали

## Результаты (2026-11-11)

- **Исправлены два блокера визуального gate:**
  (1) `vdp1_texture.cpp` передавал в мост смещение **относительно**
  `VDP1_TEXTURE_BASE`, а мост писал по нему как по абсолютному — текстура
  попадала в command table, а `SRCA` указывал в пустую область. Теперь
  адрес абсолютный: `VDP1_TEXTURE_BASE + vram_offset`; upload — `jo_dma_copy`
  (как в Jo Engine sprite path), тест обновлён.
  (2) Gouraud-таблица VDP1 — **аддитивные offsets** с нейтралью 16
  (`0x4210` в трёх каналах), а не абсолютные RGB555. Для flat база — цвет
  первой вершины; для textured база — белый, поэтому тинт не уничтожает
  текстуру. Это соответствует Jo Engine `GOUR` и Mednafen
  `GourauderTheTerrible`.
- **Visual gate (Mednafen 1.32.1, Japanese BIOS 1.01, softfb):**
  текстурированный Gouraud-куб `CUBE.MSH` + `TEX0.TEX` виден поверх
  golden-clear; Painter's-порядок стабилен; 5-минутный `timeout 320`
  завершился `exit 124` (SIGTERM), fatal/assert/hang нет.
  Скриншот: [`screenshots/saturn_vdp1_s27_28.png`](../screenshots/saturn_vdp1_s27_28.png).
- **Golden/детерминизм:** статичная сцена; crop 320×200 двух скриншотов
  (60 с и 300 с одного прогона) побайтно совпал
  (`sha256 df55cda9db4d7a341566d9ba18803922…`). `rpe_saturn_vdp1_fb_hash`
  остаётся программным golden-API (host-тест `Vdp1FramebufferGoldenHashIsStable`);
  в demo он не вызывается каждый кадр, чтобы не читать VDP1 VRAM во время
  отрисовки.
- **Fillrate/overdraw baseline:** 6 видимых textured-треугольников
  (backface culled), `c=6`, overdraw `o=40` (‰ экрана, ~2.9k px/кадр);
  5-минутный прогон — ~30 FPS (120 кадров / ~4.0 с; VBlank 60 Гц,
  игра пропускает каждый второй VBlank из-за S25–26 parallel workload).
  Политика: command list ≤ 511 команд, overdraw ≤ 1000‰ (1× экран) для
  demo; превышение — явная диагностика/деградация, не silent drop.
- **PC/PS1 reference:** PC/PS1 используют перспективно-корректные UV и
  320×240; Saturn S27–28 — 320×224 (crop 8), аффинные UV, triangle→quad
  (каждый треугольник получает полный source rect, поэтому грань куба
  читается как два текстурных треугольника). Это ожидаемое отличие
  политики ADR-010, а не регрессия; скриншоты PC/PS1 —
  `screenshots/hello_triangle_pc.png`, `hello_triangle_ps1.png`.
- **Overlay:** `Render_OverlayText` для Saturn реализован через VDP1, но
  в demo не включён: 10-символьная строка добавляет ~156 команд и роняет
  кадр до ~20 FPS. QA-строка `S27-28` выводится через VDP2 log; VDP1-overlay
  остаётся API и переносится в S31–32 (hello_triangle/rotating_cube).
- **Тесты/сборки:** host `saturn_hal_tests` — **112/112** (добавлены
  VDP2-тесты S29–30); PC-debug — 289/289 (1 skip); `saturn-release`
  собирается и грузится в pinned Mednafen.

## Риски

- **Артефакты triangle→quad.** Митигация: явная политика, golden-тесты, сравнение с эталоном; не менять `.mesh` v1.
- **Fillrate VDP1.** Митигация: меньше overdraw, бюджет команд, ранний benchmark.
- **VRAM/CLUT.** Митигация: статическая карта, выравнивание, проверка переполнения, отказ от лишних форматов.
- **Различия эмулятора и железа.** Митигация: pinned-эмулятор, документировать, что v0.2.0 — emulator validation, не real-hardware claim.
