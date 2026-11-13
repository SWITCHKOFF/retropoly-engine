// SPDX-License-Identifier: MIT
// RetroPolyEngine — include/retro_engine/retro_engine.h
//
// Umbrella-заголовок публичного SDK. Единственная документированная точка
// входа: #include <retro_engine/retro_engine.h>.
//
// Границы публичного API зафиксированы ADR-011 (docs/adr/ADR-011-public-sdk-api.md):
//   - стабильны: core/{math,memory,ecs,log}, hal/platform.h,
//     assets/assets.h, game/**;
//   - внутренние заголовки (render/*, hal/{pc,ps1,saturn}/*, hal/render_backend.h)
//     в SDK не устанавливаются и не входят в umbrella;
//   - публичные заголовки не включают SDL3/OpenGL/PSn00bSDK/Jo Engine.
#pragma once

#include <retro_engine/version.h>

#include <retro_engine/assets/assets.h>
#include <retro_engine/core/ecs/entity.h>
#include <retro_engine/core/ecs/registry.h>
#include <retro_engine/core/log/log.h>
#include <retro_engine/core/math/fixed.h>
#include <retro_engine/core/math/mat3.h>
#include <retro_engine/core/math/mat4.h>
#include <retro_engine/core/math/vec2.h>
#include <retro_engine/core/math/vec3.h>
#include <retro_engine/core/math/vec4.h>
#include <retro_engine/core/memory/linear_allocator.h>
#include <retro_engine/core/memory/memory_profiler.h>
#include <retro_engine/core/memory/pool_allocator.h>
#include <retro_engine/core/memory/stack_allocator.h>
#include <retro_engine/core/types.h>
#include <retro_engine/game/components.h>
#include <retro_engine/game/systems/camera_math.h>
#include <retro_engine/game/systems/collision_system.h>
#include <retro_engine/game/systems/input_system.h>
#include <retro_engine/game/systems/lighting_system.h>
#include <retro_engine/game/systems/movement_system.h>
#include <retro_engine/game/systems/render_system.h>
#include <retro_engine/game/systems/rotation_system.h>
#include <retro_engine/hal/platform.h>
