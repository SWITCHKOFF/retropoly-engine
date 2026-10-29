// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/core/ecs/registry.h
//
// ECS Registry: фиксированные компонентные пулы, поколения сущностей и
// dense-итерация View. Нормативные источники:
//   - docs/RetroEngine_Engineering_Documentation_v1.43.md §5.6 (бюджет ECS),
//     §6 (ECS/Registry/View contract);
//   - docs/plans/sprint_03_04.md, Неделя 8.
//
// Контракты:
//   - нет RTTI, виртуальных массивов компонентов, std::unordered_map и heap
//     в hot path; ID компонентов — compile-time через ComponentCapacity<T>;
//   - ровно 7 MVP-пулов с фиксированными ёмкостями (бюджет §5.6);
//   - каждый entity/component-вызов валидирует alive + generation;
//   - generation 255 при destroy выводит слот из оборота навсегда;
//   - View итерирует dense-массив первого компонента; в теле цикла нет
//     get_component()/sparse-lookup — итератор кэширует прямые указатели
//     на dense-строки всех запрошенных компонентов.
#pragma once

#include "core/ecs/entity.h"
#include "core/math/fixed.h"
#include "core/math/vec3.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/** Сентинел «компонента у сущности нет» в sparse-индексе пула. */
constexpr uint16_t COMPONENT_INDEX_NONE = 0xFFFFu;

// ----------------------------------------------------------------------------
// MVP-компоненты (§6.2 engineering doc) и их нормативные ёмкости (§5.6).
// ----------------------------------------------------------------------------

/** Позиция сущности в мире (16.16). */
struct Position
{
    Vec3 value;
};

/** Углы Эйлера поворота (16.16, радианы). */
struct Rotation
{
    fixed_t yaw;
    fixed_t pitch;
    fixed_t roll;
};

/** Линейная скорость (16.16, единицы/секунду). */
struct Velocity
{
    Vec3 value;
};

/** Ссылки на меш и текстуру для рендера. */
struct MeshRenderer
{
    uint32_t mesh_id;
    uint32_t texture_id;
};

/** Параметры камеры (16.16). */
struct Camera
{
    fixed_t fov;
    fixed_t near_plane;
    fixed_t far_plane;
};

/** Параметры управления игроком (16.16). */
struct PlayerControl
{
    fixed_t move_speed;
    fixed_t rotate_speed;
};

/** AABB-коллайдер (16.16). */
struct Collider
{
    Vec3 min;
    Vec3 max;
};

/**
 * Compile-time ёмкость компонентного пула для типа T.
 *
 * Специализации задают ровно 7 MVP-пулов и их нормативные ёмкости.
 * Неизвестный тип не имеет специализации — это осознанная compile-time
 * ошибка, а не скрытый heap-пул.
 */
template<typename T>
struct ComponentCapacity;

/** @cond INTERNAL */
template<>
struct ComponentCapacity<Position>
{
    static constexpr size_t VALUE = 1024;
};

template<>
struct ComponentCapacity<Rotation>
{
    static constexpr size_t VALUE = 1024;
};

template<>
struct ComponentCapacity<Velocity>
{
    static constexpr size_t VALUE = 1024;
};

template<>
struct ComponentCapacity<MeshRenderer>
{
    static constexpr size_t VALUE = 256;
};

template<>
struct ComponentCapacity<Camera>
{
    static constexpr size_t VALUE = 8;
};

template<>
struct ComponentCapacity<PlayerControl>
{
    static constexpr size_t VALUE = 8;
};

template<>
struct ComponentCapacity<Collider>
{
    static constexpr size_t VALUE = 256;
};
/** @endcond */

// ----------------------------------------------------------------------------
// ComponentPool<T, Capacity>
// ----------------------------------------------------------------------------

/**
 * Плотный компонентный пул с swap-back удалением.
 *
 * Данные лежат подряд в m_data[0..m_size); m_entity_to_index — sparse-индекс
 * на dense-строку. Ёмкость фиксирована на этапе компиляции, heap не
 * используется. T обязан быть trivially copyable/destructible (POD-компонент).
 *
 * @tparam T Тип компонента.
 * @tparam Capacity Максимальное число компонентов (1..65535).
 */
