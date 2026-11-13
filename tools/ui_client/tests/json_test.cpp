// SPDX-License-Identifier: MIT
// RetroPolyEngine — UI core unit tests: JSON.
#include <gtest/gtest.h>

#include "json.h"

using namespace rpe::ui;

TEST(JsonTest, ParsesAndSerializesObject)
{
    const std::string text =
        "{\"schema\":1,\"name\":\"demo\",\"ok\":true,\"items\":[1,2.5,\"x\"],"
        "\"nested\":{\"empty\":null}}";
    JsonValue value;
    JsonParseError error;
    ASSERT_TRUE(jsonParse(text, &value, &error)) << error.message;
    EXPECT_TRUE(value.isObject());
    int64_t schema = 0;
    ASSERT_TRUE(value.find("schema") != nullptr);
    EXPECT_TRUE(value.find("schema")->asInt64(&schema));
    EXPECT_EQ(schema, 1);
    EXPECT_EQ(value.find("name")->asString(), "demo");
    bool ok = false;
    EXPECT_TRUE(value.find("ok")->asBool(&ok));
    EXPECT_TRUE(ok);
    EXPECT_EQ(value.find("items")->array().size(), 3u);

    const std::string serialized = jsonSerialize(value);
    JsonValue reparsed;
    ASSERT_TRUE(jsonParse(serialized, &reparsed, &error)) << error.message;
    EXPECT_EQ(jsonSerialize(reparsed), serialized);
}

TEST(JsonTest, RejectsDuplicateKeysAndTrailingData)
{
    JsonValue value;
    JsonParseError error;
    EXPECT_FALSE(jsonParse("{\"a\":1,\"a\":2}", &value, &error));
    EXPECT_FALSE(error.message.empty());
    error = JsonParseError();
    EXPECT_FALSE(jsonParse("{\"a\":1} extra", &value, &error));
    EXPECT_FALSE(error.message.empty());
}

TEST(JsonTest, ReportsLineAndColumn)
{
    JsonValue value;
    JsonParseError error;
    EXPECT_FALSE(jsonParse("{\n  \"a\": ,\n}", &value, &error));
    EXPECT_GT(error.line, 1);
    EXPECT_GT(error.column, 0);
}

TEST(JsonTest, UnicodeEscape)
{
    JsonValue value;
    JsonParseError error;
    ASSERT_TRUE(
        jsonParse("{\"text\":\"\\u041f\\u0440\\u0438\\u0432\\u0435\\u0442\"}", &value, &error))
        << error.message;
    EXPECT_EQ(value.find("text")->asString(), "Привет");
}

TEST(JsonTest, DepthLimit)
{
    std::string text;
    for (int i = 0; i < 100; ++i)
        text.push_back('[');
    for (int i = 0; i < 100; ++i)
        text.push_back(']');
    JsonValue value;
    JsonParseError error;
    EXPECT_FALSE(jsonParse(text, &value, &error));
    EXPECT_FALSE(error.message.empty());
}
