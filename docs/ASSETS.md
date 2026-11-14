# Ассеты RetroPolyEngine — форматы `.mesh` / `.tex` и конвертер

> Статус: финализирован для MVP v0.1.0.
> Нормативные источники: [`RetroEngine_Engineering_Documentation_v1.43.md`](RetroEngine_Engineering_Documentation_v1.43.md)
> §8.1–8.3 (форматы и загрузчик), §1.2.5 (RGB555+STP), §1.2.7 (контракт валидации
> 7 шагов), §4.5 (CMake-интеграция); [`plans/sprint_09_10.md`](plans/sprint_09_10.md),
> [`plans/sprint_19_20.md`](plans/sprint_19_20.md) (финализация).
> Этот документ фиксирует байтовые форматы, политику решений MVP и контракт CLI
> конвертера. Единственный источник истины форматов в коде — [`../include/retro_engine/assets/assets.h`](../include/retro_engine/assets/assets.h).

## 1. Общие правила сериализации

- Всё little-endian **побайтово**: ни один файл не сериализуется кастом
  `struct → bytes` (ABI-padding хоста не попадает в файл). Чтение — через
  `assets_read_le_u16/u32`, запись — `assets_write_le_u16/u32`
  (байт-адресно, независимы от порядка байт процессора).
- Вся арифметика счётчиков и смещений — в `uint64_t` ДО приведения к `size_t`.
- Загрузчики движка читают файл только явным декодированием полей; структура
  `MeshHeader`/`TexHeader` — «раскодированный вид» (не раскладка в файле).

## 2. `.mesh` v1 (glTF → движок)

```
Файл: [Header: 24 байта][MeshVertex × vertex_count][MeshIndex × index_count]

Header:
  0x00 u32 magic      = 0x4853454D  (байты 'M','E','S','H')
  0x04 u32 version    = 1
  0x08 u32 vertex_count             (число вершинных записей)
  0x0C u32 index_count              (число ИНДЕКСНЫХ ЗАПИСЕЙ — по одной на
                                     треугольник; кратен 3 — контракт §8.3)
  0x10 u32 texture_id               (слот рантайма; вне glTF — для v1 0)
  0x14 i32 bounding_sphere_radius   (signed 16.16 engine-space)

MeshVertex (21 байт):
   0..3  i32 x      4..7 i32 y      8..11 i32 z      (signed 16.16)
  12..14 i8 nx,ny,nz                (−127..127)
  15..16 u8 u,v                     (0..255: 0..1 → 255)
  17..20 u8 r,g,b,a

MeshIndex (6 байт): u16 v0, v1, v2
```

### 2.1 Семантика `index_count` (важно)

`index_count` — число 6-байтовых структур `MeshIndex`, т.е. **количество
треугольников**, а не сырых u16-индексов потока. Это соответствует нормативному
коду §8.3: `index_bytes = index_count × 6` и `allocate_array<MeshIndex>(index_count)`.
Правило `index_count % 3 == 0` в загрузчике сохранено дословно из SAMPLE;
для файлового v1-конвертера это избыточно (каждая запись — треугольник),
но нормативное. Размер файла: `24 + 21·vertex_count + 6·index_count`.

### 2.2 Конверсии glTF → v1 (конвертер)

| Атрибут glTF     | Формат финальный                      | Правило округления |
|------------------|---------------------------------------|--------------------|
| POSITION         | `i32` fixed 16.16 через `fixed_from_float` | усечение к нулю (контракт ядра), NaN/Inf → 0, выход за диапазон → ±INT32_MAX |
| NORMAL           | `i8` ±127                             | round-half-away-from-zero с сатурацией ±127 |
| TEXCOORD_0       | `u8` 0..255                           | half-up (`v·255+0.5`), clamp в 0..1 |
| COLOR_0          | `u8` RGBA                             | half-up; VEC3 → альфа 255 |
| (нет NORMAL)     | flat-нормали                          | нормаль грани всем трём вершинам треугольника (по сериализованным позициям) |

Дополнительно:

- Позиции/нормали проходят мир-матрицу узла (`cgltf_node_transform_world`);
  вычисления — в `double`, чтение accessor'ов — float API cgltf.
- Правило MVP: только `mode = TRIANGLES` и индексированные
  примитивы; максимум 65535 вершин на файл (u16-индексация).
- `bounding_sphere_radius` — max `‖p‖` по **сериализованным** позициям
  payload'а (i32 читается как ЗНАКОВОЕ; усечение `fixed_from_float`; для куба
  ±1 это `floor(√3·65536) = 113511`). Без сатурации для обычных ассетов.
- `texture_id` всегда 0: привязка текстуры к мешу — вне glTF (v1).

## 3. `.tex` v1 (изображение → движок)