template<typename T, size_t Capacity>
class ComponentPool
{
public:
    static_assert(Capacity > 0, "Component capacity must be non-zero");
    static_assert(Capacity <= 65535, "Component capacity must fit uint16_t dense index");

    /** Инициализация sparse-таблицы сентинелами. */
    ComponentPool() : m_size(0)
    {
        for (size_t i = 0; i < MAX_ENTITIES; ++i)
            m_entity_to_index[i] = COMPONENT_INDEX_NONE;
    }

    ComponentPool(const ComponentPool&) = delete;
    ComponentPool& operator=(const ComponentPool&) = delete;

    /**
     * Добавление или замена компонента у сущности.
     *
     * @param entity Валидный (проверенный Registry) хэндл сущности.
     * @param value Значение компонента.
     * @return false при невалидном хэндле или исчерпании ёмкости пула.
     */
    bool add(Entity entity, const T& value)
    {
        if (entity == INVALID_ENTITY)
            return false;
        const uint32_t entity_slot = entity_index(entity);
        if (entity_slot >= MAX_ENTITIES)
            return false;

        uint16_t& sparse = m_entity_to_index[entity_slot];
        if (sparse != COMPONENT_INDEX_NONE)
        {
            m_data[sparse] = value;
            return true;
        }
        if (m_size >= Capacity)
            return false;

        m_data[m_size] = value;
        m_index_to_entity[m_size] = entity;
        sparse = m_size;
        ++m_size;
        return true;
    }

    /**
     * Удаление компонента (swap-back; dense-порядок сохраняется).
     *
     * @param entity Хэндл сущности.
     * @return false, если компонента нет или хэндл невалиден.
     */
    bool remove(Entity entity)
    {
        if (entity == INVALID_ENTITY)
            return false;
        const uint32_t entity_slot = entity_index(entity);
        if (entity_slot >= MAX_ENTITIES)
            return false;

        const uint16_t dense = m_entity_to_index[entity_slot];
        if (dense == COMPONENT_INDEX_NONE || dense >= m_size)
            return false;

        const uint16_t last = static_cast<uint16_t>(m_size - 1);
        if (dense != last)
        {
            m_data[dense] = m_data[last];
            const Entity moved = m_index_to_entity[last];
            m_index_to_entity[dense] = moved;
            m_entity_to_index[entity_index(moved)] = dense;
        }

        m_entity_to_index[entity_slot] = COMPONENT_INDEX_NONE;
        --m_size;
        return true;
    }

    /**
     * Доступ к компоненту по сущности через sparse-индекс.
     *
     * @param entity Хэндл сущности.
     * @return Указатель на dense-строку либо nullptr.
     */
    T* get(Entity entity)
    {
        if (entity == INVALID_ENTITY)
            return nullptr;
        const uint32_t entity_slot = entity_index(entity);
        if (entity_slot >= MAX_ENTITIES)
            return nullptr;
        const uint16_t dense = m_entity_to_index[entity_slot];
        if (dense == COMPONENT_INDEX_NONE || dense >= m_size)
            return nullptr;
        return &m_data[dense];
    }

    /**
     * Const-доступ к компоненту по сущности.
     *
     * @param entity Хэндл сущности.
     * @return Указатель на dense-строку либо nullptr.
     */
    const T* get(Entity entity) const
    {
        if (entity == INVALID_ENTITY)
            return nullptr;
        const uint32_t entity_slot = entity_index(entity);
        if (entity_slot >= MAX_ENTITIES)
            return nullptr;
        const uint16_t dense = m_entity_to_index[entity_slot];
        if (dense == COMPONENT_INDEX_NONE || dense >= m_size)
            return nullptr;
        return &m_data[dense];
    }

    /**
     * Проверка наличия компонента.
     *
     * @param entity Хэндл сущности.
     * @return true, если компонент присутствует.
     */
    bool has(Entity entity) const
    {
        if (entity == INVALID_ENTITY)
            return false;
        const uint32_t entity_slot = entity_index(entity);
        if (entity_slot >= MAX_ENTITIES)
            return false;
        const uint16_t dense = m_entity_to_index[entity_slot];
        return dense != COMPONENT_INDEX_NONE && dense < m_size;
    }

