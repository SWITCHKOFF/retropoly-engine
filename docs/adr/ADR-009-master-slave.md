# ADR-009: Master + Slave SH-2 — разделение работ, синхронизация и fallback

- **Статус:** Accepted
- **Дата:** 2026-11-10
- **Спринт:** 25–26 (недели 49–52), веха v0.2.0
- **Связанные документы:** [`../plans/sprint_25_26.md`](../plans/sprint_25_26.md),
  [`../SATURN.md`](../SATURN.md), [`ADR-008`](ADR-008-saturn-backend.md),
  ADR-007 (память), Sega **Dual CPU User's Guide** ST-202-R1,
  Sega **SCU User's Manual** ST-097-R5, Hitachi **SH7604 Hardware Manual**.

## Контекст

Saturn содержит два одинаковых SH7604 (Master + Slave), общую внешнюю память
и два независимых 4 KiB кэша **без bus snooping**. SCU не предоставляет
hardware-семафоров: штатный механизм междерверной синхронизации — FRT input
capture (запись в `0x21000000` будит Slave, `0x21800000` — Master) и
cache-through доступ к общей памяти (`addr | 0x20000000`). Jo Engine v2026.2
уже содержит проверенный Slave loop (`slave.c`), очередь job'ов и wake-up;
RPE не дублирует этот слой, а строит поверх него собственный протокол.

S23–24 дал рабочий HAL на Master: lifecycle, 6-button pad, ISO9660, VDP2
clear/log. S25–26 должен задействовать второй CPU, не нарушив VBlank/VRAM
контракт и обеспечив детерминированный fallback.

## Решения

### 1. Ответственность Master и Slave

**Master:** lifecycle, ввод, CD-ФС, VDP1/VDP2, все запросы к периферии,
финальная композиция кадра, отправка VDP1-команд. **Slave:** только чистый
compute-job'ы над shared Work RAM (в S25–26 — clip-space подготовка вершин
для будущего command list). Slave **не пишет** в VDP1/VDP2/VRAM и не вызывает
SMPC/CD/FS. Разделение software-defined: VDP1 не «закреплён» за Slave
аппаратно.

### 2. Транспорт: fixed-size SPSC ring

Очередь (`saturn::slave_worker`) — SPSC ring ёмкостью 8 job'ов, без
динамических аллокаций. Master пишет `head` (release), Slave — `tail` и
`completed` (release); чтение — acquire. Индексы и job-поля находятся в
cache-through памяти. Переполнение — **явный `false`** +
`SLAVE_WORKER_QUEUE_FULL`, overflow-счётчик и fallback; ожидания без
таймаута запрещены.

### 3. Жизненный цикл и fallback

