// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Dup::Core
{
/// A JSON value (RFC 8259): what the glTF reader needs, nothing more. Objects keep the last value of a repeated key.
class JsonValue
{
  public:
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type Kind{Type::Null};
    bool Boolean{false};
    double Number{0};
    std::string Text;
    std::vector<JsonValue> Items;
    std::map<std::string, JsonValue, std::less<>> Members;

    [[nodiscard]] auto IsObject() const -> bool
    {
        return Kind == Type::Object;
    }
    [[nodiscard]] auto IsArray() const -> bool
    {
        return Kind == Type::Array;
    }
    [[nodiscard]] auto IsNumber() const -> bool
    {
        return Kind == Type::Number;
    }
    [[nodiscard]] auto IsString() const -> bool
    {
        return Kind == Type::String;
    }
    /// The member of an object, nullptr when this is not an object or has no such member.
    [[nodiscard]] auto Find(std::string_view key) const -> const JsonValue *;
    /// Numbers and strings with a default for everything else.
    [[nodiscard]] auto NumberOr(double fallback) const -> double
    {
        return Kind == Type::Number ? Number : fallback;
    }
    [[nodiscard]] auto TextOr(const std::string &fallback) const -> std::string
    {
        return Kind == Type::String ? Text : fallback;
    }
};

/// nullopt for text that is not valid JSON (or nested deeper than 64 levels).
[[nodiscard]] auto ParseJson(std::string_view text) -> std::optional<JsonValue>;
} // namespace Dup::Core
