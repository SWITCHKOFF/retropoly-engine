# Спринт 25–26 — Параллелизм Master + Slave SH-2 (недели 49–52)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13).
> Источник задач: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md) §4 «Будущие итерации» — Saturn, S21–32.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-10):** реализация завершена; gate S25–26 закрыт (см. «Результаты»).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 25–26 |
| Недели | 49–52 — 11.10.2027 – 07.11.2027 |
| Веха | v0.2.0 — Sega Saturn (итерация 3) |
| Цель | Задействовать оба SH-2 через SCU: детерминированное разделение работ, синхронизация, измеримый параллельный workload и fallback на один CPU |
| Входные критерии | Спринт 23–24 завершён (HAL на Master SH-2, ввод, ФС, очистка экрана) ([`sprint_23_24.md`](sprint_23_24.md)) |
| На критическом пути | Да |
| Выходные артефакты | `scu_sync.*`, `slave_worker.*`, fixed-size job queue, ADR-009, benchmark/utilization, тесты, документация |
| Следующий спринт | [`sprint_27_28.md`](sprint_27_28.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 40 | 55 | 85 | 57.5 | 7.5 | 72.5 | 40–60 ч | ⚠ Максимальный риск v0.2.0: sync/cache/load balance; recovery-неделя после S26 |

## Задачи

**Неделя 49 — Архитектура и синхронизация:**

- [x] [B] Определить разделение работ: Master — main loop/игровая логика/backend; Slave — параллельный workload (кандидат: подготовка вершин/command list). Не закреплять VDP1 за Slave как «аппаратное правило» — разделение software-defined
- [x] [B] `scu_sync`: обёртки над SCU-прерываниями, семафорами и барьерами; таймауты и диагностика, без бесконечного busy-wait
- [x] [B] Карта shared-памяти в WRAM-L/WRAM-H: выравнивание, владение, read/write-контракты; учесть кэш SH-2 и явные flush/invalidate
- [x] [B] ADR-009: dual-CPU partitioning, протокол синхронизации, ответственность Master/Slave, fallback
- [x] [B] Схема жизненного цикла: boot Slave → handshake → работа → stop; повторный запуск и ошибка загрузки Slave

**Неделя 50 — Slave worker и очередь:**

- [x] [B] `slave_worker.cpp`: загрузка/запуск Slave, idle-loop, обработка команд, корректная остановка; без динамических аллокаций
- [x] [B] Fixed-size SPSC ring queue: атомарные индексы, memory barriers, детерминированный порядок; переполнение — `false` + диагностика
- [x] [B] Master dispatch API: постановка job, ожидание completion, таймаут, fallback на single-CPU
- [x] [B] Host-тестируемая логика очереди (unit-тесты) + smoke-тест в эмуляторе
- [x] [B] Метрики: счётчики busy/idle для Master/Slave, количество job, время ожидания

**Неделя 51 — Интеграция параллельного workload:**

- [x] [B] Распараллелить один реальный workload (трансформация/клиппинг или подготовка command list) за compile-time/runtime-переключателем
- [x] [B] Сравнить single-CPU и dual-CPU: корректность кадра, frame time, загрузка CPU; зафиксировать baseline
- [x] [B] Синхронизация с VBlank: Slave не пишет в VRAM вне разрешённого окна; отсутствие tearing/race
- [x] [B] `docs/benchmarks/saturn_reference_environment.yaml`: версия эмулятора, настройки, CPU utilization, frame time, git_revision
- [x] [B] Диагностика: переполнение очереди, timeout, повторный job, остановка Slave — без крашей

**Неделя 52 — Стресс и гейт:**

- [x] [B] 10-минутный стресс в pinned-эмуляторе: отсутствие deadlock/hang, стабильный frame time
- [x] [B] Детерминизм: повторный прогон с одинаковым вводом даёт одинаковый кадр/хэш
- [x] [B] Fault injection: Slave не стартовал, Slave завис, очередь переполнена — Master продолжает работу в fallback
- [x] [B] CI: `build-saturn.yml` зелёный; артефакт образа; PC/PS1 не регрессировали
- [x] [B] Документация: `docs/SATURN.md` (разделение работ, sync, ограничения), CHANGELOG

## Критерии приёмки спринта 25–26

- [x] Оба SH-2 реально используются; это подтверждено счётчиками/профайлером, а не только фактом запуска Slave
- [x] Параллельный workload даёт измеримый прирост на выбранной задаче; порог и методика зафиксированы в ADR-009
- [x] Повторный прогон детерминирован; race/deadlock не воспроизводятся в 10-минутном стрессе
- [x] При отказе Slave Master продолжает работать в single-CPU fallback без краша
- [x] VBlank-синхронизация не даёт tearing; PC/PS1 тесты зелёные

## Результаты (2026-11-10)

- **Транспорт:** `scu_sync.*` (acquire/release, cache-through, FRT-bounded
  wait) + `slave_worker.*` (SPSC ring 8, handshake 200 мс, кадровый wait
  20 мс, STOP/restart, FAILED/fallback). Overflows/timeouts/post failures —
  явные счётчики; бесконечных busy-wait нет.
- **Lifecycle:** Jo Engine `slave.c` стартует Slave; handshake PING; при
  сбое/зависании Master продолжает single-CPU fallback. `UNAVAILABLE`/
  `FAILED` не восстанавливаются без reboot; `STOPPED` → restart.
- **Workload:** 256 треугольников (128/CPU), `CUBE.MSH` с CD, clip-space
  transform fixed 16.16 (`dmulu.l`, без делений), FNV-hash; single/dual
  hash совпадает. Slave не пишет VRAM; завершение — до VBlank.
- **Метрики:** Mednafen 1.32.1, Japanese BIOS 1.01: single **23838 µs**,
  dual **16113 µs**, выигрыш **32%** (порог ADR-009 ≥15%), `h=1`;
  кадровый wait ≈4007 µs, hash кадра `af890f`, ~50–60 FPS.
- **Память:** Work RAM-L ledger: asset heap 932 + scratch 48 + command 16 +
  parallel 28 = 1024 KiB; static_assert + map (≈38 KiB до master stack).
  Терминология H/L исправлена (ADR-008 errata, ADR-009).
- **Fault injection:** `saturn-single` (`2=n/a`, fallback), `saturn-hang`
  (`slave failed: wait timeout`, Master жив), `saturn-flood`
  (`flood overflows=1`, worker READY); host-тесты 63/63.
- **Документация/CI:** ADR-009, `docs/SATURN.md` §11, benchmark YAML,
  presets `saturn-single/hang/flood`, `build-saturn.yml` собирает варианты.
- **Stress:** 10-минутный dual-CPU прогон — exit 124, без deadlock/hang;
  середина `f=12300c=12303w=4035h=af890f`, hash стабилен (`af890f`); см.
  `docs/benchmarks/saturn_reference_environment.yaml`,
  `screenshots/saturn_stress_s25_26.png`.

## Риски

- **Гонки и когерентность кэша.** Митигация: явные барьеры/flush, минимум shared-состояния, host-тесты очереди, стресс.
- **Дисбаланс Master/Slave.** Митигация: начать с одной workload, мерить, не усложнять до измеримого выигрыша.
- **Тайминги эмулятора.** Митигация: pinned-эмулятор, фиксированные настройки, отдельный baseline; не обещать real-hardware performance.
- **Сложность отладки.** Митигация: счётчики, debug-лог, fallback-режим, минимум параллельного кода в v0.2.0.
