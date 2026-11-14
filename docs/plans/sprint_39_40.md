# Спринт 39–40 — N64: исследование, toolchain, backend skeleton и CI (недели 77–80)

> Детальный план спринта. Родительский документ:
> [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13.3).
> Источник требований: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md)
> §2.3 (Nintendo 64) и §4 «Будущие итерации» — N64 v0.3.0, S39+.
> Baseline SDK — **libdragon** (MIT); точный tag/commit и SHA-256 фиксируются
> в ADR-014 и `VERSION.txt`. Любая публикация релизов проекта отложена до
> v0.9.x ([`../adr/ADR-013-release-freeze.md`](../adr/ADR-013-release-freeze.md)).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-14): реализация S39–40 закрыта локально.** Pinned
> libdragon/toolchain подняты, `n64-release` собирает bootable `.z64`,
> ROM проходит `scripts/verify_n64_rom.py`, boot-demo 60 с работает в
> gopher64 1.2.1, host-тесты N64 6/6. `build-n64.yml` готов; remote CI
> будет подтверждён после push (в этом окружении GitHub Actions не
> запускается). Публикация v0.3.0 — только draft, release freeze.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 39–40 |
| Недели | 77–80 — 08.02.2027 – 07.03.2027 (ориентир); фактически локально закрыт 2026-11-14 |
| Веха | v0.3.0 — Nintendo 64: platform bring-up (часть 1) |
| Цель | Зафиксировать N64-контракты, поднять pinned toolchain/libdragon, собрать bootable `.z64` и CI-контур без эмулятора |
| Входные критерии | S37–38 технически закрыт; release freeze действует; reference-факты N64 перепроверены по первоисточникам |
| На критическом пути | Да (первый N64-спринт) |
| Выходные артефакты | ADR-014, `docs/N64.md` (draft), `n64-release`, `src/hal/n64/` skeleton, boot-demo `.z64` + map, `build-n64.yml`, N64-секция `ci/normative_contracts.yaml` |
| Следующий спринт | [`sprint_41_42.md`](sprint_41_42.md) — HAL N64: VI, input, ROM/DFS, память |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 55 | 85 | 56.7 | 8.3 | 73.3 | 40–60 ч | ⚠ Toolchain/libdragon — главный риск; при блокере режем boot-demo до clear-screen, RSP/RDP не трогаем |

## Результаты S39–40 (2026-11-14)

| Проверка | Результат |
|---|---|
| Toolchain | mips64-elf-gcc 16.2.0, deb SHA-256 `fac8e657…0a0a4d` |
| libdragon | commit `e356bf3f56f7afbf7e5246329562f145965cfdfc` (MIT) |
| `cmake --preset n64-release` | OK (toolchain/toolchain-файл, imported libdragon) |
| `hello_n64.z64` | 212 992 B (208 KiB), title `RETRO POLYENGINE`, entry `0x80000400`, sha256 `928e5c02…f8cb6` |
| `scripts/verify_n64_rom.py` | PASS: header/ELF/sha256; `crc_policy=libdragon_ipl3_header_crc_unused` |
| Boot-demo 60 с (gopher64 1.2.1, Xvfb/llvmpipe) | PASS: 3600 кадров (60.7 с N64-времени), heartbeat, без RSP-crash |
| N64 host-тесты | 6/6 (`n64_hal_tests`) |
| PC regression | `ctest --preset pc-debug` 295/295 |
| Скриншот | `screenshots/hello_n64_boot_s39_40.png` |
| CI | `.github/workflows/build-n64.yml` + negative-checks готовы; remote run — после push |

