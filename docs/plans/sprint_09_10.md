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
- [ ] [B] `tools/asset_converter/CMakeLists.txt`: `add_executable(asset_converter main.cpp mesh_converter.cpp texture_converter.cpp color_quantizer.cpp)`, `cxx_std_14`, include third_party (cgltf/stb)
- [ ] [B] CLI: `asset_converter <input> <output>`; коды возврата ошибок
- [ ] [B] Проверить сборку в PC-конфигурации; **убедиться, что тулза НЕ собирается в ps1-release**

**Неделя 18 — .mesh v1:**
- [ ] [B] Формат (little-endian, без ABI-padding):
  - [ ] Header 24 байта: magic `0x4853454D`, version=1, vertex_count, index_count (кратно 3), texture_id, bounding_sphere_radius (i32 16.16)
  - [ ] Vertex 21 байт: i32 x,y,z (16.16); i8 nx,ny,nz (−127..127); u8 u,v (0..255); u8 r,g,b,a
  - [ ] Index 6 байт: u16 v0,v1,v2
- [ ] [B] `mesh_converter.cpp`:
  - [ ] cgltf-парсинг (примитивы, позиции/нормали/UV/цвета)
  - [ ] float → fixed_t 16.16 (fixed_from_float), нормали float→i8 (сатурация ±127), UV→u8
  - [ ] Запись LE побайтово (никаких кастов struct→bytes)
- [ ] [B] Тест: куб из Blender → cube.mesh → обратная проверка полей (самопроверка читателем из теста)

**Неделя 19 — .tex v1 + квантизатор:**
- [ ] [B] Формат: Header 20 байт: magic `0x00584554` («TEX\0»), version=1, width/height u16 (64/128/256), palette_offset, pixel_offset; payload: 512 байт палитра (256×LE u16 RGB555+STP) + width×height байт CI8; без overlap; проверки uint64 на width*height и offsets
- [ ] [B] `color_quantizer.cpp`: Median Cut → 256 цветов (fallback Octree если медленно)
- [ ] [B] `texture_converter.cpp`: stb_image → индексированное изображение + палитра в RGB555+STP (bit layout по контракту); entry 0 — политика резервирования (например, transparent black) — зафиксировать
- [ ] [B] Тест: PNG 256×256 → texture.tex → palette-валидация bit layout

**Неделя 20 — Интеграция + валидация загрузчика:**
- [ ] [B] Корневой PC CMake: `add_custom_command` → конвертация `assets/source/cube.gltf`/`texture.png` → `assets/build/cube.mesh`/`texture.tex`; `add_custom_target(convert_assets ALL)`; выходы — детерминированы
- [ ] [B] Политика ассетов: `assets/build/` коммитится как versioned-ассеты для PS1-checkout (или регенерируется PC-сборкой); release job обязан валидировать формат
- [ ] [B] `src/assets/mesh_loader.cpp` — `mesh_validate_and_load(data, size, alloc, out)`:
  - [ ] Порядок проверок (обязателен): (1) file_size ≥ header_size; (2) magic/version; (3) count/offset-арифметика в uint64_t; (4) payload_end ≤ file_size; (5) каждый index < vertex_count; (6) только затем persistent allocation; (7) rollback арены при любой пост-аллокационной ошибке
  - [ ] Никаких кастов file.data → Header/Vertex; только явное LE-декодирование
  - [ ] `texture_loader.cpp` — аналогично для .tex (перекрытие payloads, невалидные width/height, обрезанные CI8)
- [ ] [B] Тесты `tests/integration/mesh_loader_test.cpp` + `texture_loader_test.cpp` (кейсы: truncated header; overflow-sized counts; out-of-range indices; overlapping .tex payloads; invalid width/height; malformed offsets; **проверка, что persistent arena usage НЕ изменился при фейле**)
- [ ] [B] `docs/ASSETS.md` (формат байтово, CLI конвертера, пайплайн)
- [ ] [B] CHANGELOG + PR

## Критерии приёмки спринта 9–10

- [ ] glTF→.mesh (куб Blender) и PNG→.tex (256×256) работают
- [ ] Median Cut 256 цветов; палитра RGB555+STP корректна (тест)
- [ ] CMake auto-конвертация на PC; PS1-сборка потребляет готовые ассеты
- [ ] Все malformed-кейсы отвергаются безопасно, арена не расходуется
