# ADR-015: N64 renderer — RSP T&L, RDP, Z-буфер и TMEM-текстуры

- **Статус:** Accepted
- **Дата:** 2026-11-14
- **Спринт:** 43–44 (недели 85–88), веха v0.3.0
- **Связанные документы:** [`../plans/sprint_43_44.md`](../plans/sprint_43_44.md),
  [`../N64.md`](../N64.md), ADR-014 (N64 backend),
  [`../../ci/normative_contracts.yaml`](../../ci/normative_contracts.yaml),
  [`../../VERSION.txt`](../../VERSION.txt).

## Контекст

S41–42 закрыли HAL N64: VI 320×240, input, read-only DragonFS, memory
ledger 4 MiB и render skeleton. S43–44 — самый рискованный спринт N64:
нужно реально задействовать RSP для T&L, RDP для растеризации, аппаратный
Z-буфер, perspective-correct текстуры и TMEM 4 KiB, не выходя за 4 MiB
RDRAM.

Pinned libdragon `e356bf3` содержит `rdpq`/`rspq`, но **не содержит
t3d** (T&L-библиотека появилась позже). Обновление libdragon запрещено
без отдельного ADR (ADR-014). Поэтому требуется решение: собственная
RSP-программа, ограниченный CPU-T&L или смена пина.

## Решения

### 1. RSP: собственный RSPQ-overlay `rsp_tnl` (не t3d)

Выбран **собственный минимальный RSP-микрокод** `src/hal/n64/rsp_tnl.S`:
RSPQ-overlay с одной командой `TnlCmd_Transform`. Команда принимает
RDRAM-адреса input-layer, matrix-layer и output-layer; код DMA-транспортирует
их в DMEM, выполняет два `4×4 · vec4` умножения (Q16.16, MAC
`vmudl/vmadm/vmadn/vmadh`) и DMA-транспортирует clip-координаты обратно.

- **Формат вершины:** 16 компонент Q16.16 на 3 вершины (v0/v1/v2.xyzw,
  w = 1.0); слот — два `vec_slot_t` (hi/lo) по 8 компонент.
- **Матрица:** GL column-major Q16.16, **отдельный RDRAM-буфер на слот**;
  это устраняет гонку «один DMEM-буфер на все команды» при нескольких
  разных MVP в кадре.
- **Батч:** до 512 слотов, static `.bss`, без per-frame malloc;
  переполнение — явный `false` + диагностика (silent drop запрещён).
- **Синхронизация:** `rspq_wait()` в `end_frame`, D-cache invalidate
  output-слотов; RDP-команды ставятся после завершения T&L.
- **Lighting:** RSP-ядро же вычисляет `dot(n, L)` (нормаль подаётся как
  vec4 с w=1, матрица — строка `L`); CPU только умножает dot на цвет
  света и добавляет ambient. Это осознанный компромисс: тяжёлая
  вершинная математика — на RSP, а масштабирование цвета/ambient —
  на VR4300 (24 вершины/куб, 4 куба = 96 dot/кадр).
- **CPU-fallback:** только аварийный режим (RSP недоступен/явный toggle
  C-Down). Fallback считает ту же fixed-математику на CPU и даёт
  **идентичный** W0-hash (проверено).

**Альтернативы:** (a) обновить libdragon до t3d — отклонено: dependency
freeze/ADR-014, риск для CI; (b) CPU-only T&L — не проходит требование
«RSP реально используется»; (c) полный custom T&L с lighting/clipping
в ucode — отклонено по budget/risk (нужен S45–46+).

### 2. RDP: rdpq + аппаратный Z 320×240

Растеризация — libdragon `rdpq` (RSP-ассемблируемые TRI-команды):
`rdpq_triangle()` с `TRIFMT_ZBUF_SHADE_TEX`/`TRIFMT_ZBUF_SHADE`.
Z-буфер — static `uint16_t[320×240]` (150 KiB RDRAM), `FMT_RGBA16`,
`rdpq_attach(color, z)` + `rdpq_clear_z(0xFFFC)` каждый кадр.
RDP использует 18-bit internal depth; в RDRAM хранится 16-bit
представление, как требует API libdragon. Painter's-сортировка ядра
остаётся для порядка подачи, но корректность перекрытий обеспечивает Z.

### 3. Текстуры: CI8-полосы 32×64 + TLUT; TMEM ≤ 4 KiB

