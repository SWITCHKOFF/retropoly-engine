# Спринт 3–4 — Система памяти + ECS (недели 5–8)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§4).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 3–4 |
| Недели | 5–8 — 07.12.2026 – 03.01.2027 |
| Веха | M1 — Ядро (часть 2: память + ECS) |
| Цель | Linear/Pool/Stack аллокаторы + Registry с контрактами безопасности |
| Входные критерии | Спринт 1–2 завершён: `fixed_t`, Vec/Mat, GTest в CI ([`sprint_01_02.md`](sprint_01_02.md)) |
| На критическом пути | Да |
| Выходные артефакты | `linear_allocator.h`, `pool_allocator.h`, `stack_allocator.h`, `entity.h`, `registry.h/.cpp`, `tests/memory/*`, `tests/ecs/*` |
| Следующий спринт | [`sprint_05_06.md`](sprint_05_06.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 30 | 45 | 70 | 46.7 | 6.7 | 60.0 | 40–60 ч | Укладывается |

## Задачи

**Неделя 5 — LinearAllocator:**
- [ ] [B] `src/core/memory/linear_allocator.h`:
  - [ ] `Marker { const LinearAllocator* owner; size_t offset; }`
  - [ ] `allocate(size, alignment=8)`: validate buffer/alignment (power-of-two), **overflow-safe проверки против remaining** (без `m_offset+size`), padding по фактическому адресу, nullptr при нехватке
  - [ ] `mark()`, `rollback(marker)` (owner + bounds), `reset()`, `used()`, `remaining()`
- [ ] [B] Тесты `tests/memory/linear_allocator_test.cpp`:
  - [ ] BasicAllocation: указатели не перекрываются, used() точен
  - [ ] Alignment: адрес (не только offset) кратен align
  - [ ] OutOfMemory: nullptr, состояние не изменено
  - [ ] Reset: used()==0, повторная аллокация работает
  - [ ] Rollback: возвращает к marker; foreign-marker → false; marker.offset > m_offset → false
  - [ ] **UnalignedArenaBase:** арена с базой+1 байт — возвращаемый адрес всё равно выровнен
- [ ] [H] Зафиксировать baseline allocation latency бенчмарком на reference-машине (<10 нс — НЕ acceptance, только baseline)

**Неделя 6 — PoolAllocator:**
- [ ] [B] `src/core/memory/pool_allocator.h`: `template<typename T, size_t N> class Pool`:
  - [ ] `static_assert(N>0 && N<=65535)`
  - [ ] free-list через `uint16_t m_indices[N]` + `m_allocated[N]` + `m_used_count`
  - [ ] `allocate()` → nullptr при исчерпании; `deallocate(T*)`:
    - [ ] Reject foreign-указателей (вне [begin,end))
    - [ ] Reject interior-указателей (byte_offset % sizeof(T) != 0)
    - [ ] Reject double-free (idx не в allocated)
    - [ ] Никакого UB до валидации
  - [ ] `for_each(func)` по живым объектам
- [ ] [B] Тесты `tests/memory/pool_allocator_test.cpp`:
  - [ ] Basic/Reuse (после deallocate тот же адрес), OutOfMemory, Iteration (1+3 после удаления 2)
  - [ ] **RejectsForeignAndInteriorPointers:** foreign int, p+1, повторный free → false
  - [ ] 256 объектов (сценарий пуль) — стресс цикла alloc/free
- [ ] [M] Адреспроверка под AddressSanitizer (локально `pc-asan`, когда появится CI-job)

**Неделя 7 — StackAllocator + интеграция:**
- [ ] [B] `src/core/memory/stack_allocator.h`:
  - [ ] `push(size, alignment=8)` с overflow-safe проверками и выравниванием адреса
  - [ ] `mark()`, `pop(marker)` — owner-check, bounds-check
  - [ ] `reset_frame()`
- [ ] [B] Тесты `tests/memory/stack_allocator_test.cpp`:
  - [ ] PushPop: p3 == buffer + marker.offset
  - [ ] **ForeignMarkerRejected:** marker аллокатора A → pop у B = false
  - [ ] ResetFrame
  - [ ] **UnalignedArenaBase** (база+3)
- [ ] [B] Интеграционный тест: три аллокатора вместе на одном сценарии загрузки «уровня» (static buffer 448 KiB НЕ на стеке!)
- [ ] [H] Зафиксировать в коде правило: мегабайтные арены — только static/global или из memory subsystem, никогда на CPU-стеке

**Неделя 8 — ECS (Registry):**
- [ ] [B] `src/core/ecs/entity.h` — каноническая кодировка хэндла:
  - [ ] `Entity=uint32_t`, INDEX_BITS=24, GEN_BITS=8, INDEX_MASK=0x00FFFFFF, GEN_SHIFT=24, INVALID_ENTITY=0
  - [ ] `entity_make/index/generation`
- [ ] [B] `src/core/ecs/registry.h/.cpp`:
  - [ ] `ComponentPool<T, Capacity>`: dense `m_data[Capacity]`, `m_index_to_entity[Capacity]`, sparse `m_entity_to_index[MAX_ENTITIES]` (uint16_t, 20 KiB/pool); add/remove (swap-back)/get/has
  - [ ] Ровно 7 пулов с ёмкостями MVP: Position 1024, Rotation 1024, Velocity 1024, MeshRenderer 256, Camera 8, PlayerControl 8, Collider 256
  - [ ] `create_entity()`: слот + текущая generation; `destroy_entity()`: пометить dead, **increment generation до reuse**
  - [ ] Каждый entity/component вызов валидирует alive + generation (stale handle → reject/assert)
  - [ ] **Retirement:** если 8-битная generation достигнет wrap (255→0) — слот навсегда выходит из оборота (никакого silent reuse)
  - [ ] `view<Components...>`: dense-порядок итерации, **запрещены** sparse-lookup/get_component внутри цикла (O(1) прямой доступ по dense-строке)
  - [ ] Без RTTI, без виртуальных массивов компонентов, без unordered_map, без heap в hot path; ID компонентов — compile-time/static
- [ ] [B] Тесты `tests/ecs/registry_test.cpp`:
  - [ ] CreateEntity: уникальность, ≠0
  - [ ] DestroyEntity: has_component=false, is_alive=false
  - [ ] Add/RemoveComponent, Get после Roundtrip
  - [ ] View один/два компонента; ViewWithModification (MovementSystem-подобный апдейт)
  - [ ] **StaleHandleRejected:** сохранить хэндл, destroy, повторное использование → reject
  - [ ] **GenerationIncrement:** create/destroy/create на том же слоте → generation выросла
  - [ ] **GenerationWrapRetirement:** слот с generation 255 → после destroy слот не переиспользуется
  - [ ] **Performance:** 10 000 сущностей, итерация Position+Velocity, корректный count
- [ ] [B] AddressSanitizer прогон тестов (`pc-asan`) — 0 findings
- [ ] [B] CHANGELOG + push + PR + merge

## Критерии приёмки спринта 3–4

- [ ] Все три аллокатора корректны (incl. unaligned/foreign/interior/double-free)
- [ ] ECS до 10 000 сущностей; итерация cache-friendly (dense-массивы)
- [ ] Нет утечек (ASan) / 0 UB в reject-путях
- [ ] Doxygen для публичных API
