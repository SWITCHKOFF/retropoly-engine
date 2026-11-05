// SPDX-License-Identifier: MIT
// RetroPolyEngine — game/components.h
//
// Слой игры: манифест компонентов (норматив: docs/plans/sprint_11_12.md,
// неделя 22; docs/RetroEngine_Engineering_Documentation_v1.43.md §5.6/§6).
//
// ЕДИНСТВЕННАЯ ТОЧКА ПРАВДЫ компонентов — ядро движка: core/ecs/registry.h
// уже владеет семью MVP-компонентами (Position, Rotation, Velocity,
// MeshRenderer, Camera, PlayerControl, Collider) с их нормативными
// ёмкостями (§5.6). Слой игры НЕ дублирует структуры: дубликат лишил бы
// compile-time привязки Registry::pool_for<T>() (специализация у Registry
// единственная — неизвестный тип был бы ошибкой сборки) и рассинхронизировал
// бы ёмкости пулов. Новые компоненты пост-MVP добавляются ЯДРОМ (новый пул +
// ComponentCapacity + пере-сборка), затем поднимаются сюда — никогда
// локальными struct'ами в game/.
//
// Манифест — единственная точка включения компонентов для систем слоя игры
// (game/systems/*) и примеров: включайте его, а не registry.h напрямую.
#pragma once

#include "core/ecs/registry.h"

// Контракты POD-компонентов (ComponentPool копирует dense-строки swap-back'ом
// и никогда не вызывает деструкторы — тривиальность является условием пула).
static_assert(__is_trivially_copyable(Position), "Position: trivially copyable required");
static_assert(__is_trivially_copyable(Rotation), "Rotation: trivially copyable required");
static_assert(__is_trivially_copyable(Velocity), "Velocity: trivially copyable required");
static_assert(__is_trivially_copyable(MeshRenderer), "MeshRenderer: trivially copyable required");
static_assert(__is_trivially_copyable(Camera), "Camera: trivially copyable required");
static_assert(__is_trivially_copyable(PlayerControl), "PlayerControl: trivially copyable required");
static_assert(__is_trivially_copyable(Collider), "Collider: trivially copyable required");
