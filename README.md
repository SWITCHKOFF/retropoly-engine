# RetroPolyEngine (retro-engine)

Кроссплатформенный детерминированный движок для ретро-платформ: ядро на
fixed-point математике (Q16.16, без плавающей запятой в игровой логике),
HAL-архитектура с нулевым runtime-аллокатором в горячих путях. Целевые
платформы MVP v0.1.0 — PC (SDL3 + OpenGL 3.3) и Sony PlayStation 1
(PSn00bSDK v0.24, pinned). v0.2.0 (в работе) — Sega Saturn: pinned Jo
Engine v2026.2, bootable HAL 320×224; Master + Slave SH-2 (S25–26)
задействованы через SPSC-очередь и clip-space workload (**+32%**),
single-CPU fallback и fault injection; VDP1/3D — следующие спринты.

![rotating_cube PC](screenshots/rotating_cube_pc.png)

## Быстрый старт

```sh
git clone --recursive https://github.com/RetroPolyEngine/retro-engine.git
cd retro-engine

# PC debug + тесты
cmake --preset pc-debug
cmake --build --preset pc-debug
ctest --preset pc-debug --output-on-failure

# Запуск примера
./build/pc-release/game/rotating_cube
```

> Без `--recursive` SDL3/googletest не подтянутся — см. [docs/BUILDING.md](docs/BUILDING.md).

## Что внутри

- **Fixed-point ядро**: скаляр/вектор/матрицы Q16.16, saturating arithmetic,
  детерминированная тригонометрия — без float в игровой логике.
- **ECS**: 7 компонентных пулов с фиксированными ёмкостями, dense-итерация,
  generation-хэндлы с защитой от stale-use.
- **HAL**: единый C-API (`Platform/Render/Input/Audio/FileSystem`) с тремя
  compile-time бэкендами — PC (SDL3 + OpenGL 3.3), PS1 (PSn00bSDK,
  GTE+GPU+OT) и Saturn HAL (Jo Engine, VDP2 backdrop, Master+Slave S25–26).
- **Рендерер**: 320×240 reference framebuffer, affine-текстурирование,
  1/16-пиксельное квантование, Painter's algorithm, CI8/RGB555+STP.
- **Ассеты**: `.mesh` v1 (glTF→движок) и `.tex` v1 (PNG→CI8+палитра),
  host-only конвертер, безопасные загрузчики с rollback арены.
- **Демо MVP**: `rotating_cube` — 4 куба, 48 треугольников, AABB-коллизии,
  vertex lighting, debug overlay; W1 ≥ 60 FPS на PC, ≥ 30 FPS в DuckStation.

## Документация

| Документ | Назначение |
|---|---|
| [docs/RetroEngine_Engineering_Documentation_v1.43.md](docs/RetroEngine_Engineering_Documentation_v1.43.md) | Каноническая инженерная спецификация MVP (нормативная) |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Слои движка, пайплайн рендера, ассет-пайплайн, платформенные контракты |
| [docs/BUILDING.md](docs/BUILDING.md) | Установка тулчейна, канонические пресеты, запуск PC/PS1/Saturn, troubleshooting |
| [docs/SATURN.md](docs/SATURN.md) | Saturn: первоисточники, toolchain, разрешение, память, ввод, эмулятор |
| [docs/adr/ADR-008-saturn-backend.md](docs/adr/ADR-008-saturn-backend.md) | Нормативное решение по Saturn backend |
| [docs/adr/ADR-009-master-slave.md](docs/adr/ADR-009-master-slave.md) | Master+Slave SH-2: SPSC-синхронизация, workload, fallback |
| [docs/API.md](docs/API.md) | Вводный обзор API: ECS, Renderer, Input, FileSystem, Math |
| [docs/ASSETS.md](docs/ASSETS.md) | Байтовые форматы `.mesh`/`.tex` v1, CLI конвертера, загрузчики |
| [docs/TESTING.md](docs/TESTING.md) | Пирамида тестов, запуск unit/integration/visual, QA-чек-листы |
| [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) | Форк→ветка→тесты→PR, стиль, Doxygen, review-процесс |
| [docs/CODE_OF_CONDUCT.md](docs/CODE_OF_CONDUCT.md) | Contributor Covenant |
| [docs/LEGAL.md](docs/LEGAL.md) | Лицензии зависимостей и правила сохранения notices |
| [docs/STYLE_GUIDE.md](docs/STYLE_GUIDE.md) | Именование, форматирование, ограничения языка |
| [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | Частые проблемы и решения |
| [docs/DEVELOPMENT_PLAN.md](docs/DEVELOPMENT_PLAN.md) | Операционный план: вехи M0–M8, PERT, критический путь, гейты G1–G13 |
| [docs/plans/README.md](docs/plans/README.md) | Детальные планы спринтов (`docs/plans/sprint_NN.md`) |

## Скриншоты

| hello_triangle PC | rotating_cube PC | hello_triangle PS1 (эмулятор) | Saturn dual-CPU S25–26 (Mednafen) |
|---|---|---|---|
| ![ht-pc](screenshots/hello_triangle_pc.png) | ![rc-pc](screenshots/rotating_cube_pc.png) | ![ht-ps1](screenshots/hello_triangle_ps1.png) | ![saturn](screenshots/saturn_clear_s23_24.png) |

## Канонические пресеты

Единственная точка входа — `CMakePresets.json`:

| Пресет | Назначение |
|---|---|
| `pc-debug` | Разработка + unit-тесты |
| `pc-release` | Оптимизированные примеры |
| `pc-debug-render` | W0-golden + visual regression (`xvfb-run` в CI) |
| `pc-asan` | AddressSanitizer |
| `pc-bench` | google/benchmark |
| `pc-coverage` | lcov покрытие |
| `ps1-release` | Кросс-сборка PS-EXE + CD-образ |

Подробнее — [docs/BUILDING.md](docs/BUILDING.md).

## Статус MVP v0.1.0

- [x] Fixed-point математика (M1)
- [x] Аллокаторы + ECS (M1)
- [x] HAL PC + OpenGL-рендерер (M2)
- [x] Конвертер ассетов + безопасные загрузчики (M3)
- [x] hello_triangle PC (M4)
- [x] HAL PS1 + CD-ROM ФС (M5)
- [x] hello_triangle PS1 (M6)
- [x] rotating_cube demo (M7)
- [x] Документация + release gate + GitHub Release (M8)

Release-контракт: `scripts/verify_release_contract.py`.

## Лицензия

MIT — см. [LICENSE](LICENSE). Вендоренные зависимости и их лицензии:
[LEGAL.md](LEGAL.md).
