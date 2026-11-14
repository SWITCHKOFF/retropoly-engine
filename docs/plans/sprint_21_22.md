# Спринт 21–22 — Sega Saturn: изучение платформы и toolchain (недели 41–44)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13).
> Источник задач: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md) §4 «Будущие итерации» — Saturn, S21–32.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-10):** S21–22 завершён. Research/toolchain/skeleton/
> CI/docs выполнены; `saturn-release` собирается с чистого checkout +
> pinned Jo Engine v2026.2; добавлен fallback `tools/saturn_mkiso.py` для
> систем без `genisoimage`. PC-debug: 177/177 passed; PC-release и
> `ps1-release` собираются. Ручной 60-секундный smoke в Mednafen 1.32.1 с
> Japanese BIOS 1.01 пройден: VDP2 backdrop виден, краша нет; скриншот —
> `screenshots/saturn_backdrop.png`. GitHub Actions `build-saturn.yml`
> зелёный (run 42794098944); `build-pc`, `build-ps1`, `tests` — success.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 21–22 |
| Недели | 41–44 — 16.08.2027 – 12.09.2027 |
| Веха | v0.2.0 — Sega Saturn (итерация 3) |
| Цель | Проверить reference-факты Saturn, выбрать и запинить toolchain/эмулятор, добавить `PLATFORM_SATURN` и `saturn-release`, получить собираемый и загружаемый skeleton |
| Входные критерии | v0.1.0 выпущен ([`sprint_19_20.md`](sprint_19_20.md)); PC/PS1 CI зелёный; PS1 HAL — рабочий шаблон для нового backend |
| На критическом пути | Да (первый шаг v0.2.0) |
| Выходные артефакты | ADR-008 (Saturn/toolchain/разрешение), pinned Jo Engine + sh-elf-gcc, `saturn-release` preset, `src/hal/saturn/` skeleton, `build-saturn.yml`, `docs/SATURN.md`, обновлённые `BUILDING/LEGAL/VERSION/CHANGELOG` |
| Следующий спринт | [`sprint_23_24.md`](sprint_23_24.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 50 | 75 | 51.7 | 6.7 | 65.0 | 40–60 ч | ⚠ P-сценарий выше номинала: research/toolchain — главный риск; заложить recovery-неделю после S22 |

## Задачи

**Неделя 41 — Изучение платформы и решения:**

- [x] [B] Собрать первоисточники по Saturn: SH-2 (Master/Slave), SCU, VDP1, VDP2, карта памяти, CD-ROM, SMPC/геймпад; зафиксировать ссылки в `docs/SATURN.md`
- [x] [B] Перепроверить reference-факты §2.2 инженерной документации по первоисточникам: 2×SH-2 @ 28.6 МГц, 2 МБ Work RAM (Low + High), 512 КБ sound RAM, точный split VDP1/VDP2 VRAM, 320×224, отсутствие Z-буфера, quad-oriented VDP1
- [x] [B] Зафиксировать target profile: NTSC 320×224, 60 Гц, CD-ROM, 6-button pad; определить policy для общего reference 320×240 (native 320×224 / letterbox / crop) — не менять общий контракт без ADR
- [x] [B] Выбрать toolchain: Jo Engine (MIT) + sh-elf-gcc; проверить лицензии, воспроизводимость сборки с чистого checkout, наличие linker script и примеров
- [x] [B] Выбрать pinned Saturn-эмулятор (кандидаты: Mednafen/Beetle Saturn, Kronos), зафиксировать версию/commit и настройки; BIOS не вендорить и не публиковать в CI
- [x] [B] ADR-008: стратегия Saturn backend — compile-time HAL, toolchain, эмулятор, разрешение, высокоуровневая политика triangle→quad, границы v0.2.0

**Неделя 42 — Сборочный skeleton:**

- [x] [B] `CMakePresets.json`: preset `saturn-release`, опция `PLATFORM_SATURN`, guard «ровно один PLATFORM_*»
- [x] [B] `cmake/saturn-toolchain.cmake` (или штатная интеграция Jo Engine): компилятор, флаги, linker script, библиотеки, путь к SDK
- [x] [B] `src/hal/saturn/CMakeLists.txt`, `platform_saturn.cpp`, `main_saturn.cpp`: минимальный lifecycle и заглушки HAL-функций; сборка без изменения PC/PS1 целей
- [x] [B] Цель `saturn` + bootable-образ (ISO/CCD по возможностям toolchain); артефакт `saturn-release`
- [x] [B] `.github/workflows/build-saturn.yml`: pinned toolchain + sha256, configure/build `saturn-release`, upload artifact; без BIOS/ROM в репозитории
- [x] [B] Записать pin toolchain в `VERSION.txt`, обновить `ci/verify_vendored.sh` (если добавляется vendored-зависимость)

**Неделя 43 — Контракты, память, ассеты:**

- [x] [B] Аудит `src/hal/platform.h`: добавить `PLATFORM_SATURN`-ветки, не протаскивая SH-2/Jo Engine в публичный заголовок; PC/PS1 API не ломать
- [x] [B] Бюджет памяти: 2 МБ Work RAM (WRAM-L + WRAM-H), sound RAM 512 КБ, уточнённый VDP1/VDP2 VRAM; static_asserts и ledger; runtime `new/malloc` запрещён
- [x] [B] Совместимость ассетов: `.mesh` v1 (triangle list) и `.tex` v1 (CI8 + RGB555) → VDP1 CLUT/8bpp; если нужен отдельный Saturn-формат — только через ADR, v1 не ломать
- [x] [B] Контракт ввода: маппинг 6-button Saturn pad на `INPUT_PAD_*`; расширение enum — только перед `INPUT_PAD_COUNT`; задокументировать раскладку
- [x] [B] Контракт ФС: read-only ISO9660, имена 8.3, без host-путей; определить поведение при отсутствии/повреждении файла

**Неделя 44 — Smoke-тест, документация, гейт:**

- [x] [B] Минимальный boot: инициализация VDP2 и однотонный backdrop; 60 с без краша в pinned-эмуляторе; скриншот
  — **выполнено 2026-11-10:** Mednafen 1.32.1 + Japanese BIOS 1.01; backdrop виден, `timeout 60` без краша, скриншот `screenshots/saturn_backdrop.png`.
- [x] [B] Проверить, что `pc-debug`, `pc-release`, `pc-debug-render`, `ps1-release` не регрессировали (PC: 177/177 passed; `ps1-release` собран с PSn00bSDK v0.24: PS-EXE 110 592 Б, ISO 533 904 Б)
- [x] [B] Документация: `docs/SATURN.md`, `docs/BUILDING.md` (toolchain/preset/эмулятор/fallback ISO), `docs/LEGAL.md` (Jo Engine/MIT и toolchain), `VERSION.txt`, `CHANGELOG.md` [Unreleased]
- [x] [B] Гейт выхода: чистый checkout собирает `saturn-release`; CI зелёный; skeleton загружается в эмуляторе; ADR-008 принят
  — **выполнено:** чистый checkout/preset/ADR-008, сборка + структурные проверки, smoke в Mednafen; `build-saturn.yml` зелёный на GitHub runner (run 42794098944); PC/PS1 CI — success.

## Критерии приёмки спринта 21–22

- [x] `saturn-release` собирается с чистого checkout с pinned toolchain и без ручных шагов
  (локально проверено: полный `build/saturn-release` с `JO_ENGINE_ROOT`; добавлен fallback `tools/saturn_mkiso.py` для систем без `genisoimage`)
- [x] Saturn-бинарник загружается в pinned-эмуляторе, backdrop виден, 60 с без краша
  — **выполнено:** Mednafen 1.32.1 + Japanese BIOS 1.01, `timeout 60` без краша; скриншот `screenshots/saturn_backdrop.png`; чек-лист `docs/SATURN.md` §9.
- [x] CI `build-saturn.yml` зелёный; PC/PS1 сборки и тесты не затронуты
  — `build-saturn` success (run 42794098944); `build-pc`, `build-ps1`, `tests` — success; локально PC-debug 177/177, PC-release/pc-debug-render собираются.
- [x] ADR-008 фиксирует toolchain, эмулятор, разрешение, бюджет памяти и высокоуровневую политику triangle→quad
- [x] `docs/SATURN.md` содержит проверенные первоисточники и явно отделяет reference-факты от нормативных контрактов

## Риски

- **Зрелость Jo Engine / sh-elf-gcc.** Митигация: pin, сборка минимального SDK-примера, fallback на собственный toolchain file; не тянуть несколько toolchain одновременно.
- **Выбор эмулятора и точность.** Митигация: зафиксировать сборку/настройки, ручной чек-лист, не делать CI-гейт зависимым от BIOS.
- **Расхождения reference-данных.** Митигация: сверять по первоисточникам, фиксировать расхождения в `docs/SATURN.md`, не превращать справочные цифры в acceptance-контракт без проверки.
- **Расползание scope.** Митигация: в S21–22 только boot/backdrop, без 3D; VDP1/VDP2 — отдельные спринты.
