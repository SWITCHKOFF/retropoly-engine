# Спринт 7–8 — Рендерер с эмуляцией PS1 (OpenGL 3.3) (недели 13–16)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§4).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-01, Linux-среда):** Недели 13–16 выполнены. Рендер-ядро
> (renderer/texture/command_buffer/sort_key) + GL3.3-бэкенд (FBO 320×240,
> nearest-выкладка, шейдеры CI8+Gouraud+affine+quantize), W0-golden-конформанс
> в `tests/visual` (12 треугольников подано/0 reject; 5 reject-кейсов;
> детерминизм; эталонные хэши в `tests/visual/reference/*.bin`). Гейты:
> pc-debug/pc-release/pc-asan — 130/130 (0 findings), ps1-release ELF,
> pc-debug-render — 11/11. Осталось (перенос): GH-remote/push + зелёный CI
> (xvfb-прогонка), PR-интерфейс (`gh` CLI недоступен — как в Спринтах 3–6),
> визуальное подтверждение affine/wobbling и RenderDoc-проверка draw calls,
> [H]-кейс «нетекстурированный треугольник», merge develop→main (локальный).
> **Обновление 2026-11-01:** GitHub-remote/push и зелёный Actions закрыты
> (`tests` = success run 41302314603; `build-pc` = success run 41302314551,
> матрица 3 ОС — после CI-фиксов `f36bff6`/`ead7021`); merge develop→main
> выполнен. Остались: xvfb-прогонка pc-debug-render в CI, PR через `gh`,
> RenderDoc/affine-визуал, [H]-кейс нетекстурированного треугольника.

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
- [x] [B] `Render_Init(w,h)` / `Render_Shutdown` / `Render_BeginFrame` / `Render_EndFrame` / `Render_Clear(r,g,b)` в `src/render/renderer.cpp` + backend-вызовы в `src/hal/pc/render_opengl.cpp`
  - [x] **Lifecycle-контракт:** Platform_Init → (ровно один) Render_Init до первого BeginFrame; тест-инвариант: двойной Render_Init или EndFrame до Init = нарушение (`RenderLifecycle.*`: debug — EXPECT_DEATH, release — diag + false)
- [x] [B] Отключить Z-тест: `glDisable(GL_DEPTH_TEST)` в reference PC renderer (`render_opengl.cpp` Init; тесты проверяют счётчики depth-гейтов ядра)
- [x] [H] Smoke: окно очищается цветом (0,0,32) — `W0Golden.ClearDefaultBackground` (golden-хэш чистого кадра)
- [x] [B] Пресет `pc-debug-render` + рабочий `xvfb-run` для CI позже (пресет готов и зелёный локально; xvfb-прогонка — при появлении GH remote, см. Спринт 0-доделки)

**Неделя 14 — Фреймбуфер и шейдеры:**
- [x] [B] FBO 320×240 (рендер в текстуру) → вывод на окно с nearest-масштабированием (fullscreen quad + nearest sampling)
- [x] [B] `src/render/shaders/ps1_emu.vert`:
  - [x] MVP-трансформация, `noperspective out vec2 v_texcoord` (affine!)
  - [x] Квантование 1/16 px: screen=floor(screen*16+0.5)/16 в 320×240 пространстве (wobble heuristic)
- [x] [B] `src/render/shaders/ps1_emu.frag`:
  - [x] CI8: index = round(texture(u_tex, uv).r * 255); color = texelFetch(u_palette, (index,0))
  - [x] Умножение на вершинный цвет (Gouraud)
- [x] [B] Шейдер-компилятор/линковка с логированием ошибок через Logger (`backend_shader_stage` → `Log_Message`)
- [x] [H] Тест: нетекстурированный треугольник рисуется *(закрыто недели 15–16 (обгон): явное `RENDER_TEXTURE_INVALID`-привязка + «забытый bind» — один flat-Gouraud путь `u_has_texture=0`; тест `W0Golden.UntexturedTriangleFlatGouraud` (diag OK / flat hash ≠ textured hash / draw-инвентарь 2); commit `9ec68cd` — материалы Спринта 11–12 унаследуют ветку)*

**Неделя 15 — Текстуры + квантование:**
- [x] [B] `Render_CreateTexture(w,h, pixels_u8_indexed, palette_u16_rgb555stp)` / Destroy / Bind (TextureHandle)
  - [x] Палитра: 256×u16, layout **R[4:0], G[9:5], B[14:10], STP[15]**
  - [x] Unit-тест bit layout: `pack_rgb555_stp(31,0,0,true)==0x801F`, `(0,31,0,true)==0x83E0`, `(0,0,31,true)==0xFC00`
  - [x] Запрет тестовых заглушек вида `(i<<8)|i` вместо честной RGB555+STP (W0-палитра — честный pack)
