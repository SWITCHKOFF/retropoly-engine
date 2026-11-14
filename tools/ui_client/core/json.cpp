// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/json.cpp
#include "json.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits>

namespace rpe {
namespace ui {

namespace {

const int kJsonMaxDepth = 64;

/** Дописать Unicode code point в UTF-8. */
void appendUtf8(std::string* out, uint32_t cp)
{
    if (cp <= 0x7Fu) {
        out->push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out->push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out->push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out->push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out->push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out->push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out->push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out->push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out->push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out->push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

class Parser {
public:
    Parser(const std::string& text, JsonParseError* error) : text_(text), error_(error) {}

    bool parse(JsonValue* out)
    {
        skipBom();
        skipWhitespace();
        if (!parseValue(out, 0)) return false;
        skipWhitespace();
        if (pos_ != text_.size()) {
            return fail("неожиданные данные после JSON-значения");
        }
        return true;
    }

private:
    bool fail(const char* message)
    {
        if (error_ != nullptr) {
            error_->line = line_;
            error_->column = column_;
            error_->message = message;
        }
        return false;
    }

    bool eof() const { return pos_ >= text_.size(); }
    char peek() const { return eof() ? '\0' : text_[pos_]; }

    void advance()
    {
        if (eof()) return;
        if (text_[pos_] == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        ++pos_;
    }

    void skipBom()
    {
        if (text_.size() >= 3 && static_cast<unsigned char>(text_[0]) == 0xEFu &&
            static_cast<unsigned char>(text_[1]) == 0xBBu &&
            static_cast<unsigned char>(text_[2]) == 0xBFu) {
            pos_ = 3;
        }
    }

    void skipWhitespace()
    {
        while (!eof()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else {
                break;
            }
        }
    }

    bool expect(char c, const char* message)
    {
        skipWhitespace();
        if (peek() != c) return fail(message);
        advance();
        return true;
    }

    bool parseValue(JsonValue* out, int depth)
    {
        if (depth > kJsonMaxDepth) return fail("превышена максимальная глубина JSON");
        skipWhitespace();
        if (eof()) return fail("неожиданный конец JSON");

        const int start_line = line_;
        const int start_column = column_;
        const char c = peek();
        bool ok = false;
        if (c == '{') {
            ok = parseObject(out, depth);
        } else if (c == '[') {
            ok = parseArray(out, depth);
        } else if (c == '"') {
            std::string value;
            ok = parseString(&value) && setString(out, value);
        } else if (c == 't') {
            ok = parseLiteral("true") && setBool(out, true);
        } else if (c == 'f') {
            ok = parseLiteral("false") && setBool(out, false);
        } else if (c == 'n') {
            ok = parseLiteral("null") && setNull(out);
        } else {
            ok = parseNumber(out);
        }
        if (ok) {
            out->setPosition(start_line, start_column);
        }
        return ok;
    }

    bool setNull(JsonValue* out)
    {
        *out = JsonValue::makeNull();
        return true;
    }

    bool setBool(JsonValue* out, bool value)
    {
        *out = JsonValue::makeBool(value);
        return true;
    }

    bool setString(JsonValue* out, const std::string& value)
    {
        *out = JsonValue::makeString(value);
        return true;
    }

    bool parseLiteral(const char* literal)
    {
        const size_t length = strlen(literal);
        if (pos_ + length > text_.size()) return fail("незаконченный литерал");
        if (text_.compare(pos_, length, literal) != 0) return fail("неизвестный литерал");
        for (size_t i = 0; i < length; ++i)
            advance();
        return true;
    }

    bool parseObject(JsonValue* out, int depth)
    {
        advance(); // '{'
        JsonValue object = JsonValue::makeObject();
        skipWhitespace();
        if (peek() == '}') {
            advance();
            *out = object;
            return true;
        }
        while (true) {
            skipWhitespace();
            if (peek() != '"') return fail("ожидался строковый ключ объекта");
            std::string key;
            if (!parseString(&key)) return false;
            if (object.find(key) != nullptr) {
                return fail("дублирующийся ключ объекта");
            }
            if (!expect(':', "ожидалось ':' после ключа")) return false;
            JsonValue value;
            if (!parseValue(&value, depth + 1)) return false;
            object.append(key, value);
            skipWhitespace();
            if (peek() == ',') {
                advance();
                continue;
            }
            if (peek() == '}') {
                advance();
                *out = object;
                return true;
            }
            return fail("ожидалось ',' или '}' в объекте");
        }
    }

    bool parseArray(JsonValue* out, int depth)
    {
        advance(); // '['
        JsonValue array = JsonValue::makeArray();
        skipWhitespace();
        if (peek() == ']') {
            advance();
            *out = array;
            return true;
        }
        while (true) {
            JsonValue value;
            if (!parseValue(&value, depth + 1)) return false;
            array.push(value);
            skipWhitespace();
            if (peek() == ',') {
                advance();
                continue;
            }
            if (peek() == ']') {
                advance();
                *out = array;
                return true;
            }
            return fail("ожидалось ',' или ']' в массиве");
        }
    }

    bool parseString(std::string* out)
    {
        out->clear();
        advance(); // '"'
        while (!eof()) {
            const unsigned char c = static_cast<unsigned char>(peek());
            if (c == '"') {
                advance();
                return true;
            }
            if (c < 0x20u) {
                return fail("управляющий символ в строке");
            }
            if (c != '\\') {
                out->push_back(static_cast<char>(c));
                advance();
                continue;
            }
            advance(); // backslash
            if (eof()) return fail("незаконченная escape-последовательность");
            const char esc = peek();
            advance();
            switch (esc) {
                case '"':
                    out->push_back('"');
                    break;
                case '\\':
                    out->push_back('\\');
                    break;
                case '/':
                    out->push_back('/');
                    break;
                case 'b':
                    out->push_back('\b');
                    break;
                case 'f':
                    out->push_back('\f');
                    break;
                case 'n':
                    out->push_back('\n');
                    break;
                case 'r':
                    out->push_back('\r');
                    break;
                case 't':
                    out->push_back('\t');
                    break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!parseHex4(&cp)) return false;
                    if (cp >= 0xD800u && cp <= 0xDBFFu) {
                        if (peek() != '\\') return fail("незаконченная суррогатная пара");
                        advance();
                        if (peek() != 'u') return fail("незаконченная суррогатная пара");
                        advance();
                        uint32_t low = 0;
                        if (!parseHex4(&low)) return false;
                        if (low < 0xDC00u || low > 0xDFFFu) {
                            return fail("неверная суррогатная пара");
                        }
                        cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
                    } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                        return fail("неверная суррогатная пара");
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default:
                    return fail("неизвестная escape-последовательность");
            }
        }
        return fail("незаконченная строка");
    }

    bool parseHex4(uint32_t* out)
    {
        if (pos_ + 4 > text_.size()) return fail("незаконченный \\u escape");
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const int digit = hexDigit(text_[pos_]);
            if (digit < 0) return fail("неверная hex-цифра в \\u escape");
            value = (value << 4) | static_cast<uint32_t>(digit);
            advance();
        }
        *out = value;
        return true;
    }

    bool parseNumber(JsonValue* out)
    {
        const size_t start = pos_;
        if (peek() == '-') advance();
        if (eof()) return fail("незаконченное число");
        if (peek() == '0') {
            advance();
        } else if (peek() >= '1' && peek() <= '9') {
            while (!eof() && peek() >= '0' && peek() <= '9')
                advance();
        } else {
            return fail("ожидалось JSON-значение");
        }
        bool integer = true;
        if (!eof() && peek() == '.') {
            integer = false;
            advance();
            if (eof() || peek() < '0' || peek() > '9') return fail("неверная дробная часть");
            while (!eof() && peek() >= '0' && peek() <= '9')
                advance();
        }
        if (!eof() && (peek() == 'e' || peek() == 'E')) {
            integer = false;
            advance();
            if (!eof() && (peek() == '+' || peek() == '-')) advance();
            if (eof() || peek() < '0' || peek() > '9') return fail("неверная степень числа");
            while (!eof() && peek() >= '0' && peek() <= '9')
                advance();
        }

        const std::string token = text_.substr(start, pos_ - start);
        if (integer) {
            errno = 0;
            char* end = nullptr;
            const long long value = strtoll(token.c_str(), &end, 10);
            if (errno == 0 && end != nullptr && *end == '\0') {
                *out = JsonValue::makeInt(static_cast<int64_t>(value));
                return true;
            }
        }
        errno = 0;
        char* end = nullptr;
        const double value = strtod(token.c_str(), &end);
        if (end == nullptr || *end != '\0') return fail("неверное число");
        *out = JsonValue::makeDouble(value);
        return true;
    }

    const std::string& text_;
    JsonParseError* error_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
};

void writeIndent(std::string* out, int indent, int depth)
{
    if (indent <= 0) return;
    out->push_back('\n');
    out->append(static_cast<size_t>(indent * depth), ' ');
}

void writeEscaped(std::string* out, const std::string& value)
{
    out->push_back('"');
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        switch (c) {
            case '"':
                out->append("\\\"");
                break;
            case '\\':
                out->append("\\\\");
                break;
            case '\b':
                out->append("\\b");
                break;
            case '\f':
                out->append("\\f");
                break;
            case '\n':
                out->append("\\n");
                break;
            case '\r':
                out->append("\\r");
                break;
            case '\t':
                out->append("\\t");
                break;
            default:
                if (c < 0x20u) {
                    char buffer[8];
                    snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c));
                    out->append(buffer);
                } else {
                    out->push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out->push_back('"');
}

void writeValue(std::string* out, const JsonValue& value, int indent, int depth)
{
    switch (value.type()) {
        case JsonType::Null:
            out->append("null");
            break;
        case JsonType::Bool:
            out->append(value.asBool(nullptr) ? "true" : "false");
            break;
        case JsonType::Number: {
            int64_t integer = 0;
            if (value.asInt64(&integer)) {
                out->append(std::to_string(integer));
            } else {
                double number = 0.0;
                value.asDouble(&number);
                if (!isfinite(number)) number = 0.0;
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "%.17g", number);
                out->append(buffer);
            }
            break;
        }
        case JsonType::String:
            writeEscaped(out, value.asString());
            break;
        case JsonType::Array: {
            const std::vector<JsonValue>& items = value.array();
            if (items.empty()) {
                out->append("[]");
                break;
            }
            out->push_back('[');
            for (size_t i = 0; i < items.size(); ++i) {
                if (i > 0) out->push_back(',');
                writeIndent(out, indent, depth + 1);
                writeValue(out, items[i], indent, depth + 1);
            }
            writeIndent(out, indent, depth);
            out->push_back(']');
            break;
        }
        case JsonType::Object: {
            const std::vector<std::pair<std::string, JsonValue>>& members = value.object();
            if (members.empty()) {
                out->append("{}");
                break;
            }
            out->push_back('{');
            for (size_t i = 0; i < members.size(); ++i) {
                if (i > 0) out->push_back(',');
                writeIndent(out, indent, depth + 1);
                writeEscaped(out, members[i].first);
                out->append(": ");
                writeValue(out, members[i].second, indent, depth + 1);
            }
            writeIndent(out, indent, depth);
            out->push_back('}');
            break;
        }
    }
}

} // namespace

JsonValue JsonValue::makeNull()
{
    return JsonValue();
}

JsonValue JsonValue::makeBool(bool value)
{
    JsonValue result;
    result.type_ = JsonType::Bool;
    result.bool_ = value;
    return result;
}

JsonValue JsonValue::makeInt(int64_t value)
{
    JsonValue result;
    result.type_ = JsonType::Number;
    result.integer_ = true;
    result.int_ = value;
    result.double_ = static_cast<double>(value);
    return result;
}

JsonValue JsonValue::makeDouble(double value)
{
    JsonValue result;
    result.type_ = JsonType::Number;
    result.integer_ = false;
    result.double_ = value;
    return result;
}

JsonValue JsonValue::makeString(std::string value)
{
    JsonValue result;
    result.type_ = JsonType::String;
    result.string_ = std::move(value);
    return result;
}

JsonValue JsonValue::makeArray()
{
    JsonValue result;
    result.type_ = JsonType::Array;
    return result;
}

JsonValue JsonValue::makeObject()
{
    JsonValue result;
    result.type_ = JsonType::Object;
    return result;
}

bool JsonValue::asBool(bool* out) const
{
    if (type_ != JsonType::Bool) return false;
    if (out != nullptr) *out = bool_;
    return true;
}

bool JsonValue::asInt64(int64_t* out) const
{
    if (type_ != JsonType::Number) return false;
    if (integer_) {
        if (out != nullptr) *out = int_;
        return true;
    }
    if (!isfinite(double_)) return false;
    if (double_ < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
        double_ > static_cast<double>(std::numeric_limits<int64_t>::max())) {
        return false;
    }
    const int64_t truncated = static_cast<int64_t>(double_);
    if (static_cast<double>(truncated) != double_) return false;
    if (out != nullptr) *out = truncated;
    return true;
}

bool JsonValue::asDouble(double* out) const
{
    if (type_ != JsonType::Number) return false;
    if (out != nullptr) *out = double_;
    return true;
}

const JsonValue* JsonValue::find(const std::string& key) const
{
    if (type_ != JsonType::Object) return nullptr;
    for (size_t i = 0; i < object_.size(); ++i) {
        if (object_[i].first == key) return &object_[i].second;
    }
    return nullptr;
}

JsonValue* JsonValue::find(const std::string& key)
{
    if (type_ != JsonType::Object) return nullptr;
    for (size_t i = 0; i < object_.size(); ++i) {
        if (object_[i].first == key) return &object_[i].second;
    }
    return nullptr;
}

void JsonValue::append(const std::string& key, JsonValue value)
{
    if (type_ != JsonType::Object) return;
    object_.push_back(std::make_pair(key, std::move(value)));
}

void JsonValue::push(JsonValue value)
{
    if (type_ != JsonType::Array) return;
    array_.push_back(std::move(value));
}

bool jsonParse(const std::string& text, JsonValue* out, JsonParseError* error)
{
    if (out == nullptr) return false;
    if (error != nullptr) *error = JsonParseError();
    Parser parser(text, error);
    JsonValue value;
    if (!parser.parse(&value)) return false;
    *out = value;
    return true;
}

std::string jsonSerialize(const JsonValue& value, int indent)
{
    std::string out;
    writeValue(&out, value, indent, 0);
    out.push_back('\n');
    return out;
}

} // namespace ui
} // namespace rpe
