# Baseline латентности аллокаторов (Спринт 3–4)

> Источник задачи: [`sprint_03_04.md`](../plans/sprint_03_04.md), Недели 5–7 [H].
> Это **baseline**, а не acceptance-порог: `< 10 нс` в плане указано как
> ориентир наблюдения, а не критерий приёмки.

## Стенд

| Параметр | Значение |
|---|---|
| Дата | 2026-10-29 |
| CPU | 11th Gen Intel(R) Core(TM) i5-11300H @ 3.10 GHz (4 vCPU) |
| ОС | Linux 7.0.0-42-generic (x86_64) |
| Компилятор | GCC 15.2.0 |
| Сборка | `pc-bench` (Release, `-O3 -DNDEBUG`), CMake/Ninja |
| Итераций | 5 000 000 на операцию |
| Методика | `std::chrono::steady_clock`, compiler barrier на результат; медиана 5 прогонов |

## Результаты

| Операция | Медиана, нс/оп | Диапазон прогонов, нс/оп |
|---|---:|---:|
| `LinearAllocator::allocate(16, 8)` | 2.04 | 1.75 – 4.03 |
| `StackAllocator::push(16, 8)` | 2.01 | 1.82 – 2.62 |
| `Pool::allocate + Pool::deallocate` | 5.09 | 4.89 – 5.21 |

Все три операции укладываются в ориентир `< 10 нс` на данной машине.
Разброс объясняется планировщиком ОС/частотой ядра; значения не являются
детерминированным контрактом движка.

## Воспроизведение

```sh
cmake --preset pc-bench
cmake --build --preset pc-bench
./build/pc-bench/tests/memory_benchmark
```

Исходник: [`tests/benchmark/memory_benchmark.cpp`](../../tests/benchmark/memory_benchmark.cpp).
