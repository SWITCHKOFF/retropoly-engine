# Спринт 41–42 — HAL N64: VI, input, ROM/DFS и память (недели 81–84)

> Детальный план спринта. Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.3).
> Источник требований: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md)
> §2.3 (N64), §4 (итерация 4). Продолжение [`sprint_39_40.md`](sprint_39_40.md).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-14): реализация S41–42 закрыта локально.** VI
> 320×240, joypad, время, read-only DFS с `CUBE.MSH`/`TEX0.TEX`, memory
> ledger 4 MiB, render skeleton + bounded RDP arena, 46 host-тестов
> (+ASan), 60-с gate и 10-минутный stress в gopher64 1.2.1. RSP/RDP —
> S43–44. Публикация релизов — только после v0.9.x (ADR-013).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 41–42 |
| Недели | 81–84 — 08.03.2027 – 04.04.2027 (ориентир) |
| Веха | v0.3.0 — Nintendo 64: platform bring-up (часть 2) |
| Цель | Полноценный HAL N64: VI 320×240, controller, время, read-only DFS/ROM-ассеты, статические арены и защитные тесты |
| Входные критерии | S39–40: `n64-release`, libdragon pinned, boot-demo, ADR-014 |
| На критическом пути | Да |
| Выходные артефакты | `src/hal/n64/` (VI/input/time/FS/memory), `n64_hal_tests`, DFS-ассеты в ROM, `build-n64.yml` variants, `docs/N64.md` (memory/FS/input), скриншот boot |
| Следующий спринт | [`sprint_43_44.md`](sprint_43_44.md) — RSP/RDP renderer |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 40 | 60 | 90 | 61.7 | 8.3 | 78.3 | 40–60 ч | ⚠ Выше номинала: VI+FS+память; при просадке переносим Expansion Pak/профайлер в S43–44, DFS-ассеты не режем |

## Результаты S41–42 (2026-11-14)

| Проверка | Результат |
|---|---|
| VI | 320×240, 16-bit, NTSC 60 Hz, 2 buffers; init/init/shutdown/init PASS |
| Input | `joypad_poll` ровно 1/кадр; 14 кнопок + 6 осей; deadzone 8; Y инвертирован |
| Время | `get_ticks_ms()` uint32, wrap-safe; 10-минутный прогон без переполнения |
| DFS | `mkdfs` детерминирован; `CUBE.MSH` 600 B, `TEX0.TEX` 66 068 B, `TRIANGLE.MSH` 105 B |
| ROM | `hello_n64.z64` 327 680 B (320 KiB), verifier PASS, DFS извлечён и сверён |
| Memory | arenas 2 MiB + stack 64 KiB + fb 300 KiB + reserve 1 724 416 B = 4 194 304 B |
| Heap | `scripts/verify_n64_no_heap.py` PASS; ASan `n64_hal_tests` без findings |
| Host-тесты | `n64_hal_tests` 46/46; PC regression 335/335 |
| Эмулятор | gopher64 1.2.1: 60-с gate (3600 кадров / 60.8 с), 10-мин stress (602 391 мс / 36 000 кадров), input-лог `[n64_input] pad0 press …` |
| Скриншот | `screenshots/n64_boot_s41_42.png` (clear + DFS/memory/pad overlay) |
| CI | `build-n64.yml`: ROM/DFS/heap structural checks; remote push run `43601517181` и PR run `43601522296` — **success** |

## Задачи

**Неделя 81 — платформа, VI, ввод и время:**

- [x] [B] `Platform_Init`: VI 320×240, 16-bit color, NTSC 60 Hz; framebuffer в RDRAM; `RENDER_FB_WIDTH/HEIGHT` = 320×240 для N64
- [x] [B] `Platform_Shutdown`: остановка VI/вывод, идемпотентность, безопасный повторный init
- [x] [B] `Platform_PumpEvents` + libdragon `joypad`: ровно один опрос за кадр; маппинг N64 A/B/Z/Start/D-pad/C-buttons/L/R на `INPUT_*`; deadzone
- [x] [B] `Platform_GetTimeMs`: монотонный счётчик от VI/CPU; без переполнения на 10-минутном прогоне
- [x] [B] Host-тесты `n64_hal_tests`: lifecycle, input mapping, время, отсутствие контроллера (N64-код за `PLATFORM_N64`, на PC — стабы)
- [x] [H] `Log_Message` → ISViewer/debug-канал; rate limit, чтобы не заливать канал
- [x] [M] Обработка reset/повторного запуска и «контроллер не подключён» без краша

**Неделя 82 — RDRAM, ROM/DFS и загрузка ассетов:**

