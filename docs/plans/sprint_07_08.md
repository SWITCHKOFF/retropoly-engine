# Спринт 7–8 — Рендерер с эмуляцией PS1 (OpenGL 3.3) (недели 13–16)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§4).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 7–8 |
| Недели | 13–16 — 01.02.2027 – 28.02.2027 |
| Веха | M2 — PC-платформа (часть 2: рендерер) |
| Цель | Render_Init/BeginFrame/EndFrame/Clear, фреймбуфер 320×240, шейдеры affine+quantize, Painter's algorithm, command buffer |
| Входные критерии | Спринт 5–6 завершён (HAL PC) ([`sprint_05_06.md`](sprint_05_06.md)) |
| На критическом пути | Да |
| Выходные артефакты | `renderer.h/.cpp`, `mesh.h/.cpp`, `texture.h/.cpp`, `command_buffer.h`, `render_opengl.cpp`, шейдеры, W0-reference-файлы |
| Следующий спринт | [`sprint_09_10.md`](sprint_09_10.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 50 | 80 | 52.5 | 7.5 | 67.5 | 40–60 ч | ⚠ При 10 ч/нед дефицит 12.5 ч → scope reduction ([M]/[H]) или recovery-неделя после S8 |

## Задачи

**Неделя 13 — Каркас рендерера:**
- [ ] [B] `Render_Init(w,h)` / `Render_Shutdown` / `Render_BeginFrame` / `Render_EndFrame` / `Render_Clear(r,g,b)` в `src/render/renderer.cpp` + backend-вызовы в `src/hal/pc/render_opengl.cpp`
  - [ ] **Lifecycle-контракт:** Platform_Init → (ровно один) Render_Init до первого BeginFrame; тест-инвариант: двойной Render_Init или EndFrame до Init = нарушение
- [ ] [B] Отключить Z-тест: `glDisable(GL_DEPTH_TEST)` в reference PC renderer
- [ ] [H] Smoke: окно очищается цветом (0,0,32)
- [ ] [B] Пресет `pc-debug-render` + рабочий `xvfb-run` для CI позже

**Неделя 14 — Фреймбуфер и шейдеры:**
- [ ] [B] FBO 320×240 (рендер в текстуру) → вывод на окно с nearest-масштабированием (fullscreen quad + nearest sampling)
- [ ] [B] `src/render/shaders/ps1_emu.vert`:
  - [ ] MVP-трансформация, `noperspective out vec2 v_texcoord` (affine!)
  - [ ] Квантование 1/16 px: screen=floor(screen*16+0.5)/16 в 320×240 пространстве (wobble heuristic)
- [ ] [B] `src/render/shaders/ps1_emu.frag`:
  - [ ] CI8: index = round(texture(u_tex, uv).r * 255); color = texelFetch(u_palette, (index,0))
  - [ ] Умножение на вершинный цвет (Gouraud)
- [ ] [B] Шейдер-компилятор/линковка с логированием ошибок через Logger
- [ ] [H] Тест: нетекстурированный треугольник рисуется

**Неделя 15 — Текстуры + квантование:**
- [ ] [B] `Render_CreateTexture(w,h, pixels_u8_indexed, palette_u16_rgb555stp)` / Destroy / Bind (TextureHandle)
  - [ ] Палитра: 256×u16, layout **R[4:0], G[9:5], B[14:10], STP[15]**
  - [ ] Unit-тест bit layout: `pack_rgb555_stp(31,0,0,true)==0x801F`, `(0,31,0,true)==0x83E0`, `(0,0,31,true)==0xFC00`
  - [ ] Запрет тестовых заглушек вида `(i<<8)|i` вместо честной RGB555+STP
- [ ] [B] Affine-текстурирование подтверждено визуально (текстуры «плывут» на наклонных плоскостях)
- [ ] [H] Vertex-формат движка `Vertex`: fixed_t x,y,z; int8_t nx,ny,nz; uint8_t u,v,r,g,b,a (декларация в platform.h — согласовать с .mesh v1)

**Неделя 16 — DrawMesh + сортировка + command buffer:**
- [ ] [B] `Render_SetCamera(view,proj)`, `Render_DrawMesh(vertices, count, indices, index_count, model)`
- [ ] [B] Painter's algorithm: std::sort по camera-space depth (far→near), **tie-breaker submission_index** (детерминизм), depth вне диапазона — clamp/через depth_key-контракт
  - [ ] `depth_key_from_camera_z`: z<0 или вне [min,max] → DEPTH_REJECT (без modulo)
  - [ ] Тест `RenderContract.RejectsOutOfRangeDepthWithoutModuloAlias`: (-1→REJECT), (4096→REJECT), (1024 ok)
- [ ] [B] `src/render/command_buffer.h`: фиксированный буфер 64 KiB; `draw_mesh()` → status bool; **overflow = явная ошибка (status=false + diagnostic), никакого silent drop**
- [ ] [B] **W0-конформанс:** сцена 320×240, 12 текстурированных треугольников, CI8/256, Gouraud, без прозрачности, фиксированная камера
  - [ ] Готовые конформанс-кейсы (детерминированные): полностью видимый треугольник; backface-culled; пересекающий near plane; пересекающий far plane; invalid depth/clip rejection
  - [ ] Golden-сравнение: `Render_TestReadbackHash()` vs файл `tests/visual/reference/*.bin`; хэши в репо
  - [ ] Метод сравнения: framebuffer readback + hash (не «не крашится»)
- [ ] [B] `benchmarks/reference_environment.yaml` — каркас метаданных (os/cpu/gpu/driver/compiler/build_type/resolutions/vsync/power_mode/measurement_seconds/git_revision/asset_set_sha256)
- [ ] [B] CHANGELOG + PR

## Критерии приёмки спринта 7–8

- [ ] 320×240 → масштаб на окно; wobble и affine видны; CI8-текстуры работают
- [ ] Z-тест выключен; сортировка back-to-front корректна на тестовой сцене
- [ ] W0-golden хэш зафиксирован и детерминирован
- [ ] RenderDoc: draw calls корректны (ручная проверка)
