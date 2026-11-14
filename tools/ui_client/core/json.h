// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/json.h
//
// Минимальный детерминированный JSON-парсер/сериализатор для UI client
// (Спринт 35–36, недели 69–70). Внешние JSON-зависимости не вводятся:
// формат `.rpeproject`/`settings.json` небольшой, а diagnostics должны быть
// стабильными и не зависеть от версии сторонней библиотеки.
//
// Контракт:
//   - объекты сохраняют порядок ключей;
//   - duplicate keys — ошибка парсинга (детерминированно);
//   - глубина вложенности ограничена (JSON_MAX_DEPTH), чтобы повреждённый
//     файл не мог переполнить стек;
//   - ошибка содержит 1-based line/column.
#pragma once

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

namespace rpe {
namespace ui {

/** Тип JSON-значения. */
enum class JsonType
{
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

/** Ошибка разбора JSON (1-based координаты). */
struct JsonParseError
{
    int line = 0; /**< Строка или 0, если координата неизвестна. */
    int column = 0; /**< Колонка или 0. */
    std::string message; /**< Человекочитаемая причина. */
};

/** Значение JSON с сохранением порядка ключей объекта. */
class JsonValue {
public:
    JsonValue() = default;

    static JsonValue makeNull();
    static JsonValue makeBool(bool value);
    static JsonValue makeInt(int64_t value);
    static JsonValue makeDouble(double value);
    static JsonValue makeString(std::string value);
    static JsonValue makeArray();
    static JsonValue makeObject();

    JsonType type() const { return type_; }
    bool isNull() const { return type_ == JsonType::Null; }
    bool isBool() const { return type_ == JsonType::Bool; }
    bool isNumber() const { return type_ == JsonType::Number; }
    bool isString() const { return type_ == JsonType::String; }
    bool isArray() const { return type_ == JsonType::Array; }
    bool isObject() const { return type_ == JsonType::Object; }

    /** @return true, если значение — bool; иначе false (out не меняется). */
    bool asBool(bool* out) const;
    /** @return true, если значение — целое число без дробной части. */
    bool asInt64(int64_t* out) const;
    /** @return true, если значение — число. */
    bool asDouble(double* out) const;

    /** @return строка или пустая строка, если тип другой. */
    const std::string& asString() const { return string_; }

    const std::vector<JsonValue>& array() const { return array_; }
    std::vector<JsonValue>& array() { return array_; }

    const std::vector<std::pair<std::string, JsonValue>>& object() const { return object_; }
    std::vector<std::pair<std::string, JsonValue>>& object() { return object_; }

    /** Поиск ключа объекта; nullptr, если ключа нет или тип не Object. */
    const JsonValue* find(const std::string& key) const;
    JsonValue* find(const std::string& key);

    /** Добавить ключ в конец объекта (для сериализации). */
    void append(const std::string& key, JsonValue value);

    /** Добавить элемент массива. */
    void push(JsonValue value);

    /** 1-based координата значения в исходном тексте (0 для synthesized). */
    int line() const { return line_; }
    int column() const { return column_; }
    void setPosition(int line, int column)
    {
        line_ = line;
        column_ = column;
    }

private:
    friend bool jsonParse(const std::string&, JsonValue*, JsonParseError*);

    JsonType type_ = JsonType::Null;
    bool bool_ = false;
    bool integer_ = false;
    int64_t int_ = 0;
    double double_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;
    std::vector<std::pair<std::string, JsonValue>> object_;
    int line_ = 0;
    int column_ = 0;
};

/**
 * Разобрать JSON-текст.
 *
 * @param text Входной текст (UTF-8, BOM допускается).
 * @param out Результат; при ошибке не изменяется.
 * @param error Детали ошибки (может быть nullptr).
 * @return true при успехе.
 */
bool jsonParse(const std::string& text, JsonValue* out, JsonParseError* error);

/**
 * Сериализовать JSON-значение с детерминированным отступом.
 *
 * @param value Значение.
 * @param indent Число пробелов на уровень (2 по умолчанию).
 * @return Текст с завершающим переводом строки.
 */
std::string jsonSerialize(const JsonValue& value, int indent = 2);

}  // namespace ui
}  // namespace rpe
