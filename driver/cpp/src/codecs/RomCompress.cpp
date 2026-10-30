// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "RomCompress.h"

#include <algorithm>
#include <array>

namespace Dud
{
namespace
{
constexpr size_t NemesisMaxTiles = 4096;
constexpr size_t KosinskiMaxOut = 262144;
constexpr size_t MinimumOut = 64;

// ---- Nemesis -------------------------------------------------------------------------------

struct NemesisCode
{
    uint8_t Length = 0; // 0: unused
    uint8_t Code = 0;
    uint8_t Nibble = 0;
    uint8_t Run = 0;
};

struct BitReader
{
    const uint8_t *Src;
    size_t End;
    size_t Pos;
    int Left = 0; // bits left in Current
    uint8_t Current = 0;

    auto Bit(int &bit) -> bool
    {
        if (Left == 0)
        {
            if (Pos >= End)
                return false;
            Current = Src[Pos++];
            Left = 8;
        }
        bit = (Current >> --Left) & 1;
        return true;
    }
    auto Bits(int count, unsigned &value) -> bool
    {
        value = 0;
        for (int i = 0; i < count; ++i)
        {
            int b;
            if (!Bit(b))
                return false;
            value = (value << 1) | static_cast<unsigned>(b);
        }
        return true;
    }
};
} // namespace

auto DecodeNemesis(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used) -> bool
{
    out.clear();
    if (avail < 4)
        return false;
    const unsigned header = (unsigned{src[0]} << 8) | src[1];
    const bool xorMode = (header & 0x8000) != 0;
    const size_t tiles = header & 0x7FFF;
    if (tiles == 0 || tiles > NemesisMaxTiles || tiles * 32 > maxOut)
        return false;

    // code table
    std::array<NemesisCode, 256> table;
    size_t entries = 0;
    size_t pos = 2;
    int nibble = -1;
    for (;;)
    {
        if (pos >= avail)
            return false;
        uint8_t b = src[pos++];
        if (b == 0xFF)
            break;
        if (b & 0x80)
        {
            nibble = b & 0x0F;
            if (pos >= avail)
                return false;
            b = src[pos++];
            if (b & 0x80)
                return false;
        }
        if (nibble < 0 || pos >= avail || entries >= table.size())
            return false;
        const int run = ((b >> 4) & 7) + 1;
        const int length = b & 0x0F;
        const uint8_t code = src[pos++];
        if (length < 1 || length > 8 || (code >> length) != 0)
            return false;
        table[entries++] = {static_cast<uint8_t>(length), code, static_cast<uint8_t>(nibble), static_cast<uint8_t>(run)};
    }
    if (entries == 0)
        return false;
    // prefix free, also against the escape code 111111
    for (size_t i = 0; i < entries; ++i)
    {
        const auto &a = table[i];
        if (a.Length >= 6 ? ((a.Code >> (a.Length - 6)) == 0x3F) : ((0x3Fu >> (6 - a.Length)) == a.Code))
            return false;
        for (size_t j = i + 1; j < entries; ++j)
        {
            const auto &b2 = table[j];
            const int shortest = std::min(a.Length, b2.Length);
            if ((a.Code >> (a.Length - shortest)) == (b2.Code >> (b2.Length - shortest)))
                return false;
        }
    }

    // lookup by (length, code)
    std::array<std::array<int16_t, 256>, 9> lookup;
    for (auto &row : lookup)
        row.fill(-1);
    for (size_t i = 0; i < entries; ++i)
        lookup[table[i].Length][table[i].Code] = static_cast<int16_t>(i);

    BitReader bits{src, avail, pos};
    out.reserve(tiles * 32);
    const size_t totalRows = tiles * 8;
    uint32_t row = 0;
    uint32_t previous = 0;
    int nibblesInRow = 0;
    size_t rows = 0;
    while (rows < totalRows)
    {
        unsigned code = 0;
        int length = 0;
        int run = 0;
        int value = 0;
        for (;;)
        {
            int b;
            if (!bits.Bit(b))
                return false;
            code = (code << 1) | static_cast<unsigned>(b);
            ++length;
            if (length == 6 && code == 0x3F)
            {
                unsigned inl = 0;
                if (!bits.Bits(7, inl))
                    return false;
                run = static_cast<int>(inl >> 4) + 1;
                value = static_cast<int>(inl & 15);
                break;
            }
            if (length <= 8 && lookup[static_cast<size_t>(length)][code] >= 0)
            {
                const auto &e = table[static_cast<size_t>(lookup[static_cast<size_t>(length)][code])];
                run = e.Run;
                value = e.Nibble;
                break;
            }
            if (length >= 8)
                return false;
        }
        for (int i = 0; i < run && rows < totalRows; ++i)
        {
            row = (row << 4) | static_cast<uint32_t>(value);
            if (++nibblesInRow == 8)
            {
                if (xorMode)
                    row ^= previous;
                previous = row;
                out.push_back(static_cast<uint8_t>(row >> 24));
                out.push_back(static_cast<uint8_t>(row >> 16));
                out.push_back(static_cast<uint8_t>(row >> 8));
                out.push_back(static_cast<uint8_t>(row));
                row = 0;
                nibblesInRow = 0;
                ++rows;
            }
        }
    }
    // the encoders pad the last byte with zero bits
    if ((bits.Current & ((1u << bits.Left) - 1)) != 0)
        return false;
    used = bits.Pos;
    return true;
}

// ---- Kosinski ------------------------------------------------------------------------------

auto DecodeKosinski(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used) -> bool
{
    out.clear();
    size_t pos = 0;
    unsigned desc = 0;
    int left = 0; // -1: the stream ended while a bit was needed
    auto reload = [&]() {
        if (pos + 2 <= avail)
        {
            desc = src[pos] | (unsigned{src[pos + 1]} << 8);
            pos += 2;
            left = 16;
        }
        else
            left = -1;
    };
    reload();
    // the description word is reloaded as soon as its last bit is used (as the Sega routines do)
    auto bit = [&](int &value) -> bool {
        if (left <= 0)
            return false;
        value = static_cast<int>(desc & 1);
        desc >>= 1;
        if (--left == 0)
            reload();
        return true;
    };
    auto byte = [&](unsigned &value) -> bool {
        if (pos >= avail)
            return false;
        value = src[pos++];
        return true;
    };
    for (;;)
    {
        int b;
        if (!bit(b))
            return false;
        if (b)
        {
            unsigned v;
            if (!byte(v) || out.size() >= KosinskiMaxOut || out.size() >= maxOut)
                return false;
            out.push_back(static_cast<uint8_t>(v));
            continue;
        }
        int full;
        if (!bit(full))
            return false;
        int64_t distance;
        size_t count;
        if (full)
        {
            unsigned lo, hi;
            if (!byte(lo) || !byte(hi))
                return false;
            distance = static_cast<int64_t>(((hi & 0xF8) << 5) | lo) - 0x2000;
            const unsigned field = hi & 7;
            if (field != 0)
                count = field + 2;
            else
            {
                unsigned extended;
                if (!byte(extended))
                    return false;
                if (extended == 0)
                    break; // end of the stream
                if (extended == 1)
                    continue; // "nothing to copy": the stream goes on
                count = extended + 1;
            }
        }
        else
        {
            int b1, b2;
            unsigned off;
            if (!bit(b1) || !bit(b2) || !byte(off))
                return false;
            count = static_cast<size_t>(b1 * 2 + b2) + 2;
            distance = static_cast<int64_t>(off) - 256;
        }
        if (static_cast<int64_t>(out.size()) + distance < 0 || out.size() + count > std::min(maxOut, KosinskiMaxOut))
            return false;
        for (size_t i = 0; i < count; ++i)
            out.push_back(out[static_cast<size_t>(static_cast<int64_t>(out.size()) + distance)]);
    }
    used = pos;
    return out.size() >= 1;
}

auto Decode(RomCodec codec, const uint8_t *src, size_t avail, std::vector<uint8_t> &out, size_t &used) -> bool
{
    switch (codec)
    {
    case RomCodec::Nemesis:
        return DecodeNemesis(src, avail, NemesisMaxTiles * 32, out, used);
    case RomCodec::Kosinski:
        return DecodeKosinski(src, avail, KosinskiMaxOut, out, used);
    }
    return false;
}

// ---- scanner -------------------------------------------------------------------------------

namespace
{
// Real art is not flat: reject streams whose output is one repeated byte or almost empty
auto Plausible(const std::vector<uint8_t> &data) -> bool
{
    if (data.size() < MinimumOut)
        return false;
    std::array<uint32_t, 256> histogram{};
    for (const uint8_t b : data)
        ++histogram[b];
    return *std::max_element(histogram.begin(), histogram.end()) < data.size() * 99 / 100;
}
} // namespace

auto ScanMegaDrive(const uint8_t *rom, size_t size) -> std::vector<RomStream>
{
    std::vector<RomStream> found;
    std::vector<uint8_t> out;
    if (size < 0x204)
        return found;

    // Trying Kosinski at every offset is far too slow on dense data (random bytes decode for
    // thousands of codes before ending), so it is only tried where a stream can start: at the
    // addresses the ROM itself holds (the tables and code that load the data use 32 bits
    // absolute addresses), after the end of another stream, and when a terminator
    // (00 F0 00, what the encoders write) is close enough to end it.
    std::vector<bool> candidate(size / 2 + 1, false);
    for (size_t at = 0; at + 4 <= size; at += 2)
    {
        const uint32_t value = (uint32_t{rom[at]} << 24) | (uint32_t{rom[at + 1]} << 16) | (uint32_t{rom[at + 2]} << 8) | rom[at + 3];
        if (value >= 0x200 && value < size && (value & 1) == 0)
            candidate[value / 2] = true;
    }
    std::vector<size_t> terminators;
    for (size_t at = 0; at + 3 <= size; ++at)
        if (rom[at] == 0x00 && rom[at + 1] == 0xF0 && rom[at + 2] == 0x00)
        {
            terminators.push_back(at);
            for (size_t pad = 0; pad <= 6; pad += 2)
            {
                const size_t next = ((at + 3 + 1) & ~size_t{1}) + pad;
                if (next < size)
                    candidate[next / 2] = true;
            }
        }

    // the vector table and the header (0 - 0x1FF) hold no compressed data
    size_t chainUntil = 0; // starts right after a stream are tried too
    for (size_t at = 0x200; at + 4 <= size;)
    {
        size_t used = 0;
        RomStream s;
        s.Offset = static_cast<int64_t>(at);
        bool ok = false;
        if (DecodeNemesis(rom + at, size - at, NemesisMaxTiles * 32, out, used) && Plausible(out))
        {
            s.Codec = RomCodec::Nemesis;
            ok = true;
        }
        else if (candidate[at / 2] || at <= chainUntil)
        {
            // the last terminator that can end a stream starting here
            const auto limit = std::upper_bound(terminators.begin(), terminators.end(), at + KosinskiMaxOut);
            if (limit != terminators.begin() && *(limit - 1) >= at)
            {
                const size_t avail = *(limit - 1) + 3 - at;
                if (DecodeKosinski(rom + at, avail, KosinskiMaxOut, out, used) && out.size() > used && Plausible(out))
                {
                    s.Codec = RomCodec::Kosinski;
                    ok = true;
                }
            }
        }
        if (!ok)
        {
            at += 2;
            continue;
        }
        s.Packed = static_cast<int64_t>(used);
        s.Size = static_cast<int64_t>(out.size());
        found.push_back(s);
        at += (used + 1) & ~size_t{1};
        chainUntil = at + 8;
    }
    return found;
}
} // namespace Dud
