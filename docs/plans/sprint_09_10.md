# Спринт 9–10 — Конвертер ассетов (недели 17–20)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§4).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 9–10 |
| Недели | 17–20 — 01.03.2027 – 28.03.2027 |
| Веха | M3 — Пайплайн ассетов |
| Цель | C++14 утилита: glTF→.mesh v1, PNG→.tex v1 (CI8 + RGB555+STP), интеграция с CMake |
| Входные критерии | Спринт 7–8 завершён (рендерер PC); каркас PC-сборки из Спринта 0 |
| На критическом пути | Да |
| Выходные артефакты | `tools/asset_converter/`, форматы `.mesh v1` / `.tex v1`, `mesh_loader.cpp`, `texture_loader.cpp`, `docs/ASSETS.md`, malformed-тесты |
| Следующий спринт | [`sprint_11_12.md`](sprint_11_12.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 30 | 42 | 60 | 43.0 | 5.0 | 53.0 | 40–60 ч | Укладывается |

## Задачи

**Неделя 17 — Каркас тулзы:**
- [x] [B] `tools/asset_converter/CMakeLists.txt`: `add_executable(asset_converter main.cpp mesh_converter.cpp texture_converter.cpp color_quantizer.cpp)`, `cxx_std_14`, include third_party (cgltf/stb)
- [x] [B] CLI: `asset_converter <input> <output>`; коды возврата ошибок
- [x] [B] Проверить сборку в PC-конфигурации; **убедиться, что тулза НЕ собирается в ps1-release** *(структурно: `add_subdirectory(tools/asset_converter)` внутри `if(PLATFORM_PC)`; configure ps1-release падает на отсутствии `/cmake/sdk.cmake` до подкаталогов — PS1-тулчейн недоступен локально)*

**Неделя 18 — .mesh v1:**
- [x] [B] Формат (little-endian, без ABI-padding):
  - [x] Header 24 байта: magic `0x4853454D`, version=1, vertex_count, index_count (кратно 3), texture_id, bounding_sphere_radius (i32 16.16)
  - [x] Vertex 21 байт: i32 x,y,z (16.16); i8 nx,ny,nz (−127..127); u8 u,v (0..255); u8 r,g,b,a
  - [x] Index 6 байт: u16 v0,v1,v2
- [x] [B] `mesh_converter.cpp`:
  - [x] cgltf-парсинг (примитивы, позиции/нормали/UV/цвета)
  - [x] float → fixed_t 16.16 (fixed_from_float), нормали float→i8 (сатурация ±127), UV→u8
  - [x] Запись LE побайтово (никаких кастов struct→bytes)
- [x] [B] Тест: куб из Blender → cube.mesh → обратная проверка полей (самопроверка читателем из теста) *(источник-куб — детерминированный генератор `scripts/make_source_assets.py`; 24 вершины/12 треугольников)*

**Неделя 19 — .tex v1 + квантизатор:**
- [x] [B] Формат: Header 20 байт: magic `0x00584554` («TEX\0»), version=1, width/height u16 (64/128/256), palette_offset, pixel_offset; payload: 512 байт палитра (256×LE u16 RGB555+STP) + width×height байт CI8; без overlap; проверки uint64 на width*height и offsets
- [x] [B] `color_quantizer.cpp`: Median Cut → 256 цветов (fallback Octree если медленно) *(Octree-фоллбек не реализован — Median Cut детерминирован и достаточен на ≤65536 пикселей; решение зафиксировано в docs/ASSETS.md §6)*
- [x] [B] `texture_converter.cpp`: stb_image → индексированное изображение + палитра в RGB555+STP (bit layout по контракту); entry 0 — политика резервирования (например, transparent black) — зафиксировать *(слот 0 = reserved «transparent black» 0x0000; CI8-индекс 0 не входит в пиксели; STP=0 во всех слотах — docs/ASSETS.md §3.2)*
- [x] [B] Тест: PNG 256×256 → texture.tex → palette-валидация bit layout *(включая кросс-проверку против движкового `pack_rgb555_stp`)*

**Неделя 20 — Интеграция + валидация загрузчика:**
- [x] [B] Корневой PC CMake: `add_custom_command` → конвертация `assets/source/cube.gltf`/`texture.png` → `assets/build/cube.mesh`/`texture.tex`; `add_custom_target(convert_assets ALL)`; выходы — детерминированы *(повторный прогон — byte-identical: cmp/sha256)*
- [x] [B] Политика ассетов: `assets/build/` коммитится как versioned-ассеты для PS1-checkout (или регенерируется PC-сборкой); release job обязан валидировать формат *(артефакты коммитятся; загрузчики-тесты валидируют формат на каждом прогоне ctest; выделенный release job — CI-повестка следующих спринтов)*
- [x] [B] `src/assets/mesh_loader.cpp` — `mesh_validate_and_load(data, size, alloc, out)`:
  - [x] Порядок проверок (обязателен): (1) file_size ≥ header_size; (2) magic/version; (3) count/offset-арифметика в uint64_t; (4) payload_end ≤ file_size; (5) каждый index < vertex_count; (6) только затем persistent allocation; (7) rollback арены при любой пост-аллокационной ошибке
  - [x] Никаких кастов file.data → Header/Vertex; только явное LE-декодирование
  - [x] `texture_loader.cpp` — аналогично для .tex (перекрытие payloads, невалидные width/height, обрезанные CI8)
- [x] [B] Тесты `tests/integration/mesh_loader_test.cpp` + `texture_loader_test.cpp` (кейсы: truncated header; overflow-sized counts; out-of-range indices; overlapping .tex payloads; invalid width/height; malformed offsets; **проверка, что persistent arena usage НЕ изменился при фейле**) *(+ real-ассеты пайплайна; нулевая длина среза для truncated-header — через ненулевой probe-буфер с явным размером)*
- [x] [B] `docs/ASSETS.md` (формат байтово, CLI конвертера, пайплайн) *(+ политики решений MVP: index_count-семантика, слот 0, texture_id)*
- [x] [B] CHANGELOG + PR *(CHANGELOG готов; PR-интерфейс недоступен (`gh` CLI нет) — merge-повестка выполняется локально с сохранением истории, как в Спринтах 3–6)*

## Критерии приёмки спринта 9–10

- [x] glTF→.mesh (куб Blender) и PNG→.tex (256×256) работают
- [x] Median Cut 256 цветов; палитра RGB555+STP корректна (тест)
- [x] CMake auto-конвертация на PC; PS1-сборка потребляет готовые ассеты
- [x] Все malformed-кейсы отвергаются безопасно, арена не расходуется
