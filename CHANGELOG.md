# Changelog

Все заметные изменения этого проекта документируются в этом файле в стиле [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/); версионирование — [SemVer](https://semver.org/lang/ru/). Правило: обновление CHANGELOG — на каждом sprint review (docs/DEVELOPMENT_PLAN.md §1, п. 2).

## [Unreleased]

### Added

- Спринт 9–10 — конвертер ассетов (недели 17–20, M3): (1) host-only CLI-тулза
  `tools/asset_converter` — glTF 2.0 → `.mesh` v1 (cgltf-вендор: world-позиции
  в fixed 16.16 через `fixed_from_float` (усечение), нормали → i8 с сатурацией
  ±127, UV/цвет → u8 half-up, radius по сериализованным позициям (знаковое
  чтение i32), flat-нормали при отсутствии NORMAL; только TRIANGLES,
  ≤65535 вершин) и PNG/JPG/TGA/BMP → `.tex` v1 (stb_image → RGBA8 → Median
  Cut ≤255 цветов → палитра RGB555+STP по bit layout §1.2.5 (8→5 бит — `>>3`),
  политика слота 0: reserved «transparent black» `0x0000`, CI8-индекс 0 никогда
  не входит в пиксели, STP=0 во всех слотах — MVP-прозрачность выключена);
  сериализация обоих форматов побайтовая LE (assets_read/write_le_u16/u32,
  никаких struct→bytes кастов), детерминированная (повторный прогон —
  байт-идентичный файл); (2) безопасные загрузчики движка `src/assets/` —
  `mesh_validate_and_load` / `texture_validate_and_load` по контракту §1.2.7:
  7-шаговая валидация в заданном порядке, uint64-арифметика ДО size_t,
  аллокация — только после валидации, rollback арены при любой
  пост-аллокационной ошибке (malformed-вход расходует persistent-память
  0 байт), 10 диагностических кодов `AssetDiag`; (3) интеграционные тесты
  `tests/integration/mesh_loader_test.cpp`/`texture_loader_test.cpp` —
  malformed-кейсы (truncated header (включая нулевую длину среза),
  overflow counts, out-of-range indices, overlapping payloads, неверные
  dims/offsets) с проверкой, что арена не изменилась; кросс-проверка
  палитры против движкового `pack_rgb555_stp`; тесты real-ассетов пайплайна
  (canonical cube: 24 верш/12 треуг/radius 113511; texture 256×256, слот 0 =
  0x0000, пиксели ≥1) — читают `assets/build/*` по compile-time
  `RPE_TEST_SOURCE_DIR`; (4) CMake-пайплайн: `add_custom_command` +
  `convert_assets ALL` (только в PC-конфигурации; ps1-release тулзу не
  собирает — потребляет закоммиченные versioned-ассеты; локальная проверка
  структурная — PS1-тулчейн в песочнице недоступен); сгенерированные артефакты
  `assets/build/cube.mesh` (600 байт) и `assets/build/texture.tex` (66068
  байт) + генератор источников `scripts/make_source_assets.py` (детерминированный
  glTF-куб 24 вершины/12 треугольников; PNG 256×256 из 65536 уникальных цветов);
  (5) документация `docs/ASSETS.md` — байтовые форматы v1, политика решений
  MVP (index_count = число 6-байтовых записей MeshIndex; Octree-фоллбек
  Median Cut не реализован), коды возврата CLI 0..7, коды загрузчиков
  0..9.

- Спринт 7–8, недели 15–16 — закрывающий хвост (M2) — числовой конформанс +
  инвентарь draw + CI-рендер-гейт: (1) `tests/visual/render_golden_test.cpp` —
  `W0Golden.AffineNumericConformanceOnSlantedPlane` — программная реплика
  GPU-пайплайна (`model_clip_from_view` → 1/16-квантование → барицентрика по
  центрам пикселей → nearest-семпл → Gouraud-модуляция) против фактического
  readback на НАКЛОННОЙ floor-плоскости (z −5..−20, w 10→25) с двуцветной
  шов-текстурой (`w0_make_seam_pixels`, affine ≠ perspective различаются
  палитровым швом): guarded=1605 / mismatched=0 / informative=133 / persp_hits=0
  — «плывут» подтверждаются ровно affine'ом (замена ручного визуального
  пункта приёмки); (2) `hal_render_backend_draw_call_count()` — программный
  инвентарь draw-команд кадра (1 треугольник = 1 draw + 1 present quad; сброс
  в BeginFrame) — замена ручной RenderDoc-верификации (тест-инвентарь:
  комната = 13, единичный tri = 2, полный reject = 1); (3) CI: job
  `pc-render-tests` в `tests.yml` — первый в истории прогон `pc-debug-render`
  (W0-golden всего дерева 143 тестов) под `xvfb-run -a` на GitHub-runner
  (закрывает задачу «xvfb-прогонка pc-debug-render в CI», до сих пор не
  выполнявшуюся — W0-golden ранее вообще не запускался в CI); (4) фикс:
  эталоны golden-теста резолвились через getenv (иначе — зависимость от CWD
  запущенного процесса) — теперь compile-time `RPE_TEST_SOURCE_DIR`
  (CWD-зависимость устранена; CI-runnable из любого каталога); (5)
  `tools/windows/setup-windows.ps1` — идемпотентная автоматизация §3.1
  Спринта 0 (Git/CMake/Ninja/VS2022 BuildTools/MSYS2/Cursor+7/эмуляторы) с
  критериальными проверками [OK]/[FAIL].

