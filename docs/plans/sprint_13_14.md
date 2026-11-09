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
- [x] [B] `src/hal/ps1/CMakeLists.txt` (фактические имена целей: `retro_ps1`/`retro_disc`):
  - [x] `psn00bsdk_add_executable(retro_ps1 GPREL main_ps1.cpp)` + link `retro_engine`; ручная ld-группа `--start-group/--end-group` на ретро+SDK-архивах (ордер статических библиотек)
  - [x] `psn00bsdk_add_cd_image(retro_disc RETRO cmake/ps1-disc.xml DEPENDS retro_ps1 system.cnf ассеты)`
  - [x] Артефакты: `retro_ps1.elf/.exe/.map`; `RETRO.bin/.cue` (mkpsxiso) — собраны
- [x] [B] `cmake/ps1-disc.xml`: system.cnf + сгенерированный `retro_ps1.exe` + `assets/build/triangle.mesh`/`texture.tex` в ISO9660-дереве (`TRIANGLE.MSH`/`TEXTURE.TEX`, 8.3)
- [x] [B] «Hello World» PS1: компилируется через `cmake --preset ps1-release && cmake --build --preset ps1-release` — **да, зелёно**; запуск в DuckStation (`.cue`) — выполнен ручным прогоном S15–16 (треугольник отображается, PerfMon ≈ 59.8 FPS, эмуляция 100 %)

**Неделя 26 — Платформа PS1:**
- [x] [B] `src/hal/ps1/platform_ps1.cpp`:
  - [x] Platform_Init: `ResetGraph(0)`, `InitGeom()`; `SetGeomOffset(160,120)`/`SetGeomScreen(160)` — в GTE-адаптере (Render_Init-путь)
  - [x] Display/Draw environments и OT инициализируются в Render_Init (НЕ в Platform_Init)
  - [x] CD и pad — инициализируются своими сервисами (hooks `hal_ps1_fs_init`/`hal_ps1_input_init`)
  - [x] Platform_Shutdown: освобождение engine-owned ресурсов (StopPAD)
- [x] [B] Platform_PumpEvents: pad-поллинг (драйвер BIOS, vblank-IRQ); SwapBuffers: DrawSync(0) → VSync(0) → present/flip
- [x] [B] Тест: бинарник не крашится в DuckStation ≥ 30 c (ручной) — покрыт ручным прогоном S15–16 (60 с, VPS ≈ 59.8)

**Неделя 27 — GTE-адаптер + OT:**
- [x] [B] GTE adapter: engine Mat4/fixed_t → GTE-native (pose normalizer D=diag(1,−1,−1), rotation=identity, translation=D·Centroid·k через `gte_ldtr`), ofx/ofy, h; raw cast Mat4*→GTE отсутствует
  - [x] `backend_convert_vertex_to_gte` (Vec3 → SVECTOR; дельты v−Centroid, int16)
  - [x] `gte_transform_triangle`: ldv3 → rtpt → stsxy3 → stsz3 (`hal_ps1_adapter_transform`)
  - [x] NCLIP: signed screen-space area → backface reject (area ≤ 0 → culled, не ошибка; pool_rollback); предельные проверки view-space — ДО projection (явный FAIL, clamp запрещён)
  - [x] Depth: bounded OT bucket (k_min/k_max fail-fast), **modulo отсутствует**; equal-depth — tail-append сохраняет submission_index
- [ ] [B] `clip_triangle_frustum_viewspace` (finite near/far clipping до projected-координат) — **post-MVP**: в MVP вместо fan-клиппинга per-vertex гейты (близ/далёко/экран) → явный FAIL с диагностикой; ClippedPolygon/fan-интерполяция не реализованы
- [x] [B] `render_draw_triangle_ps1`: POLY_GT3 (Gouraud+текстура)/POLY_G3, setTexPage/setClut, упаковка x/y/u/v/rgb → OT tail-append; **overflow OT/command-buffer = false + диагностика**
- [x] [B] Текстуры в VRAM: DMA-загрузка CLUT+тайл-данных (LoadImage); `backend_texture_page/clut` маппинг (8bpp pages x=384+128·slot, CLUT y=240+slot)

**Неделя 28 — Ввод + ФС PS1:**
- [x] [B] `input_pad.cpp`: DualShock через PSn00bSDK pad-драйвер (InitPAD/StartPAD, типы 0x4/0x5/0x7); стики −128..127; маппинг в PadButton-enum (единый с PC; SDK-шилд `sdk::` от конфликта имён)
- [x] [B] `filesystem_cdrom.cpp`: `FileSystem_LoadFile` по ISO9660-пути на диске (CdSearchFile → CdlSetloc → CdRead/CdReadSync; host-пути не входят в PS1-контракт)
- [ ] [B] CD staging буфер 64 KiB + асинхронность через CD transfer/IRQ API — **post-MVP**: MVP читает синхронно в malloc(round_up_2048) по точному размеру файла
- [x] [B] `.github/workflows/build-ps1.yml`:
  - [x] ubuntu; скачать pinned `PSn00bSDK-0.24-Linux.zip` + `ci/psn00bsdk-v0.24-linux.sha256` (sha256sum -c; хэш сверен фактическим скачиванием)
  - [x] `PSN00BSDK_LIBS=$HOME/PSn00bSDK/lib/libpsn00b` → GITHUB_ENV (sdk.cmake подхватывается пресетом)
  - [x] Configure/build ps1-release; проверка артефактов; upload-artifact `RETRO.bin/.cue` + `retro_ps1.exe`
- [x] [B] CHANGELOG — обновлён; PR — после ревью/коммита *(PR #1 смёржен в `main`, merge-коммит `abc5e454`)*

## Критерии приёмки спринта 13–14

- [x] PS1-бинарник запускается в DuckStation; pad-ввод работает; треугольник рисуется через GTE+OT — ручная приёмка закрыта прогоном S15–16 (см. CHANGELOG)
- [x] Текстуры загружаются в VRAM (DMA); ФС читает .mesh/.tex с диска-образа — `TRIANGLE.MSH`/`TEXTURE.TEX` прочитаны с диска (приёмка S15–16)
- [ ] CI: PS1-кросс-сборка зелёная с pinned SDK + checksum (локально — зелёно; GitHub-run — после push)

## Риски

Сложность PSn00bSDK ⇒ читать doc/examples, Discord homebrew; GTE неточности ⇒ сверка с PC-результатами fixed-point.
