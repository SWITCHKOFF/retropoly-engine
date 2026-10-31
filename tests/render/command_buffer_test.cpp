// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/render/command_buffer_test.cpp
//
// Юнит-тесты engine-side command buffer (§7.3_SCANCODE normative):
//   - фиксированная вместимость 64 KiB; capacity выводится из sizeof;
//   - draw_triangle → true при успехе; PЕРЕПОЛНЕНИЕ — ЯВНАЯ ОШИБКА
//     (status=false + overflowed()); любого silent drop нет;
//   - NULL-аргумент → false + overflowed();
//   - порядок вставки сохраняется (submission_index контракта SORT);
//   - reset() очищает буфер и overflow-маркер целиком.
// Тесты не требуют GL — чисто CPU-контракты ядра (экземпляр на стеке
// хоста: стек ≥ 256 KiB, см. шапку command_buffer.h).
#include <gtest/gtest.h>

#include "render/command_buffer.h"

#include <string.h>

// ---------------------------------------------------------------------------
// Вместимость и POD-структура команды
// ---------------------------------------------------------------------------

TEST(CommandBufferLayout, FixedSize64KiB)
{
    EXPECT_EQ(RENDER_COMMAND_BUFFER_BYTES, 64u * 1024u);
    // Поле capacity — выведено из sizeof (компилируемой константы).
    EXPECT_GT(CommandBuffer::capacity(), 0u);
    EXPECT_EQ(CommandBuffer::capacity(),
              RENDER_COMMAND_BUFFER_BYTES / sizeof(RenderTriCmd));
    // Команда — маленький POD; буфер обязан вмещать «репрезентативный» кадр
    // (W0-сцены ~256 команд достаточно; 256 — минимум политики).
    EXPECT_GE(CommandBuffer::capacity(), 256u);
}

TEST(CommandBufferLayout, CommandIsPlainPod)
{
    // Документированный layout RenderTriCmd (command_buffer.h):
    //   Mat4 mvp (64) + TextureHandle (4) + depth_key (4)
    //   + submission_index (4) + x/y/z[3] (36) + attr[18] (18) = 130,
    //   выровнено до 4 (fixed_t) → sizeof = 132.
    // Тест — предохранитель от случайного разбухания команды (буфер
    // 64 KiB фиксирован: каждая команда дороже — меньше вместимость).
    EXPECT_EQ(sizeof(Mat4), 64u);
    EXPECT_EQ(sizeof(RenderTriCmd), 132u);
    EXPECT_EQ(sizeof(RenderTriCmd) % sizeof(fixed_t), 0u);
}

// ---------------------------------------------------------------------------
// Добавление и чтение команд
// ---------------------------------------------------------------------------

namespace {

/** Затолкa команда с детерминированными полями. */
RenderTriCmd make_cmd(uint32_t depth_key, uint32_t submission)
{
    RenderTriCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.texture = (TextureHandle)(submission + 1u);
    cmd.depth_key = depth_key;
    cmd.submission_index = submission;
    for (uint32_t v = 0u; v < 3u; ++v) {
        cmd.x[v] = (fixed_t)(1u + submission);
        cmd.y[v] = (fixed_t)(2u + submission);
        cmd.z[v] = (fixed_t)(3u + submission);
        cmd.attr[v * 6u + 0u] = (uint8_t)submission;
        cmd.attr[v * 6u + 1u] = (uint8_t)(submission + 1u);
        cmd.attr[v * 6u + 2u] = (uint8_t)(submission + 2u);
        cmd.attr[v * 6u + 3u] = (uint8_t)(submission + 3u);
        cmd.attr[v * 6u + 4u] = (uint8_t)(submission + 4u);
        cmd.attr[v * 6u + 5u] = (uint8_t)(submission + 5u);
    }
    return cmd;
}

} // namespace

