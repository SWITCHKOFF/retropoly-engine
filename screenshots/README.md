# Скриншоты (PC vs PS1 vs N64)

## `rotating_cube_ps1*.png` — W1 demo, DuckStation (Спринт 17–18)

Ручная приёмка 2026-11-16: `build/ps1-release/src/hal/ps1/RETRO.cue`
в DuckStation `0.1-12754-g82a118249 [master]` (Linux x64, Software
renderer, fast boot, NTSC-U, VSync off, speed 100 %, окно 640×480).

- `rotating_cube_ps1.png` — чистый F10: 4 куба двумя башнями
  (48 треугольников), 4 текстуры 256×256, фиксированная камера
  `(0, 3.2, 7)`, vertex lighting/Gouraud; верхние кубы стоят на нижних.
- `rotating_cube_ps1_overlay.png` — Select-overlay: `FPS 62 / TRI 20 /
  RAM 931K`.
- `rotating_cube_ps1_perfmon.png` — OSD PerfMon: `29.93 FPS · 59.86 VPS ·
  100 % (100 %)`; чистое 60-секундное окно (overlay off) дало
  FPS min/avg/median/max `27.89 / 29.86 / 29.91 / 30.51`, VPS
  `55.79 / 59.74 / 59.83 / 61.01`, avg frame 16.74 ms. Внутренний
  PerfMon FPS — поле 59.94/2 = 29.97 Гц (номинал 30 FPS PS1); гейт
  «≥ 30» фиксируется по VPS 59.7 и engine FPS 62. 10-мин стресс (Xvfb,
  634 с): 630 окон PerfMon, FPS 22.99/29.73/29.91/30.99,
  VPS 45.16/59.46/59.82/61.73, без E/F/crash.
- `rotating_cube_ps1_jump.png` — момент прыжка (Крестик): кубы
  поднимаются; `rotating_cube_ps1_landed.png` — после приземления:
  AABB-коллизии возвращают кубы на пол/стек (башни снова собраны).

RAM 931 KiB / 1.5 MiB, VRAM ≈ 494 KiB / 800 KiB, PS-EXE 122 880 B,
CD-образ 682 080 B. Метрики/окружение —
[`../docs/benchmarks/reference_environment.yaml`](../docs/benchmarks/reference_environment.yaml)
(`ps1_emulator`), чек-лист — [`../docs/plans/sprint_17_18.md`](../docs/plans/sprint_17_18.md).

> PC/PS1 parity fix 2026-11-16: PC-бэкенд теперь получает projection
> для view-space вершин (без повторного proj·view·model); PC golden
> rebased, `rotating_cube_pc.png`/`hello_triangle_pc.png` перегенерированы.
> PS1-геометрия W1 (GTE H=171) совпадает с PC по композиции.

## `n64_hello_triangle.png` / `n64_rotating_cube.png` — N64 v0.3.0 (Спринт 45–46)

Pinned gopher64 1.2.1 (Xvfb/llvmpipe Vulkan, 320×240, 16-bit VI):
`hello_triangle` (TRIANGLE.MSH + TEX0.TEX, интерактивная камера,
swing ±0.6 rad) и W1 `rotating_cube` (4 куба/48 tri, fixed camera,
RSP light). Overlay: режим, RSP TNL+RDP | LIGHT, frame/FPS, peak RDRAM,
TMEM, TNL/RDP µs, draw calls, W0 hash. 60-с gate: 59 FPS, без ERROR;
10-мин stress 613 361 мс / 36 600 кадров. Подробности —
[`../docs/N64.md`](../docs/N64.md) §5.2/§9.3,
[`../docs/benchmarks/n64_hello_triangle_v0_3_0.yaml`](../docs/benchmarks/n64_hello_triangle_v0_3_0.yaml).

## `n64_render_s43_44.png` — N64 renderer gate (Спринт 43–44)

