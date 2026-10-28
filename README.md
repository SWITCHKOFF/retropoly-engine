# RetroPolyEngine (retro-engine)

Кроссплатформенный детерминированный движок для ретро-платформ: ядро на fixed-point математике (Q16.16, без плавающей запятой в игровой логике), HAL-архитектура с нулевым runtime-аллокатором в горячих путях. Целевые платформы MVP v0.1.0 — PC (SDL3 + OpenGL 3.3) и Sony PlayStation 1 (PSn00bSDK v0.24, pinned).

## Документация

| Документ | Назначение |
|---|---|
| [docs/RetroEngine_Engineering_Documentation_v1.43.md](docs/RetroEngine_Engineering_Documentation_v1.43.md) | Каноническая инженерная спецификация MVP (нормативная) |
| [docs/DEVELOPMENT_PLAN.md](docs/DEVELOPMENT_PLAN.md) | Операционный план работ: вехи, критический путь, PERT, треки, гейты G1–G13 |
| [docs/plans/README.md](docs/plans/README.md) | Указатель детальных планов спринтов (`docs/plans/sprint_NN.md`) |

## Сборка

Канонические пресеты (единственная точка входа — `CMakePresets.json`):

```sh
cmake --preset pc-debug
cmake --build --preset pc-debug
ctest --preset pc-debug --output-on-failure
```

Другие пресеты: `pc-release`, `pc-debug-render`, `ps1-release` (+ опциональные `pc-asan`, `pc-bench`, `pc-coverage`).

## Лицензия

MIT — см. [LICENSE](LICENSE). Вендоренные зависимости и их лицензии: [LEGAL.md](LEGAL.md).