TEST(CommandBufferApi, DrawAddsAndKeepsInsertionOrder)
{
    CommandBuffer buffer;
    EXPECT_EQ(buffer.command_count(), 0u);
    EXPECT_EQ(buffer.used_bytes(), 0u);
    EXPECT_FALSE(buffer.overflowed());

    const RenderTriCmd first = make_cmd(11u, 0u);
    const RenderTriCmd second = make_cmd(22u, 1u);
    ASSERT_TRUE(buffer.draw_triangle(&first));
    ASSERT_TRUE(buffer.draw_triangle(&second));

    EXPECT_EQ(buffer.command_count(), 2u);
    EXPECT_EQ(buffer.used_bytes(), 2u * sizeof(RenderTriCmd));

    const RenderTriCmd* stored_first = buffer.command(0u);
    const RenderTriCmd* stored_second = buffer.command(1u);
    ASSERT_NE(stored_first, nullptr);
    ASSERT_NE(stored_second, nullptr);
    // Порядок вставки сохранён (контракт SORT submission_index).
    EXPECT_EQ(stored_first->submission_index, 0u);
    EXPECT_EQ(stored_second->submission_index, 1u);
    // Данные — точные копии (memcpy-семантика).
    EXPECT_EQ(memcmp(stored_first, &first, sizeof(RenderTriCmd)), 0);
    EXPECT_EQ(memcmp(stored_second, &second, sizeof(RenderTriCmd)), 0);
}

TEST(CommandBufferApi, OutOfRangeIndexReturnsNull)
{
    CommandBuffer buffer;
    const RenderTriCmd cmd = make_cmd(1u, 0u);
    ASSERT_TRUE(buffer.draw_triangle(&cmd));
    EXPECT_EQ(buffer.command(1u), nullptr);
    EXPECT_EQ(buffer.command(UINT32_MAX), nullptr);
}

// ---------------------------------------------------------------------------
// Явные отказы: NULL и переполнение (никакого silent drop)
// ---------------------------------------------------------------------------

TEST(CommandBufferOverflow, NullArgumentIsExplicitError)
{
    CommandBuffer buffer;
    EXPECT_FALSE(buffer.draw_triangle(NULL));
    EXPECT_TRUE(buffer.overflowed());
    EXPECT_EQ(buffer.command_count(), 0u);
}

TEST(CommandBufferOverflow, ExhaustThenRefuseWithFlag)
{
    CommandBuffer buffer;
    const uint32_t capacity = CommandBuffer::capacity();

    // Последний перед вместимостью — каждый вызов УСПЕШЕН.
    for (uint32_t i = 0u; i < capacity; ++i) {
        const RenderTriCmd cmd = make_cmd(i, i);
        EXPECT_TRUE(buffer.draw_triangle(&cmd)) << "index " << i;
    }
    EXPECT_EQ(buffer.command_count(), capacity);
    // Буфер плотный: ровно вместимость × sizeof (без хвостового остатка,
    // который не вмещает целую команду).
    EXPECT_EQ(buffer.used_bytes(), capacity * sizeof(RenderTriCmd));
    EXPECT_LT(buffer.used_bytes(), RENDER_COMMAND_BUFFER_BYTES);
    EXPECT_FALSE(buffer.overflowed());

    // N+1- команда — ЯВНАЯ ОШИБКА (status=false + overflowed()).
    const RenderTriCmd extra = make_cmd(999u, capacity);
    EXPECT_FALSE(buffer.draw_triangle(&extra));
    EXPECT_TRUE(buffer.overflowed());
    EXPECT_EQ(buffer.command_count(), capacity); // Полный кадр НЕ затронут.
}

TEST(CommandBufferOverflow, OverflowFlagPersistUntilReset)
{
    CommandBuffer buffer;
    EXPECT_FALSE(buffer.draw_triangle(NULL));
    EXPECT_TRUE(buffer.overflowed());

    buffer.reset();
    EXPECT_FALSE(buffer.overflowed());
    EXPECT_EQ(buffer.command_count(), 0u);
    EXPECT_EQ(buffer.used_bytes(), 0u);
    EXPECT_EQ(buffer.command(0u), nullptr);

    // После reset буфер снова принимает команды.
    const RenderTriCmd cmd = make_cmd(7u, 0u);
    EXPECT_TRUE(buffer.draw_triangle(&cmd));
    EXPECT_EQ(buffer.command_count(), 1u);
}
