# ADR-010: VDP1 — полигоны, texture/CLUT и политика triangle→quad

- **Статус:** Accepted
- **Дата:** 2026-11-10
- **Спринт:** 27–28 (недели 53–56), веха v0.2.0
- **Связанные документы:** [`../plans/sprint_27_28.md`](../plans/sprint_27_28.md),
  [`../SATURN.md`](../SATURN.md), [`ADR-008`](ADR-008-saturn-backend.md),
  [`ADR-009`](ADR-009-master-slave.md), Sega **VDP1 User's Manual**
  ST-013-R3-061694, Jo Engine v2026.2 (`vdp1_command_pipeline.c`,
  `sprites.c`, `internal/vdp1_command_pipeline.h`).

## Контекст

S23–26 дали рабочий HAL Saturn: lifecycle, ввод, ISO9660, VDP2 clear/log и
Master+Slave. VDP1 до S27–28 не рисовал: `draw_triangle`, texture upload и
readback честно возвращали `false`.

VDP1 имеет принципиальные ограничения, определяющие дизайн:

- полигон VDP1 — **четырёхугольник**; треугольника как примитива нет;
- текстурный примитив (distorted sprite) отображает **прямоугольный
  character pattern** на quad; произвольных per-vertex UV нет;
- Z-буфера нет: порядок задаёт программный Painter's algorithm;
- 8bpp CI8 работает через 256-цветный CLUT в VDP2 Color RAM;
- pattern width кратна 8, height — 1..255 (256 нельзя закодировать в
  CMDBSIZE);
- VDP1 VRAM — 512 KiB, отдельно от 2×256 KiB framebuffer A/B.