- [x] [B] Memory ledger RDRAM 4 MiB: SDK/boot reserve, код/данные, stack ≤ 64 KiB, asset arena ≈ 1.5 MiB, frame arena ≈ 256 KiB, command arena ≈ 128 KiB; `static_assert`/linker ASSERT
- [x] [B] Запрет heap: `new`/`delete`/`malloc`/`free` вне libdragon boot не используются; проверка в host-тестах/линковке
- [x] [B] Read-only DFS: `mkdfs` + `dfs`/`romfs` (решение ADR-014), имена в стиле существующих ассетов, проверка размера и точное чтение
- [x] [B] `Platform_FileRead`/FS API: open/read/close, коды ошибок, отсутствие утечек при malformed/truncated файлах
- [x] [B] Загрузка `.mesh`/`.tex` v1 из DFS через `mesh_validate_and_load`/`texture_validate_and_load`; арена с rollback при ошибке
- [x] [B] ROM packaging: ассеты в DFS-образе, `n64tool` собирает `.z64`; детерминированный `mkdfs`
- [x] [H] Expansion Pak: определить наличие 4/8 MiB, но v0.3.0 budget/гейт — только 4 MiB base; 8 MiB не обязателен
- [x] [M] MemoryProfiler: current/peak по аренам, вывод в debug overlay

**Неделя 83 — render skeleton, command buffer и CI:**

- [x] [B] `hal_render_backend_*` N64 skeleton: 320×240, clear color, BeginFrame/EndFrame; draw/texture до S43–44 возвращают `false` + диагностику
- [x] [B] `src/hal/n64/rdp_command.*`: bounded command/display-list buffer; переполнение — явный `false`, без silent drop
- [x] [B] Host-тесты: переполнение command buffer/арены, malformed FS, input mapping, lifecycle violation, double shutdown
- [x] [B] `build-n64.yml`: debug/release варианты (если применимо), ROM с DFS-ассетами, artifacts; structural checks
- [x] [B] `ci/normative_contracts.yaml`: N64 memory/FS/ROM asserts и `emulator_validation: manual`
- [x] [H] 10-минутный stress boot/asset-load loop в pinned эмуляторе (ручной, non-gating)
- [x] [M] Скриншот `screenshots/n64_boot_s41_42.png` (clear + overlay)

**Неделя 84 — документация и приёмка:**

- [x] [B] `docs/N64.md`: memory map/ledger, DFS layout, input mapping, build/run, ограничения 4 MiB
- [x] [B] Addendum к ADR-014, если менялись VI mode/FS/emulator/budget
- [x] [B] Приёмка: boot + чтение `CUBE.MSH`/`TEX0.TEX` из DFS + clear + input-лог в pinned эмуляторе
- [x] [B] Host-тесты 100% зелёные; ASan-прогон `n64_hal_tests` без findings
- [x] [M] Обновить roadmap/статус S41–42

## Критерии приёмки спринта 41–42

- [x] VI 320×240/16-bit/60 Hz инициализируется; `Platform_Init`/`Shutdown` идемпотентны
- [x] Controller опрашивается один раз за кадр; все кнопки маппятся детерминированно
- [x] `.mesh`/`.tex` читаются из read-only DFS; malformed/truncated отвергаются без утечки арены
- [x] Peak RDRAM укладывается в 4 MiB base; heap не используется; ledger/static_assert зелёные
- [x] Render skeleton и command buffer при переполнении дают явную ошибку + диагностику
- [x] `n64_hal_tests` и `build-n64` зелёные; 10-минутный stress PASS (ручной)

## Ручной чек-лист приёмки

- [x] Запустить ROM в pinned эмуляторе: clear, overlay, контроллер, 60 с без краша
- [x] Проверить наличие `CUBE.MSH`/`TEX0.TEX` в DFS и их загрузку (лог/размеры)
- [x] Повторно запустить ROM/сбросить эмулятор — повторный init не ломает вывод
- [x] Проверить memory overlay: арены, peak, отсутствие heap-аллокаций
- [x] Прогнать malformed-ассеты (обрезанный `.tex`, битый `.mesh`) — явная ошибка, без краша
- [x] 10-минутный stress без краша/деградации

## Риски

- **VI/framebuffer съедают RDRAM.** Митигация: 16-bit, 320×240, минимум буферов; ledger и раннее решение по double-buffer.
- **DFS/имена/размеры ассетов не совпадают с PS1/Saturn.** Митигация: общий `.mesh`/`.tex` v1, uppercase-имена, проверка размера до чтения.
- **libdragon API для FS/VI меняется.** Митигация: adapter layer в `src/hal/n64/`, pinned commit, host-тесты на абстракции.
- **Input-маппинг N64 неочевиден.** Митигация: таблица в `docs/N64.md`, unit-тесты на все кнопки, документированный deadzone.
