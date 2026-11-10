// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_log_test.cpp
//
// Saturn log sink: Log_Init всегда успешен, сообщение форматируется в
// LOG_MESSAGE_MAX и уходит в C-мост (VDP2-строка 27); после Shutdown — no-op.
#include "core/log/log.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

#include <string.h>

TEST(SaturnLog, MessageIsFormattedAndForwarded)
{
    fake_saturn::reset();

    Log_Message(LOG_LEVEL_INFO, "saturn_hal", "before init");
    EXPECT_EQ(fake_saturn::log_calls, 0);

    EXPECT_TRUE(Log_Init());
    Log_Message(LOG_LEVEL_WARNING, "saturn_hal", "value=%u hex=%x", 42u, 0xABu);

    EXPECT_EQ(fake_saturn::log_calls, 1);
    EXPECT_EQ(fake_saturn::last_log_level, (int)LOG_LEVEL_WARNING);
    EXPECT_EQ(fake_saturn::last_log_domain, "saturn_hal");
    EXPECT_EQ(fake_saturn::last_log_message, "value=42 hex=ab");

    Log_Shutdown();
    Log_Message(LOG_LEVEL_ERROR, "saturn_hal", "after shutdown");
    EXPECT_EQ(fake_saturn::log_calls, 1);
}

TEST(SaturnLog, EmptyDomainAndLongMessageAreSafe)
{
    fake_saturn::reset();
    ASSERT_TRUE(Log_Init());

    char long_text[LOG_MESSAGE_MAX * 2];
    memset(long_text, 'A', sizeof(long_text) - 1u);
    long_text[sizeof(long_text) - 1u] = '\0';

    Log_Message(LOG_LEVEL_ERROR, nullptr, "%s", long_text);
    EXPECT_EQ(fake_saturn::log_calls, 1);
    EXPECT_EQ(fake_saturn::last_log_level, (int)LOG_LEVEL_ERROR);
    EXPECT_EQ(fake_saturn::last_log_domain, "?");
    EXPECT_EQ(fake_saturn::last_log_message.size(), (size_t)LOG_MESSAGE_MAX - 1u);
    EXPECT_EQ(fake_saturn::last_log_message.substr(fake_saturn::last_log_message.size() - 4u),
              "....");

    Log_Shutdown();
}
