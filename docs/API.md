# API RetroPolyEngine

> Этот документ — вводный обзор публичных интерфейсов. Полные сигнатуры и
> Doxygen-комментарии живут в исходниках; при расхождении приоритет у
> [`include/retro_engine/hal/platform.h`](../include/retro_engine/hal/platform.h),
> [`include/retro_engine/core/ecs/registry.h`](../include/retro_engine/core/ecs/registry.h),
> [`include/retro_engine/core/math/`](../include/retro_engine/core/math/) и
> [`include/retro_engine/assets/assets.h`](../include/retro_engine/assets/assets.h).

## 1. HAL — единый C-API

Весь платформенный API объявлен в [`include/retro_engine/hal/platform.h`](../include/retro_engine/hal/platform.h).

### 1.1 Платформа

```c
bool Platform_Init(void);
void Platform_Shutdown(void);
bool Platform_PumpEvents(void);
void Platform_SwapBuffers(void);
void Platform_SleepMs(uint32_t ms);
uint32_t Platform_GetTimeMs(void);
```

`Platform_Init` создаёт окно и GL-контекст (PC) или инициализирует графику
(PS1). `Render_Init` вызывается отдельно — ровно один раз до первого кадра.

### 1.2 Рендерер

```c
bool Render_Init(uint32_t width, uint32_t height);
void Render_Shutdown(void);
void Render_BeginFrame(void);
void Render_EndFrame(void);
void Render_Clear(uint8_t r, uint8_t g, uint8_t b);

TextureHandle Render_CreateTexture(uint32_t w, uint32_t h,
                                   const uint8_t* pixels,
                                   const uint16_t* palette);
void Render_DestroyTexture(TextureHandle handle);
void Render_BindTexture(TextureHandle handle);

void Render_SetCamera(const Mat4* view, const Mat4* proj);
void Render_DrawMesh(const Vertex* vertices, uint32_t vertex_count,
                     const Triangle* indices, uint32_t index_count,
                     const Mat4* model);

void Render_OverlayText(int32_t x, int32_t y, uint8_t r, uint8_t g, uint8_t b,
                        uint32_t scale, const char* text);
```

Reference resolution: 320×240. `Render_DrawMesh` проходит pipeline
TRANSFORM→CLIP→CULL→SORT→PACK→SUBMIT.

### 1.3 Ввод

```c
bool Input_IsKeyDown(KeyCode key);
bool Input_IsPadButtonDown(uint32_t pad_index, PadButton button);
int16_t Input_GetPadAxis(uint32_t pad_index, uint32_t axis);

bool Input_SetMouseRelativeMode(bool enable);
void Input_GetMouseDelta(int32_t* out_dx, int32_t* out_dy);
```

Pad 0 на PC эмулируется клавиатурой, если физический геймпад не подключён.

Saturn (6-button pad): D-pad → UP/DOWN/LEFT/RIGHT; A→CROSS; B→CIRCLE;
C→SQUARE; X→TRIANGLE; Y→L1; Z→R1; L→L2; R→R2; START→START. Select на
6-button pad отсутствует; аналоговых осей и мыши нет. `Platform_PumpEvents`
опрашивает пад ровно один раз за кадр; `Input_*` читают защёлку, при
disconnect/не-RegularGamepad все кнопки отпущены. Полная таблица и policy
расширения enum — [`SATURN.md`](SATURN.md) §6.

### 1.4 Аудио

```c
bool Audio_Init(void);
void Audio_Shutdown(void);
void Audio_PlayBeep(uint32_t frequency, uint32_t duration_ms);
```

MVP: диагностический beep. Полный PCM-миксер — post-MVP.

### 1.5 Файловая система

```c
FileData FileSystem_LoadFile(const char* path);
void FileSystem_FreeFile(FileData* file);
```

PC: буфер принадлежит SDL-аллокатору; освобождать только через
`FileSystem_FreeFile`. Saturn: буфер выделяется из `AssetHeap` (WRAM-H) и
возвращается туда же; читаются только bare 8.3-имена без `;1`
(`CUBE.MSH`), host-пути/`..`/`;` отвергаются. Ошибка → `{nullptr, 0}` без
утечки persistent-памяти. Детали — [`SATURN.md`](SATURN.md) §8.

## 2. ECS

```cpp
#include <retro_engine/core/ecs/registry.h>

Registry registry;
Entity e = registry.create_entity();
registry.add_component(e, Position{vec3_make(0, 0, 0)});
registry.add_component(e, MeshRenderer{0u, 0u});

for (auto row : registry.view<Position, MeshRenderer>()) {
    Position& pos = row.get<Position>();
    MeshRenderer& mr = row.get<MeshRenderer>();
    // ...
}
```

- 7 компонентов: `Position`, `Rotation`, `Velocity`, `MeshRenderer`,
  `Camera`, `PlayerControl`, `Collider`.
- Ёмкости фиксированы compile-time (`ComponentCapacity<T>::VALUE`).
- `View` инвалидируется при изменении состава компонентов.

## 3. Математика

```c
fixed_t a = fixed_from_int(2);
fixed_t b = fixed_from_float(0.5f);
fixed_t c = fixed_mul(a, b);       // saturating Q16.16

Vec3 v = vec3_make(fixed_from_int(1), 0, 0);
Vec3 n = vec3_normalize(v);

Mat4 proj, view, model;
mat4_perspective(&proj, FIXED_PI/3, fixed_from_int(4)/3,
                 fixed_from_int(1), fixed_from_int(100));
mat4_look_at(&view, eye, target, up);
```

## 4. Память

```cpp
#include <retro_engine/core/memory/linear_allocator.h>

static uint8_t s_level_memory[448 * 1024];
LinearAllocator level_arena(s_level_memory, sizeof(s_level_memory));

void* p = level_arena.allocate(size, alignment);
LinearAllocator::Marker m = level_arena.mark();
// ... аллокации ...
level_arena.rollback(m);
```

## 5. Ассеты

```c
#include <retro_engine/assets/assets.h>

MeshAsset* mesh = nullptr;
uint32_t err = 0;
bool ok = mesh_validate_and_load(data, size, &arena, &mesh, &err);

TextureAsset* tex = nullptr;
ok = texture_validate_and_load(data, size, &arena, &tex, &err);
```

См. форматы и CLI конвертера в [`ASSETS.md`](ASSETS.md).

## 6. Слой игры

```cpp
#include <retro_engine/game/components.h>
#include <retro_engine/game/systems/render_system.h>

Registry registry;
game::RenderBanks banks = ...;
game::RenderSystem::update(registry, banks);
```

Системы находятся в [`include/retro_engine/game/systems/`](../include/retro_engine/game/systems/).
