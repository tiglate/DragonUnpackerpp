// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Writing glTF 2.0 with the data embedded (a base64 buffer): the accessors of the binary buffer and JSON numbers
// that do not follow the locale. Shared by the Requiem models and levels.
#pragma once

#include <charconv>
#include <cstdint>
#include <string>
#include <vector>

namespace Dud
{
inline auto Base64(const std::vector<uint8_t> &data) -> std::string
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3)
    {
        const uint32_t v = data[i] << 16 | (i + 1 < data.size() ? data[i + 1] << 8 : 0) | (i + 2 < data.size() ? data[i + 2] : 0);
        out += table[v >> 18 & 63];
        out += table[v >> 12 & 63];
        out += i + 1 < data.size() ? table[v >> 6 & 63] : '=';
        out += i + 2 < data.size() ? table[v & 63] : '=';
    }
    return out;
}

class GltfWriter
{
  public:
    // A float or index array in the binary buffer; returns the accessor number
    auto Accessor(const void *data, size_t bytes, int component, size_t count, const char *type, const double *min = nullptr,
                  const double *max = nullptr, size_t width = 0, int target = 0) -> int
    {
        while (_buffer.size() % 4 != 0)
            _buffer.push_back(0);
        const size_t offset = _buffer.size();
        _buffer.insert(_buffer.end(), static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + bytes);
        std::string view = "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(bytes);
        if (target)
            view += ",\"target\":" + std::to_string(target);
        _views.push_back(view + "}");
        std::string acc = "{\"bufferView\":" + std::to_string(_views.size() - 1) + ",\"componentType\":" + std::to_string(component) +
                          ",\"count\":" + std::to_string(count) + ",\"type\":\"" + type + "\"";
        if (min && max)
        {
            acc += ",\"min\":[";
            for (size_t i = 0; i < width; ++i)
                acc += (i ? "," : "") + Num(min[i]);
            acc += "],\"max\":[";
            for (size_t i = 0; i < width; ++i)
                acc += (i ? "," : "") + Num(max[i]);
            acc += "]";
        }
        _accessors.push_back(acc + "}");
        return static_cast<int>(_accessors.size() - 1);
    }

    // A number as JSON: not through printf, which would follow the locale of the program that loaded the driver
    static auto Num(double v) -> std::string
    {
        char b[40];
        const auto result = std::to_chars(b, b + sizeof b, v, std::chars_format::general, 9);
        return std::string(b, result.ptr);
    }

    std::vector<uint8_t> _buffer;
    std::vector<std::string> _views;
    std::vector<std::string> _accessors;
};
} // namespace Dud
