# Спринт 29–30 — VDP2: фоновые слои и композитинг (недели 57–60)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13).
> Источник задач: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md) §4 «Будущие итерации» — Saturn, S21–32.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-11):** **закрыт**. NBG1 bitmap + VDP1 compositing +
> VBlank scroll приняты в Mednafen 1.32.1; 10-минутный stress PASS
> (`exit 124`, f=17760); remote CI `build-saturn` зелёный (run 43085389214).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 29–30 |
| Недели | 57–60 — 06.12.2027 – 02.01.2028 |
| Веха | v0.2.0 — Sega Saturn (итерация 3) |
| Цель | VDP2-backend: фоновый слой, bitmap/scroll, палитра, приоритеты и синхронизация VDP1+VDP2 в композитной сцене |
| Входные критерии | Спринт 27–28 завершён (VDP1-полигоны, текстуры, порядок) ([`sprint_27_28.md`](sprint_27_28.md)) |
| На критическом пути | Да |
| Выходные артефакты | `vdp2_backend.cpp`, NBG-слой, палитра, compositor, golden-тесты, обновлённые docs |
| Следующий спринт | [`sprint_31_32.md`](sprint_31_32.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 55 | 80 | 55.8 | 7.5 | 70.8 | 40–60 ч | ⚠ Синхронизация VDP1/VDP2 — главный риск; нужен ранний прототип композитинга |

## Задачи

**Неделя 57 — VDP2-фундамент:**

- [x] [B] Проверить и зафиксировать карту VDP2 VRAM: pattern name table, bitmap/pattern data, palette, scroll registers; разрешение 320×224
- [x] [B] Один NBG-слой (bitmap) с прокруткой: загрузка фона, палитра, priority; детерминированная инициализация
- [x] [B] VDP2-регистры: display enable, backdrop, screen size, scroll offsets; сброс при shutdown
- [x] [B] Тесты: слой виден, scroll offset применяется, wrap корректен, палитра не повреждена
- [x] [B] Документация: `docs/SATURN.md` — карта VDP2 и ограничения

**Неделя 58 — Композитинг и синхронизация:**

- [x] [B] Композитинг VDP1 поверх VDP2: priority, прозрачные пиксели, порядок слоёв; без Z-буфера
- [x] [B] VBlank-синхронизация: переключение VDP1 framebuffer, обновление VDP2, отсутствие tearing
- [x] [B] Связь камеры и scroll: fixed-point → VDP2-регистры; детерминизм при повторном прогоне
- [x] [B] Golden композитного кадра: фон + 3D-объект; сравнение между прогонами
- [x] [B] Диагностика: переполнение VRAM, неверная палитра, недоступный слой — `false` + сообщение

**Неделя 59 — Сцена и ассеты:**

- [x] [B] Демо-сцена: фон (небо/пол) + 3D-объект; приоритеты и перекрытия проверены
- [x] [B] Ассет-пайплайн: фоновая текстура 256×256 → VDP2 bitmap; палитра; не ломать `.tex` v1
- [x] [B] Производительность: VDP2 fillrate/VRAM, стоимость обновления scroll; baseline в `docs/benchmarks/saturn_vdp2_s29_30.yaml`
- [x] [H] Параллакс/второй слой — **перенесён** на post-v0.2.0 (scope/budget; v0.2.0 — один NBG1)
- [x] [B] Тесты malformed/overflow для VDP2-ассетов

**Неделя 60 — Гейт:**

- [x] [B] 10-минутный стресс композитной сцены в pinned-эмуляторе; отсутствие tearing/hang
- [x] [B] Golden и скриншот; сравнить с PC/PS1, задокументировать отличия
- [x] [B] CI: `build-saturn.yml` зелёный (run 43085389214); `build-pc`, `build-ps1`, `tests` — success; артефакт образа загружен
- [x] [B] CHANGELOG + документация; гейт: VDP1+VDP2 синхронизированы, фон виден, priority корректен

## Критерии приёмки спринта 29–30

- [x] Фоновый слой VDP2 виден за 3D-объектом; priority и прозрачность корректны
- [x] VDP1+VDP2 синхронизированы; в 10-минутном прогоне нет tearing/hang
- [x] Композитный golden стабилен; отличия от PC/PS1 объяснены
- [x] VRAM/палитра/scroll укладываются в бюджет; ошибки возвращают диагностику
- [x] PC/PS1 сборки и тесты зелёные

## Результаты (2026-11-11)

- **VDP2 backend:** `src/hal/saturn/vdp2_backend.*` — NBG1 8bpp bitmap
  1024×256 (256 KiB VDP2 VRAM), палитра 7 (VDP1 CLUT 1..6), тайлинг
  `TEX1.TEX` 256×256 ×4 по X, VBlank-коммит scroll (fixed 16.16 → px),
  диагностика размеров/инициализации.
- **Композитинг:** порядок слоёв NBG0 (VDP2 log) → VDP1 → NBG1 → RBG0 →
  NBG2 → NBG3; VDP1 framebuffer при активном фоне стирается в
  `transparent` (`EWDR=0`), поэтому NBG1 виден сквозь прозрачные пиксели;
  куб остаётся поверх фона. Без Z-буфера, Painter's порядок VDP1 сохранён.
- **Сцена:** фон `TEX1.TEX` + текстурированный Gouraud-куб `CUBE.MSH` +
  `TEX0.TEX`; статичный scroll base (32,16), пад ←/→/↑/↓ панорамирует
  fixed-point scroll в пределах bitmap.
- **Метрики:** VDP2 bitmap 256 KiB (50% VDP2 VRAM), палитра 7 (512 B CRAM),
  asset heap transient 65 KiB; VDP1 `c=6` видимых треугольников,
  `o=40`‰ overdraw; scroll commits ≈2 на кадр (VBlank 60 Гц, game ~30 FPS).
  Baseline — `docs/benchmarks/saturn_vdp2_s29_30.yaml`.
- **Golden:** `screenshots/saturn_vdp2_s29_30.png` (фон + куб);
  `screenshots/saturn_vdp2_scroll_s29_30.png` (смещённый scroll).
  Детерминизм сцены подтверждён crop-сравнением (320×200):
  `sha256 776b439a45c1eb0894133967c11f49f190894b905058681cee07dcac651bd0ba`
  совпал между прогонами f=240 и f=1440.
- **Тесты:** host `saturn_hal_tests` — **112/112** (VDP2 init/upload/
  scroll/диагностика); PC-debug — **289/289** (1 skip); `saturn-single/
  hang/flood` и `ps1-release` собираются. `pc-release` — ограничение
  окружения (`libibus-1.0-dev` для SDL3), не код S29–30.
- **10-минутный stress:** `timeout 640` → `exit 124` (SIGTERM), без
  hang/tearing/сброса; финальный кадр `f=17760` (~29.6 FPS), `c=6`, `o=40`;
  скриншот `screenshots/saturn_vdp2_stress_s29_30.png`.
- **CI:** remote GitHub Actions на `3213c6b` — `build-saturn` success
  (run 43085389214, артефакт `retro-saturn-disc`), `build-pc` success
  (macOS/Ubuntu/Windows), `build-ps1` success, `tests` success;
  эмулятор в CI не запускается, BIOS не вендорится.

## Риски

- **Сложность VDP2.** Митигация: один NBG-слой в v0.2.0, без спецэффектов; расширение — после приёмки.
- **Синхронизация VDP1/VDP2.** Митигация: ранний прототип композитинга, VBlank-контракт, golden.
- **VRAM/палитра.** Митигация: статическая карта, проверки переполнения, минимальный набор форматов.
- **Scope creep.** Митигация: параллакс/второй слой — только [H]/[M], не блокируют v0.2.0.