    /** @return Число живых компонентов в пуле. */
    size_t size() const { return m_size; }

    /** @return Максимальная ёмкость пула. */
    static constexpr size_t capacity() { return Capacity; }

    /**
     * Сущность, владеющая dense-строкой (для View-итерации).
     *
     * @param dense_index Индекс в dense-массиве, 0 <= dense_index < size().
     * @return Хэндл сущности.
     */
    Entity entity_at(size_t dense_index) const
    {
        assert(dense_index < m_size);
        return m_index_to_entity[dense_index];
    }

    /** Полная очистка пула (O(size), без обращения ко всем MAX_ENTITIES). */
    void clear()
    {
        for (size_t i = 0; i < m_size; ++i)
            m_entity_to_index[entity_index(m_index_to_entity[i])] = COMPONENT_INDEX_NONE;
        m_size = 0;
    }

private:
    T m_data[Capacity];
    Entity m_index_to_entity[Capacity];
    uint16_t m_entity_to_index[MAX_ENTITIES];
    uint16_t m_size;
};

// ----------------------------------------------------------------------------
// View
// ----------------------------------------------------------------------------

template<typename... Components>
class View;

class Registry;

namespace ecs_detail
{

/** Позиция T в списке типов Components... (compile-time). */
template<typename T, typename... Types>
struct TypeIndex;

template<typename T, typename... Rest>
struct TypeIndex<T, T, Rest...>
{
    static constexpr size_t VALUE = 0;
};

template<typename T, typename U, typename... Rest>
struct TypeIndex<T, U, Rest...>
{
    static constexpr size_t VALUE = 1 + TypeIndex<T, Rest...>::VALUE;
};

template<typename T>
size_t view_size(const void* pool)
{
    return static_cast<const ComponentPool<T, ComponentCapacity<T>::VALUE>*>(pool)->size();
}

template<typename T>
Entity view_entity_at(const void* pool, size_t dense_index)
{
    return static_cast<const ComponentPool<T, ComponentCapacity<T>::VALUE>*>(pool)->entity_at(
        dense_index);
}

template<typename T>
bool view_has_entity(const void* pool, Entity entity)
{
    return static_cast<const ComponentPool<T, ComponentCapacity<T>::VALUE>*>(pool)->has(entity);
}

template<typename T>
void* view_data_for_entity(void* pool, Entity entity)
{
    return static_cast<ComponentPool<T, ComponentCapacity<T>::VALUE>*>(pool)->get(entity);
}

} // namespace ecs_detail

// ----------------------------------------------------------------------------
// Registry
// ----------------------------------------------------------------------------

/**
 * ECS Registry с фиксированным набором MVP-компонентных пулов.
 *
 * Registry не копируется и не использует heap. Владеет слотами сущностей
 * (generation + alive) и семью пулами; destroy_entity удаляет все компоненты
 * сущности. Экземпляр с бюджетом ~234 KiB размещается в static/global storage,
 * не на CPU-стеке. Размер принудительно ограничен 256 KiB (см. registry.cpp).
 */
class Registry
{
public:
    /** Инициализация слотов и пулов. */
    Registry();

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    /** Полный сброс Registry в начальное состояние (например, между тестами). */
    void reset();

    /**
     * Создание сущности.
     *
     * @return Хэндл с текущей generation слота; INVALID_ENTITY, если все
     *         слоты заняты или выведены из оборота.
     */
    Entity create_entity();

    /**
     * Уничтожение сущности и всех её компонентов.
     *
     * Generation увеличивается до повторного использования слота. Если
     * generation уже равна 255, слот выводится из оборота навсегда.
     *
     * @param entity Валидный живой хэндл.
     * @return false для stale/invalid хэндла (без изменения состояния).
     */
    bool destroy_entity(Entity entity);

    /**
     * Проверка живости хэндла с учётом generation.
     *
     * @param entity Хэндл сущности.
     * @return true, если слот жив и generation совпадает.
     */
    bool is_alive(Entity entity) const;