Pinned gopher64 1.2.1 (Xvfb/llvmpipe Vulkan, 320×240, 16-bit VI):
4 вращающихся текстурированных куба (48 треугольников), RSP T&L +
RSP dot-lighting, RDPQ с аппаратным Z (18-bit internal), CI8
TMEM-полосы 32×64 + TLUT (2560 B), hardware bilinear. Overlay:
`RPE N64 S43-44`, `RSP TNL+RDP`, `RSP LIGHT`, frame/FPS, W0 hash.
W0 5/5 PASS (`9e92e718`/`569949c5`), CPU-fallback hash совпадает,
Z self-test near-first/far-second PASS, W1 ≈59 FPS (16.9 ms).
Подробности — [`../docs/N64.md`](../docs/N64.md) §6.1/§9.2,
[`../docs/adr/ADR-015-n64-rsp-rdp.md`](../docs/adr/ADR-015-n64-rsp-rdp.md).

Отличия от PC/PS1/Saturn: perspective-correct UV (не affine), hardware
bilinear (не nearest), 16-bit output с D-pad/VI dither-политикой,
аппаратный Z вместо Painter's; TMEM-полосы и downsample 256×256 → 64×64.

## `n64_boot_s41_42.png` — N64 HAL gate (Спринт 41–42)

Скриншот boot-demo в pinned gopher64 1.2.1 (Xvfb/llvmpipe, 320×240,
16-bit VI): тёмно-синий clear `(0,0,32)`, строки `BOOT OK / S41-42`,
`DFS OK MESH 600 TEX 66068` (`CUBE.MSH`/`TEX0.TEX` из read-only DFS),
memory overlay (asset current/peak, frame/command/FS staging), состояние
pad 1 и frame/time. Ручной gate: 60 с + 10-минутный stress, input-лог
`[n64_input] pad0 press …`; эмулятор/ROM не вендорятся. Подробности —
[`../docs/N64.md`](../docs/N64.md) §9.

## `rotating_cube_pc.png` — демо MVP, веха M7 (Спринт 17–18)

Скриншот PC-версии `rotating_cube`, сгенерирован из rebased golden-эталона
`tests/visual/reference/rotating_cube_default.bin.px` (15-битный readback
reference-фреймбуфера 320×240, кадр t=0):

```sh
python3 scripts/golden_px_to_png.py \
  tests/visual/reference/rotating_cube_default.bin.px \
  screenshots/rotating_cube_pc.png
```

- W1-сцена: 4 куба (48 треугольников) двумя башнями, CI8/256, Gouraud,
  vertex lighting ambient + directional, transparency off, фикс. камера;
- инвентарь кадра: 20 подано / 28 backface (невидимые грани кубов), 0
  clip/depth-отказов;
- после parity fix 2026-11-16 геометрия совпадает с PS1 (без double-transform).

## `hello_triangle_pc.png`

Скриншот PC-версии, сгенерирован из rebased golden-эталона
`tests/visual/reference/hello_triangle_default.bin.px`:

```sh
python3 scripts/golden_px_to_png.py \
  tests/visual/reference/hello_triangle_default.bin.px \
  screenshots/hello_triangle_pc.png
```

Ожидаемое изображение: треугольник с CI8/Gouraud-текстурой на тёмно-синем
фоне `(0, 0, 32)`.

## `hello_triangle_ps1.png`

PS1-скриншот получен вручную в DuckStation 0.1-12640-gbb4e9ac5e из собранного
образа `build/ps1-release/src/hal/ps1/RETRO.cue`:

- pinned build (`ps1-release`), Software renderer, NTSC-U, fast boot, speed 100%;
- запущен образ, дождались стабильного кадра, F10 (Save Screenshot).

Метрики ручного прогона (emulator validation gate):
- PerfMon: FPS/VPS ≈ 59.8, avg frame time ≈ 16.7 ms, эмуляция 100 %;
- RAM: engine-owned static + арены ≈ 815 KiB / 1.5 MiG budget;
- VRAM: фреймбуферы + CLUT + texture page ≈ 366 KiB / 800 KiB budget;
- PS-EXE size: 96 256 bytes / 512 000 bytes.

Ожидаемые raster-различия PC vs PS1 (нормативные для MVP):
- PS1 использует affine UV-интерполяцию (GL-бэкенд PC — тоже affine),
  поэтому перспективных искажений нет;
- 12.4 пиксельная сетка GTE + 15-бит RGB555 даёт меньшую глубину цвета
  и возможное смещение силуэтных рёбер на 1 px;
- 320×240 NTSC-развёртка PS1 совпадает с reference-разрешением PC;
- backface-cull и Painter's order общие (ядро рендера), поэтому
  композиция кадра идентична.