Jo Engine запускает Slave внутри `jo_core_init()` (при
`JO_COMPILE_WITH_DUAL_CPU_SUPPORT`). RPE выполняет handshake PING-job с
таймаутом 200 мс. Состояния: `READY`, `STOPPED` (логическая остановка
STOP-job'ом), `UNAVAILABLE` (сборка без dual-CPU или Slave не стартовал),
`FAILED` (timeout/post failure). Из `STOPPED` возможен restart с повторным
handshake; из `FAILED` — только перезагрузка. При любом отказе Master
продолжает кадры в single-CPU fallback.

### 4. Завершение job'а и кэш

RPE **не вызывает** `jo_core_wait_for_slave()`: у него нет таймаута, и
зависший Slave заблокировал бы Master. Завершение отслеживается по
shared `completed` + bounded wait (FRT). После успешного wait Master
инвалидирует свой кэш (`rpe_saturn_cache_purge()`). Slave получает свежие
данные после purge перед каждым job (Jo Engine) и работает с shared-полями
через cache-through alias.

### 5. Параллельный workload S25–26

Workload — детерминированная **clip-space подготовка** 256 треугольников
(128 на CPU): mesh-вершины `CUBE.MSH` (fallback — встроенный куб)
реплицируются в сетку инстансов, трансформируются `proj·view` в clip-space,
классифицируются near/far clip и хэшируются в 8-байтовую запись
`PreparedTriangle`. Perspective divide, screen-mapping и backface остаются
в VDP1-стадии S27–28 на Master.

- Split фиксирован по индексам: `[0..128)` — Master, `[128..256)` — Slave.
- Математика — только fixed 16.16; специализированный `transform_clip`
  использует 32×32→64 (`dmulu.l`) и один clamp на компоненту; делений в
  workload нет.
- Single-CPU baseline и dual-CPU обязаны дать одинаковый FNV-1a hash.
- Slave пишет во вторую половину; Master при timeout пересчитывает её в
  отдельный fallback-буфер, поэтому поздняя запись «зависшего» Slave не
  портит кадр.

### 6. VBlank/VRAM

Кадровый callback выполняется до `__jo_core_synch()`; Slave завершает job до
этой точки (или Master уходит в fallback). В S25–26 Slave вообще не касается
VDP. В S27–28 Slave сможет готовить command list, но **отправка** команд и
любая запись VRAM останутся на Master в разрешённом VBlank-окне. Tearing/race
не допускаются.

### 7. Метрики и порог приёмки

Счётчики: submitted/completed, queue overflows, post failures, timeouts,
fallback frames, wait µs (last/total/max), slave busy µs, slave jobs.
Boot-бенчмарк — 3 прогона single и dual на одном MVP; результат логируется
строкой `1=<µs> 2=<µs> g=<%> h=<0|1>`. Кадровые метрики —
`f=<frames>c=<completed>w=<wait µs>h=<hash24>`.

**Порог:** dual-CPU workload time ≤ 0.85 × single-CPU (≥15% выигрыша) при
`hashes_match` и `slave_jobs > 0` на reference-стенде
([`../benchmarks/saturn_reference_environment.yaml`](../benchmarks/saturn_reference_environment.yaml)).
Измерено в Mednafen 1.32.1: **23838 µs → 16113 µs, 32%**, hash совпал;
кадровый wait ≈ 4 мс, устойчивые ~50–60 FPS в S25–26 demo.

### 8. Детерминизм и fault injection

Детерминизм: фиксированный split, целочисленная fixed-математика, отсутствие
float/heap/времени в решении. Одинаковый вход → одинаковый hash; повторный
прогон в эмуляторе даёт тот же `af890f`-hash.

Fault injection:
- `RPE_SATURN_DUAL_CPU=OFF` — Slave отсутствует, Master в fallback;
- `RPE_SATURN_FAULT_MODE=hang` — следующий Slave-job зависает, Master ловит
  timeout 20 мс и продолжает fallback;
- `RPE_SATURN_FAULT_MODE=flood` — boot-проба переполняет очередь и логирует
  `flood overflows=1`, после чего worker остаётся READY;
- host-тесты покрывают unavailable, queue full, post failure, timeout,
  stop/restart и flood.

## Последствия

- Оба SH-2 реально используются; счётчики `c`/`slave_jobs` и speedup — часть
  gate, а не «факт запуска».
- Появился новый shared-регион 28 KiB (reserved) в Work RAM-L; asset heap уменьшен
  932 KiB, сумма регионов по-прежнему ровно 1 MiB (static_assert).
- Slave worker — внутренний HAL-модуль; публичный `platform.h` не меняется.
- `jo_core_wait_for_slave()` в RPE не используется; при обновлении Jo Engine
  проверить, что `slave.c`/FRT wake-up не изменились.
- Любое расширение workload на VDP1 (S27–28) требует сохранения правила:
  Slave не пишет VRAM; отправка команд — Master в VBlank-окне.

## Риски и митигации

| Риск | Митигация |
|---|---|
| Когерентность кэша | cache-through для всех shared-полей; purge после wait; host-тесты очереди; stress |
| Зависший Slave | bounded wait, FAILED-состояние, fallback; hang fault build |
| Дисбаланс CPU | boot-бенчмарк, wait/slave-busy метрики; workload 128/128 |
| Тайминги эмулятора | pinned Mednafen 1.32.1 + reference YAML; порог только внутри снапшота |
| Сложность отладки | VDP2 metrics line, host fault injection, отдельные single/hang/flood сборки |

## Ссылки

- Sega, **Dual CPU User's Guide**, ST-202-R1-120994 (FRT input capture,
  cache-through, fully shared mode).
- Sega, **SCU User's Manual**, ST-097-R5-072694 (SCU registers; семафоров нет).
- Hitachi, **SH7604 Hardware Manual** (кэш, CCR, FRT, DIVU).
- Jo Engine v2026.2 `slave.c` (pinned `5e99944`): очередь, wake-up, cache purge.
- [`../SATURN.md`](../SATURN.md) §11 — карта shared-памяти, метрики и ручной чек-лист.