    /** @return Число живых сущностей. */
    size_t alive_count() const { return m_alive_count; }

    /**
     * Добавление или замена компонента.
     *
     * @tparam T Тип компонента (один из 7 MVP-типов).
     * @param entity Живой хэндл сущности.
     * @param component Значение компонента.
     * @return false для stale хэндла или переполнения пула.
     */
    template<typename T>
    bool add_component(Entity entity, const T& component)
    {
        if (!is_alive(entity))
            return false;
        return pool_for<T>().add(entity, component);
    }

    /**
     * Удаление компонента.
     *
     * @tparam T Тип компонента.
     * @param entity Живой хэндл сущности.
     * @return false для stale хэндла или отсутствующего компонента.
     */
    template<typename T>
    bool remove_component(Entity entity)
    {
        if (!is_alive(entity))
            return false;
        return pool_for<T>().remove(entity);
    }

    /**
     * Доступ к компоненту.
     *
     * @tparam T Тип компонента.
     * @param entity Живой хэндл сущности.
     * @return Указатель на компонент либо nullptr.
     */
    template<typename T>
    T* get_component(Entity entity)
    {
        if (!is_alive(entity))
            return nullptr;
        return pool_for<T>().get(entity);
    }

    /**
     * Const-доступ к компоненту.
     *
     * @tparam T Тип компонента.
     * @param entity Живой хэндл сущности.
     * @return Указатель на компонент либо nullptr.
     */
    template<typename T>
    const T* get_component(Entity entity) const
    {
        if (!is_alive(entity))
            return nullptr;
        return pool_for<T>().get(entity);
    }

    /**
     * Проверка наличия компонента у живой сущности.
     *
     * @tparam T Тип компонента.
     * @param entity Живой хэндл сущности.
     * @return true, если компонент присутствует.
     */
    template<typename T>
    bool has_component(Entity entity) const
    {
        if (!is_alive(entity))
            return false;
        return pool_for<T>().has(entity);
    }

    /**
     * Создание View по набору компонентов.
     *
     * Итерация идёт по dense-массиву ПЕРВОГО компонента; итератор кэширует
     * указатели на dense-строки остальных компонентов и не выполняет
     * sparse-lookup в теле пользовательского цикла.
     *
     * @tparam Components Компоненты пересечения (минимум один).
     * @return View<Components...>.
     */
    template<typename... Components>
    View<Components...> view();

private:
    using PositionPool = ComponentPool<Position, ComponentCapacity<Position>::VALUE>;
    using RotationPool = ComponentPool<Rotation, ComponentCapacity<Rotation>::VALUE>;
    using VelocityPool = ComponentPool<Velocity, ComponentCapacity<Velocity>::VALUE>;
    using MeshRendererPool = ComponentPool<MeshRenderer, ComponentCapacity<MeshRenderer>::VALUE>;
    using CameraPool = ComponentPool<Camera, ComponentCapacity<Camera>::VALUE>;
    using PlayerControlPool = ComponentPool<PlayerControl, ComponentCapacity<PlayerControl>::VALUE>;
    using ColliderPool = ComponentPool<Collider, ComponentCapacity<Collider>::VALUE>;

    struct Slot
    {
        uint8_t generation;
        bool alive;
    };

    void remove_all_components(Entity entity);

    template<typename T>
    ComponentPool<T, ComponentCapacity<T>::VALUE>& pool_for();

    template<typename T>
    const ComponentPool<T, ComponentCapacity<T>::VALUE>& pool_for() const;

    PositionPool m_positions;
    RotationPool m_rotations;
    VelocityPool m_velocities;
    MeshRendererPool m_mesh_renderers;
    CameraPool m_cameras;
    PlayerControlPool m_player_controls;
    ColliderPool m_colliders;

    Slot m_slots[MAX_ENTITIES];
    uint16_t m_free_next[MAX_ENTITIES];
    uint16_t m_free_head;
    uint16_t m_alive_count;
};

// ----------------------------------------------------------------------------
// Registry::pool_for<T> — compile-time привязка типа к члену-пулу.
// ----------------------------------------------------------------------------

