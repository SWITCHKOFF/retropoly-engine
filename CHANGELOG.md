# Changelog

Все заметные изменения этого проекта документируются в этом файле в стиле [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/); версионирование — [SemVer](https://semver.org/lang/ru/). Правило: обновление CHANGELOG — на каждом sprint review (docs/DEVELOPMENT_PLAN.md §1, п. 2).

## [Unreleased]

### Added

- Спринт 0 (M0: Инфраструктура): каркас репозитория, канонические CMake-пресеты (`pc-debug`, `pc-release`, `pc-debug-render`, `ps1-release`), вендоринг зависимостей, первый CI-контур.
- Консолидация документации в `docs/` (инженерная спецификация v1.43 + план разработки + планы спринтов).
- Спринт 1–2, Неделя 1 (M1): `docs/STYLE_GUIDE.md`, `.clang-format` (4 пробела, ≤100 симв., 100 колонок), шаблон PR с чек-листом DoD (§17).
- Спринт 1–2, Неделя 2 (M1): ядро fixed-point 16.16 — `src/core/math/fixed.h/.cpp` (`fixed_clamp_i64/from_int/from_float/to_int/to_float/add/sub/mul/div`, `fixed_sqrt` — детерминированный integer isqrt); юнит-тесты `tests/math/fixed_test.cpp` (19 кейсов, 100% pass rate, debug+release); GoogleTest подключён канонически (`tests/CMakeLists.txt`), корневой `enable_testing()` (CTest-агрегация), заглушка `core/placeholder.cpp` удалена.