TMEM 4 KiB, верхняя половина (2 KiB) — палитры/TLUT. Для CI8-текстуры
полоса `sw×h + 512 B` обязана быть ≤ 4 KiB, а texture area ≤ 2 KiB.
Политика backend:

- движковые текстуры больше 64×64 **детерминированно downsample'ятся**
  nearest-семплингом до ≤64×64 (TEX0 256×256 → 64×64);
- ширина полосы: `min(64, 2048/h)`, выровненная вниз до степени двойки
  (64×64 → 32×64, 32×64 → 32×64, 64×32 → 64×32);
- каждая полоса грузится `rdpq_tex_upload_sub` + `rdpq_tex_upload_tlut`;
  треугольник клипается по `u` на границах полос (`tnl_clip_to_u_range`);
  TLUT — PS1 RGB555+STP → N64 RGBA5551 (STP трактуется как opaque);
- TMEM на полосу: 2×64×32 CI8 + 512 B TLUT = **2560 B** ≤ 4096 B;
- фильтрация — **hardware bilinear** для textured-групп; untextured/
  overlay — point. Это задокументированное отличие от PS1 nearest.

### 4. W0-golden и benchmark

5 W0-кейсов (visible/backface/near-cross/far-cross/behind) выполняются
на boot через `Render_*`, framebuffer читается app-side и хэшируется
FNV-1a 32 (RGB). Хэши зафиксированы в ROM и `docs/benchmarks/n64_render_s43_44.yaml`;
CPU-fallback visible даёт тот же hash. W1-подобная нагрузка — 4 куба /
48 треугольников, 320×240, NTSC 60 Hz; frame time и TNL/RDP/lighting
замеряются app-side (`get_ticks_us`).

## Последствия

- `rsp_tnl.S` — единственный собственный ucode; обновление libdragon
  по-прежнему требует ADR. `rsp_tnl` собирается тем же CMake-правилом,
  что `n64.mk` (objcopy .text/.data → `rpe_tnl_*`).
- RDP-команды идут через `rdpq`; «RSP→RDP напрямую» не реализовано:
  RSP выдаёт clip-вершины в RDRAM, CPU ставит RDP-команды. Это
  документированная граница v0.3.0; прямой вывод RDP-команд из ucode —
  кандидат S45–46.
- Текстурная политика ограничивает N64-контент 64×64; большие текстуры
  downsample'ятся без silent drop (лог `texture N: WxH → 64x64`).
- CPU-fallback существует для gate/сравнения, но не является штатным
  режимом; при недоступном RSP backend логирует предупреждение.

## Приёмка S43–44 (2026-11-14)

| Проверка | Результат |
|---|---|
| RSP T&L self-test | `max_error=0` (tol 8 Q16.16) на W0-векторах |
| RSP lighting dot | `max_error=0` (tol 8) |
| W0 5 кейсов | visible `9e92e718`; reject-кейсы `569949c5`; PASS |
| CPU-fallback | visible hash `9e92e718` = RSP; PASS |
| Z/текстуры | 4 текстурированных куба, TMEM 2560 B/полоса, bilinear |
| Z self-test | near-first/far-second pixel `(255,0,0)` — PASS |
| Stress | 618 375+ мс / 36 900+ кадров, без ERROR/crash |
| W1 48 треугольников | ≈59 FPS (software RDP gopher64, gate ≥30; target 60 — на железе) |
| 4 MiB base | `--disable-expansion-pak true`: boot + W0/light/Z + W1 PASS |
| Host-тесты | `n64_hal_tests` 61/61 |
| Скриншот | `screenshots/n64_render_s43_44.png` |

## Риски и митигации

| Риск | Митигация |
|---|---|
| RSP-ucode сложнее оценки | одна команда transform; CPU-fallback; self-test на boot |
| TMEM thrash | полосы 32×64, downsample до 64×64, явный бюджет/лог |
| Z/порядок отличается от PS1/Saturn | per-platform golden, документированные отличия; Z не Painter's |
| Эмуляторная точность RDP/RSP | pinned gopher64, app-side hash, ручной gate |
| Bilinear снижает FPS в software RDP | gate ≥30 FPS; target 60 — на железе/при point-политике |

## Ссылки

- [`../N64.md`](../N64.md) — RSP/RDP/Z/TMEM и ручной gate.
- [`../plans/sprint_43_44.md`](../plans/sprint_43_44.md).
- libdragon `examples/rspqdemo` (MIT) — база transform-последовательности.
- [Nintendo 64 Programming Manual](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro01/index.html).
