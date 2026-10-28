# Спринт 13–14 — HAL для PS1 (PSn00bSDK) (недели 25–28)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§5).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 13–14 |
| Недели | 25–28 — 26.04.2027 – 23.05.2027 |
| Веха | M5 — PS1-платформа |
| Цель | PS1-бэкенд: платформа, ввод, рендер (GTE+GPU+OT), CD-ROM ФС; PS-EXE грузится в DuckStation |
| Входные критерии | Спринт 11–12 завершён (hello_triangle PC) ([`sprint_11_12.md`](sprint_11_12.md)); pinned PSn00bSDK v0.24 из Спринта 0 |
| На критическом пути | Да |
| Выходные артефакты | `src/hal/ps1/*` (platform_ps1, GTE-адаптер, OT, input_pad, filesystem_cdrom), `ps1-disc.xml`, `build-ps1.yml` |
| Следующий спринт | [`sprint_15_16.md`](sprint_15_16.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 40 | 55 | 85 | 57.5 | 7.5 | 72.5 | 40–60 ч | ⚠ Максимальный риск критического пути (115% среднего номинала): нужен ранний резерв — recovery-неделя после S14 и перенос части [H] в пост-MVP при просадке |

## Задачи

**Неделя 25 — Сборка PS1:**
- [ ] [B] `src/hal/ps1/CMakeLists.txt`:
  - [ ] `psn00bsdk_add_executable(retro_engine_ps1 STATIC main_ps1.cpp)` + link `retro_engine`
  - [ ] `psn00bsdk_add_cd_image(retro_engine_disc retro-engine ${CMAKE_CURRENT_LIST_DIR}/cmake/ps1-disc.xml DEPENDS retro_engine_ps1)`
  - [ ] Ожидаемые артефакты: `retro_engine_ps1.elf/.exe/.map`; `retro-engine.bin/.cue` (mkpsxiso)
- [ ] [B] `cmake/ps1-disc.xml`: system.cnf + сгенерированный `retro_engine_ps1.exe` + `assets/build/*.mesh/.tex` в ISO9660-дереве (точные имена/case — release-contract данные)
- [ ] [B] «Hello World» PS1: компилируется через `cmake --preset ps1-release && cmake --build --preset ps1-release`; запускается в DuckStation (`.cue`)

**Неделя 26 — Платформа PS1:**
- [ ] [B] `src/hal/ps1/platform_ps1.cpp`:
  - [ ] Platform_Init: `ResetGraph(0)`, `InitGeom()`, `SetGeomOffset(160,120)`, `SetGeomScreen(160)`
  - [ ] Display/Draw environments и OT инициализируются в Render_Init (НЕ в Platform_Init)
  - [ ] CD и pad — инициализируются своими сервисами
  - [ ] Platform_Shutdown: освобождение engine-owned ресурсов
- [ ] [B] Platform_PumpEvents: pad-поллинг; SwapBuffers: VSync/draw-sync
- [ ] [B] Тест: бинарник не крашится в DuckStation ≥ 30 c (ручной)

**Неделя 27 — GTE-адаптер + OT:**
- [ ] [B] GTE adapter (структура `GteTransform`): engine Mat4/fixed_t → GTE-native rotation (Mat3), translation, ofx/ofy, h; **запрещён raw cast Mat4*→GTE**
  - [ ] `backend_convert_vertex_to_gte` (Vec3 → SVECTOR)
  - [ ] `gte_transform_triangle`: ldv3 → rtpt → stsxy3 → stsz3
  - [ ] NCLIP: signed screen-space area → backface reject (area ≤ 0 → culled, не ошибка); **clip во view-space выполняется ДО projection**
  - [ ] Depth: `backend_map_depth_to_ot(z0,z1,z2)` → bounded OT bucket; вне диапазона — reject/clamp, **modulo запрещён**; equal-depth порядок сохраняет submission_index
- [ ] [B] `clip_triangle_frustum_viewspace` (finite near/far clipping до projected-координат):
  - [ ] ClippedPolygon v[6]; fan-триангуляция (0,i,i+1); интерполяция position/UV/color/normal для сгенерированных вершин
- [ ] [B] `render_draw_triangle_ps1`: POLY_GT3 (Gouraud+текстура), setTexPage/setClut, упаковка x/y/u/v/rgb → `backend_ot_add(ot, depth, &packet)`; **overflow OT/command-buffer = false + диагностика**
- [ ] [B] Текстуры в VRAM: DMA-загрузка CLUT+тайл-данных (psxgpu); `backend_texture_page/clut` маппинг

**Неделя 28 — Ввод + ФС PS1:**
- [ ] [B] `input_pad.cpp`: DualShock через PSn00bSDK pad-драйвер; стики −128..127; маппинг в PadButton-enum (единый с PC)
- [ ] [B] `filesystem_cdrom.cpp`: `FileSystem_LoadFile` по ISO9660-пути на диске (host-пути не входят в PS1-контракт)
- [ ] [B] CD staging буфер 64 KiB (engine budget); асинхронность через PSn00bSDK CD transfer/IRQ API
- [ ] [B] `.github/workflows/build-ps1.yml`:
  - [ ] ubuntu; скачать pinned `PSn00bSDK-0.24-Linux.zip` + `ci/psn00bsdk-v0.24-linux.sha256` (sha256sum -c)
  - [ ] Автопоиск `$PSN00BSDK_LIBS/cmake/sdk.cmake` → GITHUB_ENV/GITHUB_PATH
  - [ ] Configure/build ps1-release; upload-artifact `build/ps1-release/`
- [ ] [B] CHANGELOG + PR

## Критерии приёмки спринта 13–14

- [ ] PS1-бинарник запускается в DuckStation; pad-ввод работает; треугольник рисуется через GTE+OT
- [ ] Текстуры загружаются в VRAM (DMA); ФС читает .mesh/.tex с диска-образа
- [ ] CI: PS1-кросс-сборка зелёная с pinned SDK + checksum

## Риски

Сложность PSn00bSDK ⇒ читать doc/examples, Discord homebrew; GTE неточности ⇒ сверка с PC-результатами fixed-point.