### Changed

- Golden-протокол → v2 (после первого CI-прогона `pc-render-tests`):
  быстрый путь — hash (7 из 8 эталонов bit-стабильны даже на
  runner-llvmpipe); для `Room12TexturedTriangles` — фолбэк по сырым
  15 битам против px-партнёра эталона (`w0_room12.bin.px`) с бюджетом
  128 px и капом ±2 LSB канала — хэш-эталон байт-в-байт не изменён
  (`a7dce7faeadb1b7f`); причина — rasterizer tie-breaking на силуэтных
  рёбрах (замер: 26 px из 76 800, все ±1 LSB, локальный драйвер vs
  llvmpipe; GL не специфицирует принадлежность пикселя на ребре).
- `.clang-format` согласован с фактическим стилем кода (BraceWrapping
  определений, `SortIncludes: Never`, `AlignTrailingComments: Leave`,
  касты без пробела): дерево проекта проверяется формальнее —
  `clang-format --dry-run --Werror` чистый по `src/ tests/ game/
  tools/asset_converter`; прогон форматтера (v23) применён к файлам —
  прямое закрытие «прогон clang-format» из Спринта 1–2.
- `docs/STYLE_GUIDE.md` §2: определения (typedef/struct/enum/функции) —
  скобка с переносом, стейтменты — attach; include-порядок — курируемый;
  хвостовые комментарии — рукописные выравнивания.

### Added