Нужно встроить эти ограничения в уже существующий контракт ядра
(`HalRenderBackendVertex` — view-space + UV + Gouraud; ядро уже делает
near/far clip, backface cull и Painter's sort).

## Решения

### 1. Карта VDP1 VRAM

Карта фиксируется в [`../../src/hal/saturn/vdp1_vram.h`](../../src/hal/saturn/vdp1_vram.h)
и совпадает с pinned Jo Engine v2026.2:

| Диапазон | Размер | Владелец |
|---|---:|---|
| `0x00000..0x0001F` | 32 B | jump table VDP1 |
| `0x00020..0x07FFF` | 32 736 B | command table A (Jo Engine) |
| `0x08000..0x0FFFF` | 32 768 B | command table B (Jo Engine) |
| `0x10000..0x7E4F7` | 451 832 B | RPE texture area (first-fit) |
| `0x7E4F8..0x7E6F7` | 512 B | RPE Gouraud tables, 2×32 |
| `0x7E700..0x7FF7F` | 6 016 B | Jo Engine 3D clip tables (не используются) |
| `0x7FF80..0x7FEFF` | 384 B | reserved |
| `0x7FF00..0x7FFFF` | 256 B | Jo Engine light Gouraud tables |

`static_assert` в `vdp1_vram.h` проверяет отсутствие пересечений и точное
покрытие 512 KiB. Framebuffer A/B (`0x25C80000`, 2×256 KiB) в карту не
входит.

### 2. Политика triangle→quad

Нормативная реализация — [`../../src/hal/saturn/triangle_to_quad.*`](../../src/hal/saturn/triangle_to_quad.h).
Правила:

1. **UV-углы.** Каждая из трёх UV-вершин обязана лежать в пределах
   **2 текселей** от одного из четырёх углов исходного прямоугольника
   текстуры. Иначе — `TRI_QUAD_UNSUPPORTED_UV`; backend рисует
   flat-Gouraud с явным счётчиком `uv_fallbacks` (silent drop запрещён).
2. **Различные углы.** Три вершины должны соответствовать трём разным
   углам. Совпадение углов — тот же отказ.
3. **Source rect.** bbox UV квантуется: X — вниз/вверх до 8 текселей
   (требование VDP1), Y — до 1 текселя. Прямоугольник ограничивается
   pattern'ом. UV-подпрямоугольники должны быть выровнены по X на 8
   текселей, иначе вершина окажется дальше допуска 2 текселя и получит
   flat-fallback.
4. **Вырожденный quad.** Четвёртая вершина дублирует **следующий
   назначенный** угол в цикле TL→TR→BR→BL. Последовательность остаётся
   циклическим обходом трёх реальных углов:
   - TL,TR,BR → `[TL,TR,BR,TL]`;
   - TL,TR,BL → `[TL,TR,BL,BL]`;
   - TL,BR,BL → `[TL,BR,BR,BL]`;
   - TR,BR,BL → `[TR,TR,BR,BL]`.
5. **Высота 256.** `.tex` v1 допускает 256×256, VDP1 pattern height —
   255. В v0.2.0 загружаются строки 0..254; последняя строка теряется,
   `height_clamped` инкрементируется и пишется WARNING. Splitting на два
   pattern'а — future work (не ломает `.mesh`/`.tex` v1).
6. **Winding.** Backface culling выполняет ядро (screen-space, 320×240).
   Backend не дублирует cull и не навязывает VDP1 направление обхода:
   quad строится в порядке углов текстуры. VDP1 не делает backface
   culling; порядок вершин задаёт только UV-разметку.

Политика **не меняет** `.mesh` v1: assets остаются triangle list. Треугольник,
у которого UV не в углах, деградирует в flat-Gouraud — это документированное
ограничение, а не ошибка загрузчика.

### 3. Порядок без Z-буфера

Ядро сортирует треугольники far→near (`sort_key.h`: `depth_key` по
убыванию, `submission_index` по возрастанию). Backend подаёт команды в
этом же порядке и присваивает `sort_key = count − 1 − index`: первая
команда получает наибольший ключ. Pinned Jo Engine сортирует команды по
убыванию ключа (farthest first) и стабильно сохраняет порядок при равных
ключах. Итог:

- Painter's порядок = порядок ядра;
- равная глубина → `submission_index` ASC;
- никакого Z-буфера и никакой недетерминированной сортировки.

VDP2 clear/erase выполняется до команд; VDP1 ERASE-цвет (EWDR) равен
цвету кадра. Overlay (`draw_overlay`) добавляется после сцены и получает
меньшие ключи — рисуется поверх.

### 4. Проекция и координаты

Вершины приходят **view-space** (ядро уже применило view·model). Saturn,
как PS1, использует `proj` из `hal_render_backend_set_camera`, а не
`mvp` (mvp — PC-only: PC-шейдер применяет его к своим вершинам).
Проекция — fixed 16.16:

```
clip = proj · (x, y, z, 1)
ndc  = clip.xy / clip.w          (w = −z_view > 0 после CLIP ядра)
sx   = ndc.x·160 + 160
sy   = 120 − ndc.y·120 − 8       (crop reference 320×240 → Saturn 320×224)
```

Округление до целых — half away from zero. Экран — 320×224, origin
top-left, y вниз. Полностью вне экрана — `screen_rejects`; частично —
VDP1 system clip. Никакого `float` в runtime-пути, кроме конверсии
входных float в fixed на границе backend'а.

### 5. Текстуры, CLUT и VRAM

- Формат — `.tex` v1 CI8 (1 байт/пиксель) + 256×RGB555+STP.
- VDP1 pattern: width кратна 8 (8..256), height 1..255; адрес — /8.
- VRAM-аллокатор — first-fit free-list, 32-байтовое выравнивание,
  коалесцирование, явные `OUT_OF_MEMORY`/`FOREIGN_OFFSET`/`DOUBLE_FREE`.
  Утилизируемая ёмкость — 451 808 B; последние 24 B области (451 832 B) —
  alignment padding.
- CLUT — палитры **1..7** (0..255 слова CRAM заняты VDP2-текстом Jo
  Engine). Максимум 7 уникальных CLUT одновременно; восьмая текстура
  получает явный `CLUT_OUT_OF_MEMORY`. Дедупликация палитр не вводится
  (в v0.2.0 достаточно; W1 использует 4 текстуры).
- Upload — CPU-копия 16-битными словами в VDP1 VRAM (boot/loading, вне
  кадрового цикла; SH-2 DMAC не используется, чтобы не зависеть от
  доступности Work RAM-L для DMA).
  Ошибка upload/VRAM/CLUT откатывает и VRAM, и CLUT: persistent-память
  после неудачи — 0 байт.
- `CMDCOLR = clut · 256`; `CMDBSIZE = (width/8) << 8 | height`.

### 6. Gouraud

- Таблица — 4×RGB555 (MSB очищен), адрес /8.
- В VDP1 VRAM два набора по 32 таблицы; набор чередуется по чётности
  кадра, чтобы не перезаписать таблицы, которые VDP1 ещё дочитывает.
- Внутри кадра таблицы дедуплицируются по 4 цветам (куб с одинаковыми
  цветами = 1 таблица).
- Исчерпание набора → flat-цвет первой вершины + `gouraud_fallbacks`
  (явная деградация, не silent drop).
- `grda = 0` и снятый бит GOURAUD при flat-fallback.

### 7. Clip/cull и диагностика

| Этап | Где | Поведение |
|---|---|---|
| near/far | ядро (view-space) | reject треугольника, `RENDER_DIAG_CLIP_REJECT` |
| backface | ядро (screen-space) | reject, `RENDER_DIAG_CULL_REJECT` |
| depth key | ядро | `DEPTH_REJECT` |
| экран 320×224 | backend | полностью вне — `screen_rejects`, `true` (штатный cull) |
| degenerate | backend | `degenerate`, `true` |
| UV не в углах | backend | flat fallback, `uv_fallbacks`, `true` |
| texture source rect | backend | `false` + ERROR |
| command overflow | backend | `false` + ERROR, `overflows` |
| pipeline table full | backend | `false` + ERROR, `pipeline_drops` |

Ни один отказ не является молчаливым: либо счётчик, либо ERROR-лог, либо
явный `false` наверх в `RENDER_DIAG_BACKEND_FAIL`.

### 8. Golden и различия с PC/PS1

- Основной golden — **детерминированный скриншот pinned Mednafen 1.32.1**
  (ручная приёмка, BIOS не вендорится).
- Дополнительно backend умеет `rpe_saturn_vdp1_fb_hash(A/B)`: FNV-1a 32
  по 320×224 словам RGB555 из VDP1 framebuffer. Хэш не требует 307 KiB
  RGBA-буфера (в отличие от PC `Render_TestReadbackHash`), поэтому
  пригоден для SH-2. Демо логирует `[INFO][vdp1] f=… c=… t=… h=…`.
- Ожидаемые отличия от PC/PS1:
  - 320×224 crop (8 строк сверху и снизу reference-кадра);
  - аффинная (не perspective-correct) интерполяция UV — как PS1;
  - triangle→quad: UV только в углах прямоугольника; иначе flat;
  - 256-высокая текстура теряет последнюю строку;
  - порядок — Painter's без Z-буфера; equal-depth tie-breaker —
    submission ASC;
  - CLUT: максимум 7 уникальных палитр;
  - VDP1 integer-координаты (нет 1/16-px wobble PC-шейдера).

## Последствия

- VDP1-путь использует **внутренний** API pinned Jo Engine
  (`__jo_vdp1_add_command`): pipeline сам сортирует, линкует, копирует
  команды в VRAM и синхронизирует VBlank. При обновлении Jo Engine
  проверить `vdp1_command_pipeline.c` и `internal/vdp1_command_pipeline.h`.
- Новые файлы HAL Saturn: `vdp1_backend.*`, `vdp1_command_list.*`,
  `vdp1_texture.*`, `vdp1_vram.*`, `triangle_to_quad.*`.
- Work RAM-L ledger не меняется: command arena 16 KiB переиспользуется
  VDP1-списком (legacy `CommandBuffer` остаётся каркасом метрик).
- VDP1 VRAM 451 832 B textures + 512 B Gouraud + системные области
  Jo Engine. Texture upload — boot/loading, не в кадре.
- `hal_render_backend_read_pixels` на Saturn по-прежнему `false`
  (нет 307 KiB static RGBA); golden readback — `fb_hash`.
- Overlay (`Render_OverlayText`) реализован через те же flat-Gouraud
  команды: полезен для QA и не требует VDP2-текста.

## Риски и митигации

| Риск | Митигация |
|---|---|
| Артефакты triangle→quad | явная политика UV-углов, flat fallback, host-тесты, ADR |
| Winding/VDP1 fill | quad в порядке углов текстуры; проверка на pinned эмуляторе; backface уже отброшен ядром |
| Fillrate/overdraw | pre-clipping disable для полностью видимых команд, бюджет 511 команд, метрики `c`/`o` |
| VRAM/CLUT | статическая карта, 32-байтовое выравнивание, rollback, явные ошибки |
| Перезапись Gouraud-таблиц | два набора по чётности кадра |
| Расхождение эмулятор/железо | pinned Mednafen 1.32.1, документированный emulator-validation (не real-HW claim) |

## Ссылки

- Sega, **VDP1 User's Manual**, ST-013-R3-061694 (quads, CMDBSIZE,
  character pattern, CLUT, framebuffer).
- Sega, **VDP2 User's Manual**, ST-058-R2-060194 (Color RAM).
- Jo Engine v2026.2 `vdp1_command_pipeline.c` (`__jo_vdp1_add_command`,
  radix sort far→near, DMA table), `sprites.c` (8bpp/CLUT),
  `internal/vdp1_command_pipeline.h`.
- [`../SATURN.md`](../SATURN.md) §12 — ручной чек-лист и ограничения.
- [`../../src/hal/saturn/vdp1_vram.h`](../../src/hal/saturn/vdp1_vram.h) —
  исполнимая карта VRAM.
