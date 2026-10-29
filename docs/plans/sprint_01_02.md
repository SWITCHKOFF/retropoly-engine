# Спринт 1–2 — Структура репозитория + математическое ядро (недели 1–4)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§4).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-10-29, Linux-среда):** Недели 1–4 выполнены на 49 юнит-тестах (pc-debug и pc-release — 49/49 зелёных; ps1-release — сборка ELF зелёная). Недоделано только CI-гейты, требующие GitHub-remote: GitHub Actions и cross-platform матрица.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 1–2 |
| Недели | 1–4 — 09.11.2026 – 06.12.2026 |
| Веха | M1 — Ядро (часть 1: математика) |
| Цель | Fixed-point математика с unit-тестами, работающая детерминированно |
| Входные критерии | Спринт 0 завершён (гейт `plans/sprint_00.md`) |
| На критическом пути | Да |
| Выходные артефакты | `fixed.h/.cpp`, `vec2.h/vec3.h/vec4.h`, `mat3.h/mat4.h`, `math.cpp`, `tests/math/*`, `docs/STYLE_GUIDE.md` |
| Следующий спринт | [`sprint_03_04.md`](sprint_03_04.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 30 | 45 | 70 | 46.7 | 6.7 | 60.0 | 40–60 ч | Укладывается; при 10 ч/нед близко к нижней границе |

## Задачи

**Неделя 1 — закрепление инфраструктуры:**
- [x] [B] Сверить структуру каталогов с §10 ЭТАПА 4 (отчёт/комментарий в PR) *(сверено 2026-10-29: все каталоги/каркас по списку §10 присутствуют; platform.h и заглушки файлов слоёв не входят в объём Спринта 0 — создаются предметными спринтами 3–4, 5–6)*
- [x] [B] Завести `docs/STYLE_GUIDE.md` (PascalCase классы, snake_case функции, m_ для полей, UPPER_SNAKE константы; 4 пробела; ≤100 символов; `auto` запрещён в публичном API; `using namespace` запрещён в заголовках; new/delete и printf запрещены в runtime) *(создан; исключение — normative POD-слои ядра Vec/Mat с контрактным layout)*
- [x] [B] Включить `.clang-format` под правила стайлгайда *(Google-based, IndentWidth 4, ColumnLimit 100; бинарь clang-format на машине отсутствует — прогон форматтера перенесён на CI-трек после GitHub-remote)*
- [x] [H] Шаблон PR: описание, чек-лист DoD (см. §17 Приложение В) *(.github/pull_request_template.md)*

**Неделя 2 — fixed_t (16.16):**
- [x] [B] Создать `src/core/math/fixed.h`:
  - [x] `typedef int32_t fixed_t;` + `FIXED_BITS=16`, `FIXED_ONE = 1<<16`
  - [x] `fixed_clamp_i64(int64_t)` (saturating clamp) *(плюс `fixed_sat_add_i64` — насыщающий аккумулятор редукций, защита от UB int64-переполнения)*
  - [x] `fixed_from_int` (умножение на FIXED_ONE через int64_t + clamp)
  - [x] `fixed_from_float` (isfinite→0 иначе; truncation toward zero; clamp к INT32_MIN/MAX) *(isfinite — без <cmath>: freestanding PS1; `fixed_float_isfinite` через IEEE-семантику сравнений)*
  - [x] `fixed_to_int`, `fixed_to_float`
  - [x] `fixed_add/fixed_sub` (int64_t промежуточные, saturating)
  - [x] `fixed_mul` (int64_t intermediate, масштаб 2^16, trunc toward zero, saturating)
  - [x] `fixed_div` (int64_t; деление на 0: debug assert, release → INT32_MAX/INT32_MIN по знаку)
  - [x] Объявить `fixed_sqrt` (реализация в .cpp: отрицательный вход — invalid: debug assert, release 0; целочисленный детерминированный алгоритм с bounded iteration, например Newton) *(реализован побитовый integer longhand-isqrt: ровно 24 шага, без делений/float; Newton заменён как эквивалентно-ограниченный)*
- [x] [B] Doxygen-комментарии для всех публичных функций (@param/@return, note про saturating)
- [x] [B] Тесты `tests/math/fixed_test.cpp` (GTest):
  - [x] Conversion: `fixed_from_int(0/±1/100)`, round-trip to_int, float-конверсии с погрешностью < 0.001
  - [x] Add/Sub/Mul/Div точные значения (10+3, 30−10, 5×7, 100/10, дробные 2.5×4.0, 10.0/3.0)
  - [x] **SaturatingArithmetic:** `fixed_add(INT32_MAX,1)==INT32_MAX`; `fixed_sub(INT32_MIN,1)==INT32_MIN`
  - [x] **MultiplyUsesQ16_16Scale:** `fixed_mul(from_int(5),from_int(7))==from_int(35)`
  - [x] **DivisionByZero:** debug `EXPECT_DEATH`; release: `INT32_MAX` для a≥0, `INT32_MIN` для a<0
  - [x] **AccuracyStressTest:** mt19937(42), 10 000 итераций, raw-значения ±6·FIXED_ONE, эталон в double; max error < 2.0/FIXED_ONE (mul и div)
  - [x] **Determinism:** одинаковый вход → идентичный raw-выход; сравнение с независимым ожиданием (123×456 → INT32_MAX: 123·456 = 56088 > 32767.9999 — насыщение, ожидание посчитано вручную)
  - [x] Sqrt: `sqrt(25)==5`, `sqrt(2)≈1.414±0.01`, `sqrt(отрицательного)` → контракт
- [x] [B] `tests/CMakeLists.txt`: `add_subdirectory(googletest EXCLUDE_FROM_ALL)`, `add_executable(retro_engine_tests ...)`, link `retro_engine` + `GTest::gtest_main`, `gtest_discover_tests` *(плюс корневой `enable_testing()` — CTest-агрегация; заглушка placeholder.cpp удалена)*
- [x] [B] `ctest --preset pc-debug` зелёный; push; CI зелёный *(ctest: 49/49 зелёных локально; push/CI — блокировано отсутствием GitHub-remote)*

**Неделя 3 — Vec2/Vec3/Vec4:**
- [x] [B] `src/core/math/vec2.h`, `vec3.h`, `vec4.h`:
  - [x] Структуры {x,y,z} из fixed_t
  - [x] `vec3_make/add/sub/scale`
  - [x] **`fixed_reduce3_products`**: полный int64_t аккумулятор для всей редукции, clamp **ровно один раз** в конце *(плюс насыщающий chain для запретных по UB значений аккумулятора)*
  - [x] `vec3_dot` (через reduce3), `vec3_cross` (int64_t промежуточные по компонентам)
  - [x] `vec3_length_sq`, `vec3_length`, `vec3_normalize` (деление нуля — контракт fixed_div) *(length/normalize — math.cpp, normative non-inline)*
- [x] [B] Тесты `tests/math/vec3_test.cpp`:
  - [x] Операции на целых значениях (1,2,3)·(4,5,6): dot = from_int(32)
  - [x] **ReductionUsesSingleFinalClamp:** `vec3_dot(vec3(INT32_MAX,INT32_MAX,INT32_MAX), vec3(ONE,ONE,ONE)) == INT32_MAX` (сравнение raw fixed_t, не только float) *(плюс отрицательный аналог INT32_MIN)*
  - [x] Cross на ортах (e1×e2=e3)
  - [x] Normalize: unit-length, длина²≈ONE
- [x] [H] Vec2/Vec4 — минимальный набор (make/add/sub/scale/dot) + тесты
- [x] [B] CHANGELOG: запись в [Unreleased]

**Неделя 4 — Mat3/Mat4 + CI:**
- [x] [B] `src/core/math/mat4.h` (column-major, как OpenGL):
  - [x] `mat4_identity`, `mat4_multiply` (int64_t аккумуляторы по элементам) *(aliasing-safe; насыщающий int64-аккумулятор)*
  - [x] `mat4_translate`, `mat4_rotate_x/y/z` (тригонометрия через fixed-таблицы: реализовать `core/math/sin_fixed/cos_fixed` с табличным/полиномным детерминированным алгоритмом) *(реализовано полиномом Тейлора (4 члена) после целочисленного приведения к [0, π/2]; без float и без таблиц — детерминировано, погрешность ≤ ~1e-4 abs)*
  - [x] `mat4_perspective(fov,aspect,near,far)`, `mat4_look_at(eye,target,up)` *(имена параметров near_z/far_z — MSVC-совместимость с Win16-макросами near/far)*
  - [x] `mat4_transform(m, Vec4) → Vec4`
  - [x] Mat3 (rotation-подмножество) для конверсии в GTE-представление *(identity, from_mat4, multiply, transform)*
- [x] [B] Тесты `tests/math/mat4_test.cpp`:
  - [x] Identity нейтрален; перемножение ассоциативно на фиксированных векторах *(+ aliasing-safety: out == a/b)*
  - [x] translate/rotate на известных углах (90° → поворот осей без накопления float-ошибки)
  - [x] perspective: точка near/far проектируется корректно *(NDC z = ∓1 через clip/w; fov = π/2 → m[5] = 1)*
  - [x] look_at: глаз-камера смотрит в target
  - [x] **Determinism:** raw-матрицы побитно равны между запусками (зафиксировать golden-векторы) *(golden G1/G2/T захвачены фактическим прогоном канонической сборки и зашиты в тест)*
- [x] [B] `.github/workflows/tests.yml` уже существует — убедиться, что триггеры push/PR на main/master/develop, добить матрицу позже *(проверено: триггеры push/PR на [main, develop] присутствуют; матрица — после GitHub-remote)*
- [x] [M] Бенчмарк-заготовка: `tests/benchmark/math_benchmark.cpp` (google/benchmark): BM_FixedMul, BM_Vec3Normalize, BM_Mat4Multiply (`benchmark::DoNotOptimize`); включается пресетом `pc-bench` *(подключение — гард: собирается только при вендоренном google/benchmark; submodule-add с pin — отдельная задача §3.6)*
- [x] [H] Критерии спринта: все тесты зелёные на PC (Windows/Linux/macOS в CI позже; сейчас локально + ubuntu) *(GCC 15.2 с -Wall -Wextra -Wpedantic -Werror; PS1-тулчейн GCC 12.3 freestanding — ps1-release постройка зелёная; GitHub/Actions — blocked (нет remote))*

## Критерии приёмки спринта 1–2

- [x] Все unit-тесты проходят (100% pass rate) *(49/49 pc-debug; 49/49 pc-release)*
- [ ] CI запускает тесты автоматически на push *(блокировано: нет GitHub-remote; workflow и триггеры готовы)*
- [x] Точность по операциям подтверждена детерминированными векторами (не «запустил дважды на одной машине») *(golden raw-матрицы/векторы + mt19937(42)-стресс с double-эталоном)*
- [x] Doxygen на всех публичных функциях; код отформатирован *(clang-format в среде отсутствует — правила соблюдены по STYLE_GUIDE вручную; прогон форматтера перенесён в CI-трек)*

## Риски

Погрешность 16.16 высокой ⇒ (редко) пересмотреть отдельные операции на 32.32 с отдельным типом; недетерминизм ⇒ CI-тесты raw-значений.
