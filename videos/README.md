# Видео демки rotating_cube (S17–18)

Записаны 2026-11-16 с финальных сборок после PC/PS1 parity fix.
Формат: WebM / VP9, 640×480, 30 fps, без звука (audio gate S53–54
проверяется отдельно).

| Файл | Длительность | Источник |
|---|---:|---|
| `rotating_cube_pc.webm` | 12.8 с | `build/pc-release/game/rotating_cube`, PC OpenGL-бэкенд; overlay `FPS 132 / TRI 20 / RAM 492K`, прыжки Space |
| `rotating_cube_ps1.webm` | 12.3 с | DuckStation `0.1-12754-g82a118249`, Software, NTSC-U, `RETRO.cue`; overlay `FPS 62 / TRI 20 / RAM 931K`, OSD `VPS 59.8 / 100 %`, прыжки Cross |

Запись: GStreamer `ximagesrc` (Xvfb, окно эмулятора/PC-игры) →
`videoscale` 640×480 → `vp9enc` → `webmmux`. Исходные кадры — реальные
прогоны, не раскадровка golden.

Скриншоты тех же сцен — [`../screenshots/`](../screenshots/README.md).