- Спринт 7–8, Недели 13–15 (M2): ядро рендера с эмуляцией PS1 — `src/render/renderer.h/.cpp` (`Render_Init` — ровно один до первого `BeginFrame` (Lifecycle-contract: debug assert / release-diag), `Render_Shutdown/BeginFrame/EndFrame/Clear/BindTexture`, `RENDER_CLEAR_DEFAULT (0,0,32)`), `src/render/command_buffer.h` (фиксированный 64 KiB, overflow = явный отказ, без silent-drop), `src/render/sort_key.h` (SortRef + `sort_ref_before` — far→near с tie-breaker `submission_index`), `src/render/mesh.h` (Vertex/Triangle/MeshReader-заготовка), `src/render/texture.h/.cpp` (`pack_rgb555_stp` R[4:0]/G[9:5]/B[14:10]/STP[15]; пул CI8-текстур на static-арене 128 KiB/16 слотов, `RENDER_TEXTURE_MAX_DIM`=256); GL3.3-бэкенд `src/hal/pc/render_opengl.cpp` (reference FBO 320×240 RGBA8 без depth-attachment, `glDisable(GL_DEPTH_TEST/BLEND/CULL_FACE)` — backface-куллинг зона ядра; компиляция/линковка шейдеров с журналированием через core/log; динамический VBO per-triangle + static quad выкладки nearest). Шейдеры `src/render/shaders/ps1_emu.{vert,frag}` (affine UV `noperspective`, квантование 1/16 px 320×240 — wobble heuristic; CI8 → палитра 256×1 RGB555→RGB8 честный (c<<3)|(c>>2), Gouraud) и `ps1_present.{vert,frag}` (nearest-выкладка на окно); генерация `shaders_embedded.h` — `cmake/EmbedShaders.cmake` (C-escaping, `CMAKE_CONFIGURE_DEPENDS` на *.vert/*.frag); шейдерные qualifier'ы вынесены 1:1 vert↔frag. Пул текстур ядра ↔ GPU-клоны через `hal_render_backend_create_texture(handle,w,h,pixels,palette)` (HAL не ходит в пул ядра; слоёные статические библиотеки `retro_engine_log` → `retro_engine_hal` → `retro_engine`).
- Спринт 7–8, Неделя 16 (M2): `Render_SetCamera(view,proj)` (вывод near/far из projection валидатором — мусорная матрица отказывается детерминированно), `Render_DrawMesh` (гейты: Transform→Clip(whole-triangle near/far)→Cull(A<0 SY-DOWN)→depth_key→PACK; `model=NULL` — явный NULL_ARGUMENT, «NULL=identity» нет), Painter's algorithm — стабильная вставочная сортировка SortRef (без `<algorithm>` — freestanding PS1), `depth_key_from_camera_z` (-1→REJECT, 4096→REJECT, 1024 ok — §1.2.6 1:1). W0-golden-конформанс `tests/visual/` (пресет `pc-debug-render`, gate `BUILD_RENDER_INTEGRATION_TESTS`): сцена 320×240 — комната из 12 CI8/Gouraud треугольников перед фиксированной камерой (12 подано/0 reject), 5 reject-кейсов (visible/backface/near/far/behind-camera), lifecycle-контракты; метод сравнения — framebuffer readback + FNV-1a 64 над 15-битным сжатием RGBA8 (`Render_TestReadbackHash`), эталоны `tests/visual/reference/*.bin` в репо (8-байтовые big-endian; regen — только `RENDER_GOLDEN_REBASE=1`). Каркас метаданных окружения `docs/benchmarks/reference_environment.yaml` (os/cpu/gpu/driver/compiler/build_type/resolutions/vsync/power_mode/measurement_seconds/git_revision/asset_set_sha256). Итог: 141 тест — 130 юнит (pc-debug/pc-release/pc-asan, 0 findings) + 11 visual/lifecycle (pc-debug-render); `ps1-release` ELF собирается.

- Спринт 5–6, Недели 9–12 (M2): HAL PC на SDL3 — `src/hal/platform.h` (единый C-API из §3 нормативной документации: `Platform_*` / `Render_*` / `Input_*` / `Audio_*` / `FileSystem_*`; Render_* объявлены контрактно, реализация — Спринты 7–8); `platform_pc.cpp` (SDL3: `SDL_INIT_VIDEO|SDL_INIT_GAMEPAD` без SDL_INIT_TIMER; GL 3.3 Core-атрибуты до создания окна; окно 1280×960; `CreateContext → MakeCurrent → gladLoadGL` до любых GL-вызовов; полный откат при частичном сбое); `input_sdl.cpp` (строго SDL_Gamepad API — SDL_GetGamepads/SDL_OpenGamepad/SDL_GetGamepadButton/SDL_GetGamepadAxis; L2/R2 по порогу 50% аналоговой оси; клавиатурная эмуляция цифрового PS1-пада для pad 0: WASD/стрелки → D-pad, SPACE → Крестик, J/K/L → Круг/Квадрат/Треугольник, U/I → L1/R1, Enter/Tab → Start/Select; оси −128..127 детерминированным целочисленным raw/256); `audio_sdl.cpp` (SDL3 device-stream mono 22.05 кГц U8, квадратный beep через атомики — главный поток не блокируется; полный PCM — post-MVP); `filesystem_pc.cpp` (`SDL_LoadFile`/`SDL_free`; отказы — {nullptr, 0} без краша); `render_opengl.cpp` (GL-swap; расширение backend'ом — Спринты 7–8).
- Спринт 5–6: интеграционные тесты HAL `tests/integration/hal_test.cpp` (Init/Shutdown с реальным окном 1280×960; delta time Sleep(100) ∈ [95..150] мс; input валидация out-of-range; аудио-жизненный цикл; ФС валидный/несуществующий/пустой файл) — 96 юнит-тестов (88 прежних + 8 HAL), 100% pass в `pc-debug`/`pc-release`/`pc-asan` (LSan-суппрессии системного Mesa-драйвера — `ci/lsan_suppressions.supp`, утечки вне движка).
- Спринт 5–6: CI `build-pc.yml` — матрица pc-release на ubuntu/windows/macos (линт ninja/X11-dev установок по ОС, verify_vendored на всех трёх); `.gitattributes` (LF для исходников/конфигов, binary для ассетов/субмодулей).

- Спринт 0 (M0: Инфраструктура): каркас репозитория, канонические CMake-пресеты (`pc-debug`, `pc-release`, `pc-debug-render`, `ps1-release`), вендоринг зависимостей, первый CI-контур.
- Консолидация документации в `docs/` (инженерная спецификация v1.43 + план разработки + планы спринтов).
- Спринт 1–2, Неделя 1 (M1): `docs/STYLE_GUIDE.md`, `.clang-format` (4 пробела, ≤100 симв., 100 колонок), шаблон PR с чек-листом DoD (§17).
- Спринт 1–2, Неделя 2 (M1): ядро fixed-point 16.16 — `src/core/math/fixed.h/.cpp` (`fixed_clamp_i64/from_int/from_float/to_int/to_float/add/sub/mul/div`, `fixed_sqrt` — детерминированный integer isqrt); юнит-тесты `tests/math/fixed_test.cpp` (19 кейсов, 100% pass rate, debug+release); GoogleTest подключён канонически (`tests/CMakeLists.txt`), корневой `enable_testing()` (CTest-агрегация), заглушка `core/placeholder.cpp` удалена.
- Спринт 1–2, Неделя 3 (M1): `src/core/math/vec2.h/vec3.h/vec4.h` (make/add/sub/scale/dot; `vec3_cross`, `vec3_length_sq`; `fixed_reduce3_products` — полный int64-аккумулятор, clamp ровно один раз; насыщающий 64-битный аккумулятор `fixed_sat_add_i64` против UB-переполнения редукций). Non-inline `vec3_length`/`vec3_normalize` — `src/core/math/math.cpp`. Тесты `tests/math/{vec2,vec3,vec4}_test.cpp` (ReductionUsesSingleFinalClamp на экстремальных raw-векторах, Cross на ортах, Determinism).
- Спринт 1–2, Неделя 4 (M1): Mat3/Mat4 (column-major, §4.3) — identity/multiply (aliasing-safe, int64-аккумуляторы)/translate/rotate_x/y-z/perspective/look_at/transform; Mat3 (rotation-подмножество для GTE); детерминированная тригонометрия `sin_fixed`/`cos_fixed` (полином Тейлора после целочисленного приведения, без float — freestanding PS1-совместим). Тесты `tests/math/mat4_test.cpp` (оси 90°, NDC ±1, look_at, побитные golden-векторы) — итого 49 юнит-тестов, 100% pass (debug+release). Бенчмарк-заготовка `tests/benchmark/math_benchmark.cpp` (включается после вендоринга google/benchmark, пресет pc-bench).
- Спринт 3–4, Неделя 5 (M1): `src/core/memory/linear_allocator.h` — bump-аллокатор с overflow-safe проверками против `remaining`, выравниванием фактического адреса, `mark/rollback/reset/used/remaining`; тесты `tests/memory/linear_allocator_test.cpp` (unaligned arena base, foreign/future markers, OOM без изменения состояния); baseline-бенчмарк `tests/benchmark/memory_benchmark.cpp` и протокол `docs/benchmarks/memory_baseline.md` (медиана `allocate(16,8)` ≈ 2.0 нс на i5-11300H).
- Спринт 3–4, Неделя 6 (M1): `src/core/memory/pool_allocator.h` — `Pool<T, N>` с free-list `uint16_t`, валидацией foreign/interior/double-free без UB, `for_each`; тесты `tests/memory/pool_allocator_test.cpp` (reuse, OOM, итерация, 256 объектов + 10 000 циклов alloc/free); AddressSanitizer-прогон — 0 findings.
- Спринт 3–4, Неделя 7 (M1): `src/core/memory/stack_allocator.h` — `push/mark/pop/reset_frame` с overflow-safe проверками, owner/bounds-check marker и выравниванием адреса; тесты `tests/memory/stack_allocator_test.cpp` (nested frames, foreign/future markers, unaligned base); интеграционный сценарий загрузки уровня тремя аллокаторами `tests/memory/allocator_integration_test.cpp` (static-арены 448 KiB/64 KiB, не на CPU-стеке).
- Спринт 3–4, Неделя 8 (M1): ECS — `src/core/ecs/entity.h` (24-bit index + 8-bit generation, `INVALID_ENTITY=0`), `src/core/ecs/registry.h/.cpp`: 7 MVP-пулов (Position/Rotation/Velocity 1024, MeshRenderer/Collider 256, Camera/PlayerControl 8), dense ComponentPool с swap-back, generation-increment и retirement слота при 255, `Registry::view<...>()` с кэшированием прямых dense-указателей; `sizeof(Registry)=239 648 B` (≤ 256 KiB budget). Тесты `tests/ecs/entity_test.cpp`, `tests/ecs/registry_test.cpp` (stale handle, generation wrap, dense order, 10 000 сущностей) — итого 88 юнит-тестов, 100% pass (pc-debug, pc-release, pc-asan); `ps1-release` ELF собирается.