```
Файл: [Header: 20 байт][палитра: 512 байт][CI8-пиксели: width×height]

Header:
  0x00 u32 magic    = 0x00584554  (байты 'T','E','X','\0')
  0x04 u32 version  = 1
  0x08 u16 width                  (побайтно LE)
  0x0A u16 height
  0x0C u32 palette_offset = 20    (плоская CLUT-схема: сразу за заголовком)
  0x10 u32 pixel_offset   = 532   (20 + 512)
```

### 3.1 Палитра (256 × LE u16, RGB555+STP)

Bit layout §1.2.5: `b15 = STP`, `b14..b10 = B5`, `b9..b5 = G5`, `b4..b0 = R5`.
8→5 бит канала: усечение старших бит (`>>3`). Обратный раскод 5→8:
`(c<<3)|(c>>2)` (круговой trip идемпотентен). Код упаковки — стыкуем
с движковым `pack_rgb555_stp` (`src/render/texture.h`), что проверяется тестом
`TexContract.EngineAndRefPackingAgree`.

### 3.2 Политика слота 0 (фиксируется этим документом)

- Слот 0 — **reserved «transparent black»**: слово `0x0000` (RGB 0,0,0,
  STP = 0).
- CI8-индекс `0` **никогда не входит** в payload пикселей: все пиксели
  принимают значения 1..255.
- Квантование (Median Cut) использует только слоты 1..255 (≤255 цветов);
  при ≤255 уникальных цветах палитра точная. Цвет `(0,0,0)` квантуется в
  ближайший тёмный ненулевой entry.
- `STP = 0` во всех слотах: MVP-прозрачность выключена (§8.2); резерв
  подготовлен к будущей STP-политике без изменения формата.

### 3.3 Ограничения MVP (§8.2)

- `width == height` ∈ {64, 128, 256} (квадратная сторона; любое другое
  значение/пара — отказ).
- Размер пиксельного payload строго `width×height` байт (1 пиксель = 1 индекс).
- Payload'ы не пересекают заголовок и друг друга; проверки — uint64.

## 4. Конвертер `asset_converter`

Host-only инструмент (C++14): `tools/asset_converter/`. Собирается
исключительно в PC-конфигурации (root CMakeLists: `add_subdirectory` внутри
`if(PLATFORM_PC)`) и НИКОГДА — MIPS-тулчейном: PS1-сборка потребляет
заранее сгенерированные `assets/build/*`.

### 4.1 CLI и коды возврата

```
asset_converter <input> <output>
```

- `.gltf`/`.glb` → `.mesh` (glTF 2.0, cgltf-вендор);
- `.png`/`.jpg`/`.jpeg`/`.tga`/`.bmp` → `.tex` (stb_image).

| Код | Смысл |
|-----|-------------|
| 0   | успех |
| 1   | usage: аргументов не 2 |
| 2   | неизвестное расширение входа |
| 3   | вход не открыт/не декодирован (cgltf/stb) |
| 4   | структура входа повреждена (validate/атрибуты/индексы/типы) |
| 5   | вход не поддержан MVP (dims, counts, mode, bounds) |
| 6   | не удалось записать выход (`fopen`/`fwrite`/`fclose`) |
| 7   | нехватка памяти хоста / прочее |

Детерминированность: вывод конвертера не зависит от порядка обхода, seed
нет; повторный прогон над тем же входом — байт-идентичный файл (проверено
`cmp`/`sha256`).

### 4.2 CMake-интеграция (§4.5)

```
assets/source/cube.gltf  ──asset_converter──▶  assets/build/cube.mesh
assets/source/texture.png ──asset_converter──▶ assets/build/texture.tex
```

- Реализуется `add_custom_command` + `add_custom_target(convert_assets ALL)`
  в PC-конфигурации; артефакты `assets/build/` коммитятся как
  versioned-ассеты (PS1-checkout потребляет их без перегенерации).
- Тесты читают артефакты по compile-time `RPE_TEST_SOURCE_DIR`:
  `MeshLoader.RealBlenderCubeAssetLoadsAndValidates` проверяет поля
  (vertex_count = 24, index_count = 12, radius = 113511 для канонического
  куба, все индексы < vertex_count), `TextureLoader` — то же для texture.tex
  (256×256, слот 0 = 0x0000, пиксели ≥1, палитра против `pack_rgb555_stp`).

## 5. Загрузчики движка (runtime)

- `src/assets/mesh_loader.cpp` — `mesh_validate_and_load(data, size, alloc, out, out_error_code)`
- `src/assets/texture_loader.cpp` — `texture_validate_and_load(data, size, alloc, out, out_error_code)`

Диагностические коды `AssetDiag` (`include/retro_engine/assets/assets.h`; порядок — контракт):

