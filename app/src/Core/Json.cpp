// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/Json.h"

#include <charconv>
#include <cstdint>
#include <system_error>

namespace Dup::Core
{
auto JsonValue::Find(std::string_view key) const -> const JsonValue *
{
    if (Kind != Type::Object)
    {
        return nullptr;
    }
    const auto found = Members.find(key);
    return found == Members.end() ? nullptr : &found->second;
}

namespace
{
constexpr int MAX_DEPTH = 64;

class Parser
{
  public:
    explicit Parser(std::string_view text) : _text(text)
    {
    }

    auto Run() -> std::optional<JsonValue>
    {
        JsonValue value;
        SkipSpace();
        if (!ParseValue(value, 0))
        {
            return std::nullopt;
        }
        SkipSpace();
        if (_pos != _text.size())
        {
            return std::nullopt;
        }
        return value;
    }

  private:
    void SkipSpace()
    {
        while (_pos < _text.size() && (_text[_pos] == ' ' || _text[_pos] == '\t' || _text[_pos] == '\n' || _text[_pos] == '\r'))
        {
            ++_pos;
        }
    }

    auto Take(char c) -> bool
    {
        if (_pos < _text.size() && _text[_pos] == c)
        {
            ++_pos;
            return true;
        }
        return false;
    }

    auto Literal(std::string_view word) -> bool
    {
        if (_text.substr(_pos, word.size()) == word)
        {
            _pos += word.size();
            return true;
        }
        return false;
    }

    static void AppendUtf8(std::string &out, std::uint32_t code)
    {
        if (code < 0x80)
        {
            out += static_cast<char>(code);
        }
        else if (code < 0x800)
        {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else if (code < 0x10000)
        {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    auto Hex4(std::uint32_t &code) -> bool
    {
        if (_pos + 4 > _text.size())
        {
            return false;
        }
        code = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char c = _text[_pos++];
            code <<= 4;
            if (c >= '0' && c <= '9')
            {
                code |= static_cast<std::uint32_t>(c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                code |= static_cast<std::uint32_t>(c - 'a' + 10);
            }
            else if (c >= 'A' && c <= 'F')
            {
                code |= static_cast<std::uint32_t>(c - 'A' + 10);
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    auto ParseString(std::string &out) -> bool
    {
        if (!Take('"'))
        {
            return false;
        }
        while (_pos < _text.size())
        {
            const char c = _text[_pos++];
            if (c == '"')
            {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20)
            {
                return false;
            }
            if (c != '\\')
            {
                out += c;
                continue;
            }
            if (_pos >= _text.size())
            {
                return false;
            }
            const char e = _text[_pos++];
            switch (e)
            {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t code = 0;
                if (!Hex4(code))
                {
                    return false;
                }
                if (code >= 0xD800 && code < 0xDC00 && _text.substr(_pos, 2) == "\\u")
                {
                    _pos += 2;
                    std::uint32_t low = 0;
                    if (!Hex4(low) || low < 0xDC00 || low > 0xDFFF)
                    {
                        return false;
                    }
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                AppendUtf8(out, code);
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    auto ParseValue(JsonValue &value, int depth) -> bool
    {
        if (depth > MAX_DEPTH || _pos >= _text.size())
        {
            return false;
        }
        const char c = _text[_pos];
        if (c == '{')
        {
            ++_pos;
            value.Kind = JsonValue::Type::Object;
            SkipSpace();
            if (Take('}'))
            {
                return true;
            }
            while (true)
            {
                SkipSpace();
                std::string key;
                if (!ParseString(key))
                {
                    return false;
                }
                SkipSpace();
                if (!Take(':'))
                {
                    return false;
                }
                SkipSpace();
                JsonValue member;
                if (!ParseValue(member, depth + 1))
                {
                    return false;
                }
                value.Members[std::move(key)] = std::move(member);
                SkipSpace();
                if (Take(','))
                {
                    continue;
                }
                return Take('}');
            }
        }
        if (c == '[')
        {
            ++_pos;
            value.Kind = JsonValue::Type::Array;
            SkipSpace();
            if (Take(']'))
            {
                return true;
            }
            while (true)
            {
                SkipSpace();
                JsonValue item;
                if (!ParseValue(item, depth + 1))
                {
                    return false;
                }
                value.Items.push_back(std::move(item));
                SkipSpace();
                if (Take(','))
                {
                    continue;
                }
                return Take(']');
            }
        }
        if (c == '"')
        {
            value.Kind = JsonValue::Type::String;
            return ParseString(value.Text);
        }
        if (Literal("true"))
        {
            value.Kind = JsonValue::Type::Bool;
            value.Boolean = true;
            return true;
        }
        if (Literal("false"))
        {
            value.Kind = JsonValue::Type::Bool;
            return true;
        }
        if (Literal("null"))
        {
            return true;
        }
        // a number: -? digits (. digits)? ([eE] [+-]? digits)?
        const std::size_t start = _pos;
        Take('-');
        std::size_t digits = 0;
        while (_pos < _text.size() && _text[_pos] >= '0' && _text[_pos] <= '9')
        {
            ++_pos;
            ++digits;
        }
        if (digits == 0)
        {
            return false;
        }
        if (Take('.'))
        {
            std::size_t fraction = 0;
            while (_pos < _text.size() && _text[_pos] >= '0' && _text[_pos] <= '9')
            {
                ++_pos;
                ++fraction;
            }
            if (fraction == 0)
            {
                return false;
            }
        }
        if (_pos < _text.size() && (_text[_pos] == 'e' || _text[_pos] == 'E'))
        {
            ++_pos;
            if (_pos < _text.size() && (_text[_pos] == '+' || _text[_pos] == '-'))
            {
                ++_pos;
            }
            std::size_t exponent = 0;
            while (_pos < _text.size() && _text[_pos] >= '0' && _text[_pos] <= '9')
            {
                ++_pos;
                ++exponent;
            }
            if (exponent == 0)
            {
                return false;
            }
        }
        value.Kind = JsonValue::Type::Number;
        // not strtod: it follows the locale of the program (a comma as the decimal point would cut every number)
        const auto number = _text.substr(start, _pos - start);
        const auto result = std::from_chars(number.data(), number.data() + number.size(), value.Number);
        if (result.ec != std::errc() && result.ec != std::errc::result_out_of_range)
        {
            return false;
        }
        return true;
    }

    std::string_view _text;
    std::size_t _pos{0};
};
} // namespace

auto ParseJson(std::string_view text) -> std::optional<JsonValue>
{
    return Parser(text).Run();
}
} // namespace Dup::Core
