# Changelog

Все заметные изменения этого проекта документируются в этом файле в стиле [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/); версионирование — [SemVer](https://semver.org/lang/ru/). Правило: обновление CHANGELOG — на каждом sprint review (docs/DEVELOPMENT_PLAN.md §1, п. 2).

## [Unreleased]

### Added

- Спринт 0 (M0: Инфраструктура): каркас репозитория, канонические CMake-пресеты (`pc-debug`, `pc-release`, `pc-debug-render`, `ps1-release`), вендоринг зависимостей, первый CI-контур.
- Консолидация документации в `docs/` (инженерная спецификация v1.43 + план разработки + планы спринтов).
- Спринт 1–2, Неделя 1 (M1): `docs/STYLE_GUIDE.md`, `.clang-format` (4 пробела, ≤100 симв., 100 колонок), шаблон PR с чек-листом DoD (§17).
- Спринт 1–2, Неделя 2 (M1): ядро fixed-point 16.16 — `src/core/math/fixed.h/.cpp` (`fixed_clamp_i64/from_int/from_float/to_int/to_float/add/sub/mul/div`, `fixed_sqrt` — детерминированный integer isqrt); юнит-тесты `tests/math/fixed_test.cpp` (19 кейсов, 100% pass rate, debug+release); GoogleTest подключён канонически (`tests/CMakeLists.txt`), корневой `enable_testing()` (CTest-агрегация), заглушка `core/placeholder.cpp` удалена.
- Спринт 1–2, Неделя 3 (M1): `src/core/math/vec2.h/vec3.h/vec4.h` (make/add/sub/scale/dot; `vec3_cross`, `vec3_length_sq`; `fixed_reduce3_products` — полный int64-аккумулятор, clamp ровно один раз; насыщающий 64-битный аккумулятор `fixed_sat_add_i64` против UB-переполнения редукций). Non-inline `vec3_length`/`vec3_normalize` — `src/core/math/math.cpp`. Тесты `tests/math/{vec2,vec3,vec4}_test.cpp` (ReductionUsesSingleFinalClamp на экстремальных raw-векторах, Cross на ортах, Determinism).
- Спринт 1–2, Неделя 4 (M1): Mat3/Mat4 (column-major, §4.3) — identity/multiply (aliasing-safe, int64-аккумуляторы)/translate/rotate_x/y-z/perspective/look_at/transform; Mat3 (rotation-подмножество для GTE); детерминированная тригонометрия `sin_fixed`/`cos_fixed` (полином Тейлора после целочисленного приведения, без float — freestanding PS1-совместим). Тесты `tests/math/mat4_test.cpp` (оси 90°, NDC ±1, look_at, побитные golden-векторы) — итого 49 юнит-тестов, 100% pass (debug+release). Бенчмарк-заготовка `tests/benchmark/math_benchmark.cpp` (включается после вендоринга google/benchmark, пресет pc-bench).
