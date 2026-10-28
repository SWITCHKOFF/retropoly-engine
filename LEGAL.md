# LEGAL.md — Лицензии вендоренных зависимостей

Спринт 0 §3.6 (docs/plans/sprint_00.md). Полная фиксация — VERSION.txt (тег/commit, SHA-256).

| Компонент | Путь | Лицензия | Примечание |
|---|---|---|---|
| SDL3 | `src/third_party/sdl3` | zlib | Подключение ТОЛЬКО через `add_subdirectory` (vcpkg — не-канонический fallback) |
| GoogleTest | `src/third_party/googletest` | BSD-3-Clause | Canonical; FetchContent запрещён |
| PSn00bSDK (core) | `src/third_party/psn00bsdk` | MPL-2.0 | Pinned v0.24 (`06e65be`); обновление — отдельная задача с ADR |
| mkpsxiso / dumpsxiso | `psn00bsdk/tools/mkpsxiso` | GPL-2.0-or-later | Сохранять notices; вложенный submodule (не инициализируется) |
| cgltf | `src/third_party/cgltf/cgltf.h` | MIT | Pinned v1.15 |
| stb (image, image_write) | `src/third_party/stb/` | PD/MIT (dual) | Pinned archive `003501f6` |
| glad | `src/third_party/glad/` | MIT | Сгенерирован glad2: OpenGL Core 3.3 + loader, extensions=all |

Правило распространения: проект распространяется под MIT (см. LICENSE); при включении компонентов под GPL (mkpsxiso) в распространяемые пакеты соблюдаются их лицензии и обязательные attributions.
