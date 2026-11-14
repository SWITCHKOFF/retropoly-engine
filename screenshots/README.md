# Скриншоты (PC vs PS1 vs N64)

## `n64_boot_s41_42.png` — N64 HAL gate (Спринт 41–42)

Скриншот boot-demo в pinned gopher64 1.2.1 (Xvfb/llvmpipe, 320×240,
16-bit VI): тёмно-синий clear `(0,0,32)`, строки `BOOT OK / S41-42`,
`DFS OK MESH 600 TEX 66068` (`CUBE.MSH`/`TEX0.TEX` из read-only DFS),
memory overlay (asset current/peak, frame/command/FS staging), состояние
pad 1 и frame/time. Ручной gate: 60 с + 10-минутный stress, input-лог
`[n64_input] pad0 press …`; эмулятор/ROM не вендорятся. Подробности —
[`../docs/N64.md`](../docs/N64.md) §9.

## `rotating_cube_pc.png` — демо MVP, веха M7 (Спринт 17–18)

Скриншот PC-версии `rotating_cube`, сгенерирован из golden-эталона
`tests/visual/reference/rotating_cube_default.bin` (RGBA-dump кадра t=0
через `RENDER_GOLDEN_DUMP=1`; конвертация dump → PNG с переворотом строк —
`glReadPixels` отдаёт снизу-вверх):

- W1-сцена: 4 куба (48 треугольников) двумя башнями, CI8/256, Gouraud,
  vertex lighting ambient + directional, transparency off, фикс. камера;
- инвентарь кадра: 20 подано / 28 backface (невидимые грани кубов), 0
  clip/depth-отказов.

## `hello_triangle_pc.png`

Скриншот PC-версии, сгенерирован из golden-эталона
`tests/visual/reference/hello_triangle_default.bin.px` (15-битный readback
reference-фреймбуфера 320×240):

```sh
head -c 153600 tests/visual/reference/hello_triangle_default.bin.px | \
  ffmpeg -f rawvideo -pix_fmt rgb555le -s 320x240 -i - \
         -frames:v 1 -sws_flags neighbor -pix_fmt rgb24 \
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