/** @cond INTERNAL */
template<>
inline ComponentPool<Position, ComponentCapacity<Position>::VALUE>& Registry::pool_for<Position>()
{
    return m_positions;
}

template<>
inline const ComponentPool<Position, ComponentCapacity<Position>::VALUE>&
Registry::pool_for<Position>() const
{
    return m_positions;
}

template<>
inline ComponentPool<Rotation, ComponentCapacity<Rotation>::VALUE>& Registry::pool_for<Rotation>()
{
    return m_rotations;
}

template<>
inline const ComponentPool<Rotation, ComponentCapacity<Rotation>::VALUE>&
Registry::pool_for<Rotation>() const
{
    return m_rotations;
}

template<>
inline ComponentPool<Velocity, ComponentCapacity<Velocity>::VALUE>& Registry::pool_for<Velocity>()
{
    return m_velocities;
}

template<>
inline const ComponentPool<Velocity, ComponentCapacity<Velocity>::VALUE>&
Registry::pool_for<Velocity>() const
{
    return m_velocities;
}

template<>
inline ComponentPool<MeshRenderer, ComponentCapacity<MeshRenderer>::VALUE>&
Registry::pool_for<MeshRenderer>()
{
    return m_mesh_renderers;
}

template<>
inline const ComponentPool<MeshRenderer, ComponentCapacity<MeshRenderer>::VALUE>&
Registry::pool_for<MeshRenderer>() const
{
    return m_mesh_renderers;
}

template<>
inline ComponentPool<Camera, ComponentCapacity<Camera>::VALUE>& Registry::pool_for<Camera>()
{
    return m_cameras;
}

template<>
inline const ComponentPool<Camera, ComponentCapacity<Camera>::VALUE>&
Registry::pool_for<Camera>() const
{
    return m_cameras;
}

template<>
inline ComponentPool<PlayerControl, ComponentCapacity<PlayerControl>::VALUE>&
Registry::pool_for<PlayerControl>()
{
    return m_player_controls;
}

template<>
inline const ComponentPool<PlayerControl, ComponentCapacity<PlayerControl>::VALUE>&
Registry::pool_for<PlayerControl>() const
{
    return m_player_controls;
}

template<>
inline ComponentPool<Collider, ComponentCapacity<Collider>::VALUE>& Registry::pool_for<Collider>()
{
    return m_colliders;
}

template<>
inline const ComponentPool<Collider, ComponentCapacity<Collider>::VALUE>&
Registry::pool_for<Collider>() const
{
    return m_colliders;
}
/** @endcond */

// ----------------------------------------------------------------------------
// View<Components...>
// ----------------------------------------------------------------------------

/**
 * View пересечения компонентов.
 *
 * Итерация идёт по dense-массиву ПЕРВОГО компонента. Итератор при переходе
 * к следующей валидной строке проверяет O(1) наличие остальных компонентов
 * и кэширует прямые указатели; в теле цикла get<T>() — O(1) без sparse-lookup.
 *
 * @note View хранит снимок размера primary-пула. Любое структурное изменение
 *       Registry (create/destroy/add/remove) инвалидирует существующий View;
 *       создавайте View заново после изменения состава компонентов.
 */
template<typename... Components>
class View
{
public:
    static_assert(sizeof...(Components) > 0, "View requires at least one component");
    static constexpr size_t COMPONENT_COUNT = sizeof...(Components);

    class Iterator
    {
    public:
        /**
         * Итератор на dense-строку primary-пула.
         *
         * @param view View-владелец.
         * @param row Начальная строка primary-пула.
         */
        Iterator(View& view, size_t row) : m_view(&view), m_row(row), m_entity(INVALID_ENTITY)
        {
            for (size_t i = 0; i < COMPONENT_COUNT; ++i)
                m_components[i] = nullptr;
            find_next();
        }

        /** Переход к следующей валидной строке. */
        Iterator& operator++()
        {
            ++m_row;
            find_next();
            return *this;
        }

        /** Разрешение range-for; возвращает сам итератор-строку. */
        Iterator& operator*() { return *this; }