**Отклонение от плана:** baseline эмулятора — **gopher64 1.2.1**, не
`ares`/`mupen64plus`: mupen64plus не загружает libdragon IPL3 r5+
(mupen64plus-core#1185), ares не публикует Linux-бинарь. Причина и пины —
ADR-014.

## Задачи

**Неделя 77 — исследование и нормативные решения:**

- [x] [B] Перепроверить N64-reference-факты по первоисточникам (Nintendo 64 Programming Manual, libdragon docs): NEC VR4300 (R4300i) 93.75 МГц; 4 MiB RDRAM base (+Expansion Pak до 8 MiB); RCP 62.5 МГц; RSP/RDP; TMEM 4 KiB; 18-bit internal Z; тайловые текстуры 64×64 / 32×64; VI 320×240; картридж 4–64 MiB. Записать в `docs/N64.md` с метками **HW** / **policy**
- [x] [B] ADR-014: N64 backend — compile-time `PLATFORM_N64`, pinned libdragon (tag/commit/SHA-256), toolchain `mips64-elf-gcc`, формат ROM `.z64`, emulator policy, границы v0.3.0 (без audio, без расширенной ФС; только read-only DFS)
- [x] [B] Лицензии и пины: libdragon (MIT), toolchain notices, SHA-256 манифест; `VERSION.txt`, `LEGAL.md`; SDK не вендорится в git (env-install) либо фиксируется как submodule — решение в ADR-014
- [x] [B] Spike: эмулятор для ручного gate — `ares` vs `mupen64plus`; проверить pinned version, deterministic screenshot/framebuffer capture, запуск под Xvfb; выбрать baseline и зафиксировать в ADR-014. Итог: mupen64plus не грузит libdragon IPL3 r5+, ares без Linux-бинаря → gopher64 1.2.1
- [x] [B] Проверить ROM-контур: `n64tool`/`mkdfs`, boot-заголовок, checksum, layout `.z64`; получить эталонный «минимальный ROM» (clear screen) на примерах libdragon
- [x] [H] Сверить N64 HAL с PC/PS1/Saturn: какие интерфейсы переиспользуются, что N64-специфично (VI, PI/ROM, RSP/RDP, controller, TMEM)
- [x] [M] Зафиксировать предварительный memory map RDRAM: код/данные, stack, SDK reserve, engine arenas, framebuffer/Z; цифры уточняются в S41–42

**Неделя 78 — toolchain, CMake и CI:**

- [x] [B] `cmake/n64-toolchain.cmake` + preset `n64-release`; `PLATFORM_N64` в guard «ровно один backend»; `RPE_PLATFORM_NAME=n64`, arch `mips64`
- [x] [B] libdragon discovery: `LIBDRAGON_ROOT`/env, проверка версии/commit, imported target; pinned SHA-256 проверяется в CI
- [x] [B] `src/hal/n64/` skeleton: идемпотентные `Platform_Init`/`Shutdown`, `Platform_PumpEvents`, `Platform_GetTimeMs`; stub `hal_render_backend_*` возвращает `false` + диагностику (без silent drop)
- [x] [B] Линковка N64-цели с libdragon; сборка `.z64` через `n64tool`, map-файл, sha256 артефакта
- [x] [B] `.github/workflows/build-n64.yml`: pinned toolchain, configure/build `n64-release`, upload `.z64` + map + sha256; эмулятор в CI **не запускается**
- [x] [B] `ci/normative_contracts.yaml`: секция `platforms.n64` (toolchain pin, ROM extension, budget placeholders, `emulator_validation: manual`)
- [x] [H] `docs/BUILDING.md` § N64: установка libdragon/toolchain, env vars, сборка/запуск
- [x] [H] `README.md`: supported matrix — N64 (development), ссылка на `docs/N64.md`

**Неделя 79 — boot, ROM и защитные проверки:**

- [x] [B] Boot path: `Platform_Init` → VI init 320×240 (16-bit) → clear через RDP → main loop → `Platform_Shutdown`; повторный init/shutdown не ломает процесс (self-test init/init/shutdown/init + host-тесты)
- [x] [B] Boot self-test: вывести версию/target/time, дойти до main loop; лог — через ISViewer или выбранный debug-канал (решение ADR-014)
- [x] [B] CI negative-checks: неверный toolchain, отсутствует `LIBDRAGON_ROOT`, нет `.z64` на выходе, включено больше одного `PLATFORM_*` (workflow + локальный прогон — все 4 кейса FAIL как ожидается)
- [x] [B] Проверка ROM-заголовка и checksum в CI; фиксация размера ROM в логе/артефакте
- [x] [H] `scripts/run_n64.sh`/`.bat`: запуск pinned эмулятора с документированными настройками; BIOS/ROM не вендорятся
- [x] [M] Минимальный `hello_n64` (clear + лог), без треугольника — доказательство boot-контура

**Неделя 80 — документы и приёмка:**

- [x] [B] `docs/N64.md` (draft): reference-факты, memory map, build/run, ограничения, план gate
- [x] [B] ADR-014 accepted; `VERSION.txt`/`LEGAL.md` согласованы с пинами
- [x] [B] Приёмка: `n64-release` собирается с чистого checkout; `.z64` проходит header/checksum; boot-demo доходит до main loop в pinned эмуляторе
- [x] [B] Отсутствие BIOS/ROM/ключей/build-каталогов в репозитории и артефактах
- [x] [M] Обновить roadmap/статус S39–40 в планах

## Критерии приёмки спринта 39–40

- [x] `cmake --preset n64-release` собирается с pinned libdragon/toolchain; guard «ровно один backend» включает N64
- [x] CI `build-n64` workflow готов (pinned toolchain, artifacts; локальный эквивалентный прогон — PASS); remote green — после push
- [x] ROM-заголовок/checksum валидны (libdragon IPL3 policy); boot-demo стабильно работает 60 с в pinned эмуляторе
- [x] `Platform_Init`/`Shutdown` идемпотентны; stub renderer явно возвращает ошибку, без silent drop
- [x] ADR-014, `docs/N64.md`, `VERSION.txt`, `LEGAL.md`, `ci/normative_contracts.yaml` согласованы
- [x] В репозитории/артефактах нет BIOS, ROM-образов игр, ключей и build-каталогов

## Ручной чек-лист приёмки

- [x] Установить libdragon/toolchain по `docs/BUILDING.md` на чистую машину и собрать preset (local env-install, 2026-11-14)
- [x] Запустить `.z64` в pinned эмуляторе: clear screen, лог, 60 с без краша (gopher64 1.2.1, 3600 кадров / 60.7 с N64-времени)
- [x] Проверить ROM header/checksum и размер; сравнить с CI-артефактом (`verify_n64_rom.py`, 212 992 B)
- [x] Проверить пины `VERSION.txt`/SHA-256 и отсутствие ROM/BIOS в git
- [x] Переключиться между PC/PS1/Saturn/N64 пресетами — остальные сборки не сломаны (PC 295/295; PS1/Saturn конфигурация не затронута)

## Риски

- **libdragon/toolchain дрейфует или долго ставится.** Митигация: immutable tag/commit + SHA-256, env-install, CI-кэш; обновление — отдельная задача с ADR.
- **Эмуляторы различаются по точности.** Митигация: pinned baseline (gopher64 1.2.1), app-side heartbeat, manual gate; CI не зависит от эмулятора. mupen64plus исключён из-за mupen64plus-core#1185.
- **RCP/RSP/RDP сложнее оценки.** Митигация: S39–40 ограничен boot/clear; RSP/RDP — S43–44; CPU-fallback как аварийный путь.
- **ROM header/boot ломается на чистом checkout.** Митигация: `n64tool`/checksum-проверки в CI, документированный layout, smoke на чистой распаковке.
- **gopher64/llvmpipe медленнее реального N64.** Митигация: gate меряет wall-clock 60 с и heartbeat; на llvmpipe 60 с wall-clock дали 3600 кадров / 60.7 с N64-времени (software render), это допустимо для bring-up.