| Код | Имя                     | Шаг §1.2.7 |
|-----|-------------------------|------------|
| 0   | `ASSET_ERR_OK`          | — |
| 1   | `ASSET_ERR_NULL_ARGUMENT` | data/alloc/out == NULL |
| 2   | `ASSET_ERR_TRUNCATED_HEADER` | (1) file_size < header_size |
| 3   | `ASSET_ERR_BAD_MAGIC` | (2) magic |
| 4   | `ASSET_ERR_BAD_VERSION` | (2) version ≠ 1 |
| 5   | `ASSET_ERR_BAD_COUNTS` | счётчики: counts==0/не кратен 3; размеры .tex вне {64,128,256} |
| 6   | `ASSET_ERR_ARITHMETIC` | (3) переполнение uint64-арифметики |
| 7   | `ASSET_ERR_PAYLOAD_RANGE` | (4) payload вне файла / перекрытие |
| 8   | `ASSET_ERR_INDEX_RANGE` | (5) индекс ≥ vertex_count |
| 9   | `ASSET_ERR_ALLOC_FAIL` | (6) арена исчерпана (после rollback) |

Гарантии:

- Проверки (1)–(5) выполняются ДО любой аллокации.
- Аллокация — только после успешной валидации (арена; шаг 6).
- Любая пост-аллокационная ошибка → арена откатывается к marker'у:
  `used()` после фейла равен `used()` до вызова; malformed-вход расходует
  persistent-память ровно 0 байт.

## 6. Решения MVP (фиксируются здесь)

1. **Octree-фоллбек Median Cut не реализован.** Median Cut детерминирован и
   достаточно быстр на ≤ 65536 пикселей; иного обоснования фоллбеку нет.
   Возврат к Octree возможен в будущих спринтах без изменения форматов.
2. **Привязка текстуры к мешу (texture_id) не из glTF.** В v1 всегда 0;
   связывание меша со слотом текстуры — прикладной слой будущих спринтов
   (сцена/материалы).
3. **Прозрачность выключена** (STP = 0 в палитре); слот 0 зарезервирован
   под «transparent black» (см. §3.2).
4. **`index_count` = число записей MeshIndex** (см. §2.1).

## 7. Saturn v0.2.0

- Форматы `.mesh` v1 / `.tex` v1 не меняются; Saturn-бэкенд конвертирует
  CI8+RGB555 в VDP1 8bpp CLUT на этапе загрузки (S27–28).
- Потолок `.tex` 256×256 — **engine policy** asset-pipeline, а не
  hardware maximum VDP1 (см. `docs/SATURN.md` §2). Отдельный Saturn-формат
  вводится только через новый ADR; v1 не ломается.
- Имена файлов на CD — ISO9660 8.3 upper-case. HAL получает **bare-имена
  без `;1`** (`CUBE.MSH`, `TEX0.TEX`); строчные нормализуются в upper-case,
  host-пути/`..`/`;`/wildcard'ы отвергаются. Детали — `docs/SATURN.md` §8.
- Карта образа v0.2.0: `assets/build/cube.mesh → CUBE.MSH`,
  `triangle.mesh → TRIANGLE.MSH`, `texture.tex → TEXTURE.TEX`,
  `texture_{0..3}.tex → TEX{0..3}.TEX`; копирование выполняет
  `src/hal/saturn/CMakeLists.txt`, CI сверяет байты.
- Загрузка: размер → `AssetHeap` (Work RAM-L, 932 KiB) → точное чтение
  секторами 2048 Б → валидация `.mesh`/`.tex` во временной арене. Ошибка
  чтения/валидации освобождает буфер (0 байт persistent-памяти). Boot
  self-test читает `CUBE.MSH` + `TEX0.TEX`, логирует размер/FNV-1a.
  С S25–26 `CUBE.MSH` остаётся в отдельной persistent-арене 64 KiB и служит
  входом параллельного workload; `TEX0.TEX` валидируется во временной арене
  и освобождается. Asset heap уменьшен до 932 KiB, чтобы выделить 28 KiB
  shared-региона Master/Slave (см. `docs/SATURN.md` §2.1.1/§11).
- **S27–28 (VDP1):** после валидации `.tex` вызывается
  `Render_CreateTexture` → backend `vdp1_texture`: CI8 копируется
  `jo_dma_copy` в VDP1 VRAM (451 832 B first-fit, 32-байтовое выравнивание)
  по абсолютному адресу `VDP1_TEXTURE_BASE+offset`, палитра — в CRAM
  (палитры 1..6; 7 — VDP2 background). Ошибка VRAM/CLUT/upload откатывает
  оба ресурса; после неудачи persistent-память VDP1 — 0 байт. Width
  обязана быть кратна 8, height ≤ 256 (256 клампится до 255 — VDP1 pattern
  height 8 бит). UV треугольника обязаны быть в углах прямоугольника
  текстуры, иначе — flat-Gouraud с диагностикой (ADR-010).
- **S29–30 (VDP2):** `TEX1.TEX` (256×256 CI8) дополнительно тайлится 4× по X
  в NBG1 bitmap 1024×256 (256 KiB VDP2 VRAM), палитра — CRAM 7;
  `.tex` v1 не меняется. Ошибка размеров/инициализации — явный `false`;
  фон можно отключить, тогда VDP1 clear снова непрозрачный.