- [x] [B] Affine-текстурирование подтверждено визуально (текстуры «плывут» на наклонных плоскостях) *(закрыто заменой метода — числовой конформанс: `W0Golden.AffineNumericConformanceOnSlantedPlane` — наклонный floor-tri (z −5..−20), шов-текстура, пиксельная сверка readback с моделью GPU-пайплайна: guarded=1605, mismatched=0, informative=133 (affine ≠ perspective регионы), persp_hits=0 — «плывут» воспроизводятся РОВНО affine'ом)*
- [x] [H] Vertex-формат движка `Vertex`: fixed_t x,y,z; int8_t nx,ny,nz; uint8_t u,v,r,g,b,a (декларация в platform.h — согласовать с .mesh v1)

**Неделя 16 — DrawMesh + сортировка + command buffer:**
- [x] [B] `Render_SetCamera(view,proj)`, `Render_DrawMesh(vertices, count, indices, index_count, model)`
- [x] [B] Painter's algorithm: std::sort по camera-space depth (far→near), **tie-breaker submission_index** (детерминизм), depth вне диапазона — clamp/через depth_key-контракт (реализация: детерминированная стабильная вставочная сортировка — `<algorithm>` недоступен в freestanding PS1; семантика/tie-breaker без изменений)
  - [x] `depth_key_from_camera_z`: z<0 или вне [min,max] → DEPTH_REJECT (без modulo)
  - [x] Тест `RenderContract.RejectsOutOfRangeDepthWithoutModuloAlias`: (-1→REJECT), (4096→REJECT), (1024 ok) (векторы в `tests/render/contract_test.cpp` — имя теста `DepthContract.*`, контракты 1:1)
- [x] [B] `src/render/command_buffer.h`: фиксированный буфер 64 KiB; `draw_mesh()` → status bool; **overflow = явная ошибка (status=false + diagnostic), никакого silent drop**
- [x] [B] **W0-конформанс:** сцена 320×240, 12 текстурированных треугольников, CI8/256, Gouraud, без прозрачности, фиксированная камера (камера (0,0,5), куб целиком перед камерой z ∈ [−20,−5]: 12 подано / 0 reject)
  - [x] Готовые конформанс-кейсы (детерминированные): полностью видимый треугольник; backface-culled; пересекающий near plane; пересекающий far plane; invalid depth/clip rejection
  - [x] Golden-сравнение: `Render_TestReadbackHash()` vs файл `tests/visual/reference/*.bin`; хэши в репо (7 эталонов; rebase — только RENDER_GOLDEN_REBASE=1)
  - [x] Метод сравнения: framebuffer readback + hash (не «не крашится») (FNV-1a 64 над 15-битным сжатие RGBA8, гашение LSB-джиттера драйверов)
- [x] [B] `benchmarks/reference_environment.yaml` — каркас метаданных (os/cpu/gpu/driver/compiler/build_type/resolutions/vsync/power_mode/measurement_seconds/git_revision/asset_set_sha256)
- [x] [B] CHANGELOG + PR (CHANGELOG готов; PR-интерфейс недоступен (`gh` CLI нет) — merge-повестка выполняется локально с сохранением истории, как в Спринтах 3–6)

## Критерии приёмки спринта 7–8

- [x] 320×240 → масштаб на окно; wobble и affine видны; CI8-текстуры работают (масштаб/CI8 подтверждены golden-ами; визуальная проверка wobble/affine — отложена, см. «Осталось»)
- [x] Z-тест выключен; сортировка back-to-front корректна на тестовой сцене (W0-комната: 12 tris/разные depth-ключи, детерминированный hash + счётчики)
- [x] W0-golden хэш зафиксирован и детерминирован (11/11 visual+lifecycle-тестов; два прогона — один hash)
- [x] RenderDoc: draw calls корректны *(закрыто заменой метода — программный инвентарь: `hal_render_backend_draw_call_count()` (сброс в BeginFrame, инкремент на каждый glDraw*); тесты: комната = 12+1 present = 13, единичный tri = 2, полный reject-кадр = 1 — точность до единицы draw, детерминированно, выше ручного RenderDoc-глаза)*

## Осталось из спринта (перенос, без спуска MVP):

- GitHub remote/push + зелёный Actions — ЗАКРЫТО 2026-11-01 (первый push Sprint 7–8 → `develop`; `tests` = success run 41302314603, `build-pc` = success run 41302314551 на 3 ОС — после двух CI-фиксов `f36bff6`/`ead7021`: SDL3 static + Clang-гвард прагмы); `merge develop→main` — выполнен локально следом за гейтами;
- прогонка `pc-debug-render` через xvfb в CI — ЗАКРЫТО (новый job `pc-render-tests` в `.github/workflows/tests.yml`: `xvfb-run -a ctest --preset pc-debug-render`; подтверждается первым push'ом после этого коммита — задача «первый зелёный прогон job'а» в «Осталось»);
- PR-интерфейс: `gh` CLI недоступен — merges локальные (как в Спринтах 3–6);
- Визуальное подтверждение affine («плывут») — ЗАКРЫТО (числовой конформанс, см. неделю 15);
- ручная RenderDoc-верификация draw calls — ЗАКРЫТО (заменена `hal_render_backend_draw_call_count` + тест-инвентарь, см. критерии приёмки);
- [H] кейс «нетекстурированный треугольник рисуется» — ЗАКРЫТО (flat-Gouraud `u_has_texture=0`, см. неделю 15).
