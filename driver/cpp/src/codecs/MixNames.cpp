// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "MixNames.h"

#include <unordered_map>
#include <vector>

#include <zlib.h>

namespace Dud
{
namespace
{
constexpr size_t PackedSize = 61550;
constexpr size_t UnpackedSize = 250487;
const uint8_t PackedData[PackedSize] = {
#include "MixNames.data.inc"
};

auto Upper(const std::string &s) -> std::string
{
    std::string r = s;
    for (char &c : r)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return r;
}

struct Database
{
    std::unordered_map<uint32_t, std::string> Td, Ra;

    Database()
    {
        std::vector<uint8_t> raw(UnpackedSize);
        uLongf size = static_cast<uLongf>(raw.size());
        if (uncompress(raw.data(), &size, PackedData, PackedSize) != Z_OK)
            return;
        size_t start = 0;
        for (size_t i = 0; i <= size; ++i)
        {
            if (i == size || raw[i] == '\n')
            {
                if (i > start)
                {
                    const std::string name(reinterpret_cast<const char *>(raw.data()) + start, i - start);
                    Td[MixIdCommandAndConquer(name)] = name;
                    Ra[MixIdRedAlert(name)] = name;
                }
                start = i + 1;
            }
        }
    }
};

auto Db() -> const Database &
{
    static const Database db;
    return db;
}
} // namespace

auto MixIdCommandAndConquer(const std::string &name) -> uint32_t
{
    const std::string s = Upper(name);
    uint32_t id = 0;
    size_t i = 0;
    while (i < s.size())
    {
        uint32_t a = 0;
        for (int j = 0; j < 4; ++j)
        {
            a >>= 8;
            if (i < s.size())
                a += static_cast<uint32_t>(static_cast<uint8_t>(s[i++])) << 24;
        }
        id = ((id << 1) | (id >> 31)) + a;
    }
    return id;
}

auto MixIdRedAlert(const std::string &name) -> uint32_t
{
    std::string s = Upper(name);
    const size_t l = s.size();
    const size_t a = l >> 2;
    if ((l & 3) > 0)
    {
        s += static_cast<char>(l - (a << 2));
        for (size_t i = 3 - (l & 3); i > 0; --i)
            s += s[a << 2];
    }
    return static_cast<uint32_t>(crc32(0, reinterpret_cast<const Bytef *>(s.data()), static_cast<uInt>(s.size())));
}

auto MixNameCommandAndConquer(uint32_t id) -> std::string
{
    const auto it = Db().Td.find(id);
    return it == Db().Td.end() ? std::string() : it->second;
}

auto MixNameRedAlert(uint32_t id) -> std::string
{
    const auto it = Db().Ra.find(id);
    return it == Db().Ra.end() ? std::string() : it->second;
}
} // namespace Dud