        /**
         * Сравнение итераторов.
         *
         * @param other Другой итератор.
         * @return true, если совпадают View и строка.
         */
        bool operator==(const Iterator& other) const
        {
            return m_view == other.m_view && m_row == other.m_row;
        }

        /**
         * Сравнение итераторов.
         *
         * @param other Другой итератор.
         * @return true, если итераторы различны.
         */
        bool operator!=(const Iterator& other) const { return !(*this == other); }

        /** @return Хэндл текущей сущности (INVALID_ENTITY для end()). */
        Entity entity() const { return m_entity; }

        /**
         * Прямой доступ к компоненту текущей dense-строки.
         *
         * @tparam T Один из компонентов View.
         * @return Ссылка на компонент.
         */
        template<typename T>
        T& get() const
        {
            void* component = m_components[ecs_detail::TypeIndex<T, Components...>::VALUE];
            assert(component != nullptr);
            return *static_cast<T*>(component);
        }

    private:
        void find_next();

        View* m_view;
        size_t m_row;
        Entity m_entity;
        void* m_components[COMPONENT_COUNT];
    };

    /**
     * Конструктор View (вызывается Registry::view<...>()).
     *
     * @param pools Пулы ровно в порядке Components....
     */
    explicit View(ComponentPool<Components, ComponentCapacity<Components>::VALUE>*... pools)
        : m_primary_size(0)
    {
        const PoolAccess access[COMPONENT_COUNT] = {
            PoolAccess{static_cast<void*>(pools), &ecs_detail::view_size<Components>,
                &ecs_detail::view_entity_at<Components>, &ecs_detail::view_has_entity<Components>,
                &ecs_detail::view_data_for_entity<Components>}...};

        for (size_t i = 0; i < COMPONENT_COUNT; ++i)
            m_access[i] = access[i];
        m_primary_size = m_access[0].size(m_access[0].pool);
    }

    /** @return Итератор на первую валидную строку. */
    Iterator begin() { return Iterator(*this, 0); }

    /** @return Итератор за последней dense-строкой primary-пула. */
    Iterator end() { return Iterator(*this, m_primary_size); }

    /**
     * Подсчёт валидных строк (O(primary_size)).
     *
     * @return Число сущностей, имеющих все компоненты View.
     */
    size_t count()
    {
        size_t result = 0;
        const Iterator last = end();
        for (Iterator it = begin(); it != last; ++it)
            ++result;
        return result;
    }

private:
    struct PoolAccess
    {
        void* pool;
        size_t (*size)(const void*);
        Entity (*entity_at)(const void*, size_t);
        bool (*has_entity)(const void*, Entity);
        void* (*data_for_entity)(void*, Entity);
    };

    PoolAccess m_access[COMPONENT_COUNT];
    size_t m_primary_size;
};

template<typename... Components>
void View<Components...>::Iterator::find_next()
{
    if (m_view == nullptr)
    {
        m_entity = INVALID_ENTITY;
        for (size_t i = 0; i < COMPONENT_COUNT; ++i)
            m_components[i] = nullptr;
        return;
    }

    while (m_row < m_view->m_primary_size)
    {
        const Entity entity =
            m_view->m_access[0].entity_at(m_view->m_access[0].pool, m_row);

        bool present = true;
        for (size_t i = 1; i < COMPONENT_COUNT; ++i)
        {
            if (!m_view->m_access[i].has_entity(m_view->m_access[i].pool, entity))
            {
                present = false;
                break;
            }
        }

        if (present)
        {
            m_entity = entity;
            for (size_t i = 0; i < COMPONENT_COUNT; ++i)
            {
                m_components[i] =
                    m_view->m_access[i].data_for_entity(m_view->m_access[i].pool, entity);
            }
            return;
        }

        ++m_row;
    }

    m_entity = INVALID_ENTITY;
    for (size_t i = 0; i < COMPONENT_COUNT; ++i)
        m_components[i] = nullptr;
}

// ----------------------------------------------------------------------------
// Registry::view<Components...>() — определение после полного объявления View.
// ----------------------------------------------------------------------------

template<typename... Components>
View<Components...> Registry::view()
{
    return View<Components...>(&pool_for<Components>()...);
}
