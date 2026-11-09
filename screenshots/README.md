# Скриншоты Hello Triangle (PC vs PS1)

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
