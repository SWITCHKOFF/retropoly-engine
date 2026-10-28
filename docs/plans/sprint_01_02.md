# Спринт 1–2 — Структура репозитория + математическое ядро (недели 1–4)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§4).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

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
- [ ] [B] Сверить структуру каталогов с §10 ЭТАПА 4 (отчёт/комментарий в PR)
- [ ] [B] Завести `docs/STYLE_GUIDE.md` (PascalCase классы, snake_case функции, m_ для полей, UPPER_SNAKE константы; 4 пробела; ≤100 символов; `auto` запрещён в публичном API; `using namespace` запрещён в заголовках; new/delete и printf запрещены в runtime)
- [ ] [B] Включить `.clang-format` под правила стайлгайда
- [ ] [H] Шаблон PR: описание, чек-лист DoD (см. §17 Приложение В)

**Неделя 2 — fixed_t (16.16):**
- [ ] [B] Создать `src/core/math/fixed.h`:
  - [ ] `typedef int32_t fixed_t;` + `FIXED_BITS=16`, `FIXED_ONE = 1<<16`
  - [ ] `fixed_clamp_i64(int64_t)` (saturating clamp)
  - [ ] `fixed_from_int` (умножение на FIXED_ONE через int64_t + clamp)
  - [ ] `fixed_from_float` (isfinite→0 иначе; truncation toward zero; clamp к INT32_MIN/MAX)
  - [ ] `fixed_to_int`, `fixed_to_float`
  - [ ] `fixed_add/fixed_sub` (int64_t промежуточные, saturating)
  - [ ] `fixed_mul` (int64_t intermediate, масштаб 2^16, trunc toward zero, saturating)
  - [ ] `fixed_div` (int64_t; деление на 0: debug assert, release → INT32_MAX/INT32_MIN по знаку)
  - [ ] Объявить `fixed_sqrt` (реализация в .cpp: отрицательный вход — invalid: debug assert, release 0; целочисленный детерминированный алгоритм с bounded iteration, например Newton)
- [ ] [B] Doxygen-комментарии для всех публичных функций (@param/@return, note про saturating)
- [ ] [B] Тесты `tests/math/fixed_test.cpp` (GTest):
  - [ ] Conversion: `fixed_from_int(0/±1/100)`, round-trip to_int, float-конверсии с погрешностью < 0.001
  - [ ] Add/Sub/Mul/Div точные значения (10+3, 30−10, 5×7, 100/10, дробные 2.5×4.0, 10.0/3.0)
  - [ ] **SaturatingArithmetic:** `fixed_add(INT32_MAX,1)==INT32_MAX`; `fixed_sub(INT32_MIN,1)==INT32_MIN`
  - [ ] **MultiplyUsesQ16_16Scale:** `fixed_mul(from_int(5),from_int(7))==from_int(35)`
  - [ ] **DivisionByZero:** debug `EXPECT_DEATH`; release: `INT32_MAX` для a≥0, `INT32_MIN` для a<0
  - [ ] **AccuracyStressTest:** mt19937(42), 10 000 итераций, raw-значения ±6·FIXED_ONE, эталон в double; max error < 2.0/FIXED_ONE (mul и div)
  - [ ] **Determinism:** одинаковый вход → идентичный raw-выход; сравнение с независимым ожиданием (123×456)
  - [ ] Sqrt: `sqrt(25)==5`, `sqrt(2)≈1.414±0.01`, `sqrt(отрицательного)` → контракт
- [ ] [B] `tests/CMakeLists.txt`: `add_subdirectory(googletest EXCLUDE_FROM_ALL)`, `add_executable(retro_engine_tests ...)`, link `retro_engine` + `GTest::gtest_main`, `gtest_discover_tests`
- [ ] [B] `ctest --preset pc-debug` зелёный; push; CI зелёный

**Неделя 3 — Vec2/Vec3/Vec4:**
- [ ] [B] `src/core/math/vec2.h`, `vec3.h`, `vec4.h`:
  - [ ] Структуры {x,y,z} из fixed_t
  - [ ] `vec3_make/add/sub/scale`
  - [ ] **`fixed_reduce3_products`**: полный int64_t аккумулятор для всей редукции, clamp **ровно один раз** в конце
  - [ ] `vec3_dot` (через reduce3), `vec3_cross` (int64_t промежуточные по компонентам)
  - [ ] `vec3_length_sq`, `vec3_length`, `vec3_normalize` (деление нуля — контракт fixed_div)
- [ ] [B] Тесты `tests/math/vec3_test.cpp`:
  - [ ] Операции на целых значениях (1,2,3)·(4,5,6): dot = from_int(32)
  - [ ] **ReductionUsesSingleFinalClamp:** `vec3_dot(vec3(INT32_MAX,INT32_MAX,INT32_MAX), vec3(ONE,ONE,ONE)) == INT32_MAX` (сравнение raw fixed_t, не только float)
  - [ ] Cross на ортах (e1×e2=e3)
  - [ ] Normalize: unit-length, длина²≈ONE
- [ ] [H] Vec2/Vec4 — минимальный набор (make/add/sub/scale/dot) + тесты
- [ ] [B] CHANGELOG: запись в [Unreleased]

**Неделя 4 — Mat3/Mat4 + CI:**
- [ ] [B] `src/core/math/mat4.h` (column-major, как OpenGL):
  - [ ] `mat4_identity`, `mat4_multiply` (int64_t аккумуляторы по элементам)
  - [ ] `mat4_translate`, `mat4_rotate_x/y/z` (тригонометрия через fixed-таблицы: реализовать `core/math/sin_fixed/cos_fixed` с табличным/полиномным детерминированным алгоритмом)
  - [ ] `mat4_perspective(fov,aspect,near,far)`, `mat4_look_at(eye,target,up)`
  - [ ] `mat4_transform(m, Vec4) → Vec4`
  - [ ] Mat3 (rotation-подмножество) для конверсии в GTE-представление
- [ ] [B] Тесты `tests/math/mat4_test.cpp`:
  - [ ] Identity нейтрален; перемножение ассоциативно на фиксированных векторах
  - [ ] translate/rotate на известных углах (90° → поворот осей без накопления float-ошибки)
  - [ ] perspective: точка near/far проектируется корректно
  - [ ] look_at: глаз-камера смотрит в target
  - [ ] **Determinism:** raw-матрицы побитно равны между запусками (зафиксировать golden-векторы)
- [ ] [B] `.github/workflows/tests.yml` уже существует — убедиться, что триггеры push/PR на main/master/develop, добить матрицу позже
- [ ] [M] Бенчмарк-заготовка: `tests/benchmark/math_benchmark.cpp` (google/benchmark): BM_FixedMul, BM_Vec3Normalize, BM_Mat4Multiply (`benchmark::DoNotOptimize`); включается пресетом `pc-bench`
- [ ] [H] Критерии спринта: все тесты зелёные на PC (Windows/Linux/macOS в CI позже; сейчас локально + ubuntu)

## Критерии приёмки спринта 1–2

- [ ] Все unit-тесты проходят (100% pass rate)
- [ ] CI запускает тесты автоматически на push
- [ ] Точность по операциям подтверждена детерминированными векторами (не «запустил дважды на одной машине»)
- [ ] Doxygen на всех публичных функциях; код отформатирован

## Риски

Погрешность 16.16 высокой ⇒ (редко) пересмотреть отдельные операции на 32.32 с отдельным типом; недетерминизм ⇒ CI-тесты raw-значений.
