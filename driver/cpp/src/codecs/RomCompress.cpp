// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "RomCompress.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Dud
{
namespace
{
constexpr size_t NemesisMaxTiles = 4096;
constexpr size_t KosinskiMaxOut = 262144;
constexpr size_t MinimumOut = 64;
constexpr size_t Lz2MaxOut = 131072;
constexpr size_t MinimumAbutting = 6;
constexpr size_t PointerRun = 4; // consecutive addresses that make a table

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

auto DecodeLz2(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used,
               Lz2Variant variant, size_t *copies) -> bool
{
    out.clear();
    if (copies)
        *copies = 0;
    size_t pos = 0;
    for (;;)
    {
        if (pos >= avail)
            return false;
        const unsigned header = src[pos++];
        if (header == 0xFF)
            break;
        unsigned command = header >> 5;
        size_t length = (header & 31) + 1;
        if (command == 7)
        {
            if (pos >= avail)
                return false;
            command = (header >> 2) & 7;
            length = ((size_t{header} & 3) << 8 | src[pos++]) + 1;
        }
        if (out.size() + length > maxOut)
            return false;
        switch (command)
        {
        case 0:
            if (pos + length > avail)
                return false;
            out.insert(out.end(), src + pos, src + pos + length);
            pos += length;
            break;
        case 1:
            if (pos >= avail)
                return false;
            out.insert(out.end(), length, src[pos++]);
            break;
        case 2:
            if (pos + 2 > avail)
                return false;
            for (size_t i = 0; i < length; ++i)
                out.push_back(src[pos + (i & 1)]);
            pos += 2;
            break;
        case 3: {
            if (pos >= avail)
                return false;
            uint8_t value = src[pos++];
            for (size_t i = 0; i < length; ++i)
                out.push_back(value++);
            break;
        }
        case 4: {
            if (copies && variant != Lz2Variant::Metroid) // (Metroid is judged by its own commands)
                ++*copies;
            if (pos + 2 > avail)
                return false;
            const size_t from = variant == Lz2Variant::Big ? (size_t{src[pos]} << 8) | src[pos + 1]
                                                           : src[pos] | (size_t{src[pos + 1]} << 8);
            pos += 2;
            if (from >= out.size())
                return false;
            for (size_t i = 0; i < length; ++i)
                out.push_back(out[from + i]);
            break;
        }
        case 5: // Super Metroid: copy of the output, bits flipped
        case 6: { // Super Metroid: copy from a byte offset back
            if (variant != Lz2Variant::Metroid)
                return false;
            if (copies)
                ++*copies;
            size_t from;
            if (command == 5)
            {
                if (pos + 2 > avail)
                    return false;
                from = src[pos] | (size_t{src[pos + 1]} << 8);
                pos += 2;
            }
            else
            {
                if (pos >= avail)
                    return false;
                const size_t back = src[pos++];
                if (back == 0 || back > out.size())
                    return false;
                from = out.size() - back;
            }
            if (from >= out.size())
                return false;
            for (size_t i = 0; i < length; ++i)
                out.push_back(command == 5 ? static_cast<uint8_t>(out[from + i] ^ 0xFF) : out[from + i]);
            break;
        }
        default:
            return false;
        }
    }
    used = pos;
    return !out.empty();
}

auto DecodeLzss(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used,
                bool headerCountsItself) -> bool
{
    out.clear();
    if (avail < 4)
        return false;
    const size_t header = src[0] | (size_t{src[1]} << 8);
    const size_t payload = headerCountsItself ? (header >= 2 ? header - 2 : 0) : header;
    const size_t end = 2 + payload;
    if (payload < 2 || end > avail)
        return false;
    std::array<uint8_t, 2048> ring{};
    size_t write = 0x7DE;
    size_t pos = 2;
    while (pos < end)
    {
        const unsigned flags = src[pos++];
        for (int bit = 0; bit < 8 && pos < end; ++bit)
        {
            if ((flags >> bit) & 1)
            {
                const uint8_t value = src[pos++];
                out.push_back(value);
                ring[write] = value;
                write = (write + 1) & 2047;
            }
            else
            {
                if (pos + 2 > end)
                    return false; // a reference cut by the end of the data
                const size_t from = src[pos] | (size_t{src[pos + 1] & 7} << 8);
                const size_t length = (src[pos + 1] >> 3) + 3;
                pos += 2;
                for (size_t k = 0; k < length; ++k)
                {
                    const uint8_t value = ring[(from + k) & 2047];
                    out.push_back(value);
                    ring[write] = value;
                    write = (write + 1) & 2047;
                }
            }
            if (out.size() > maxOut)
                return false;
        }
    }
    used = end;
    return !out.empty();
}

namespace
{
// One try with the width of the offsets assumed; the first control byte must agree with it
auto DecodePacketsWidth(unsigned width, const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out,
                        size_t &used) -> bool
{
    out.clear();
    size_t end = src[0] | (size_t{src[1]} << 8);
    size_t pos = 2;
    unsigned count = 8;
    bool first = true;
    for (;;)
    {
        // the last mask of a packet may run a little over the end offset: the control byte follows it
        if (pos >= end)
        {
            if (pos + 1 > avail)
                return false;
            const unsigned control = src[pos];
            if (first && (control < 0xC0) != (width == 12))
                return false;
            first = false;
            count = control & 63;
            if (count == 0)
            {
                used = pos + 1;
                return !out.empty();
            }
            if (pos + 3 > avail)
                return false;
            end = src[pos + 1] | (size_t{src[pos + 2]} << 8);
            pos += 3;
            continue;
        }
        if (first && pos > 2 + 0x10000)
            return false;
        const unsigned mask = src[pos++];
        if (mask == 0 && count == 8)
        {
            if (pos + 8 > avail)
                return false;
            out.insert(out.end(), src + pos, src + pos + 8);
            pos += 8;
        }
        else
        {
            for (unsigned bit = 0; bit < count; ++bit)
            {
                if (bit < 8 && ((mask >> bit) & 1))
                {
                    if (pos + 2 > avail)
                        return false;
                    const size_t value = src[pos] | (size_t{src[pos + 1]} << 8);
                    pos += 2;
                    const size_t distance = value & ((size_t{1} << width) - 1);
                    const size_t length = (value >> width) + 3;
                    if (distance == 0 || distance > out.size())
                        return false;
                    for (size_t k = 0; k < length; ++k)
                        out.push_back(out[out.size() - distance]);
                }
                else
                {
                    if (pos + 1 > avail)
                        return false;
                    out.push_back(src[pos++]);
                }
            }
        }
        if (out.size() > maxOut)
            return false;
        count = 8;
    }
}
} // namespace

auto DecodeLzPackets(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used) -> bool
{
    if (avail < 4)
        return false;
    return DecodePacketsWidth(12, src, avail, maxOut, out, used) || DecodePacketsWidth(11, src, avail, maxOut, out, used);
}

auto Decode(RomCodec codec, const uint8_t *src, size_t avail, std::vector<uint8_t> &out, size_t &used) -> bool
{
    switch (codec)
    {
    case RomCodec::Nemesis:
        return DecodeNemesis(src, avail, NemesisMaxTiles * 32, out, used);
    case RomCodec::Kosinski:
        return DecodeKosinski(src, avail, KosinskiMaxOut, out, used);
    case RomCodec::Lz2:
        return DecodeLz2(src, avail, Lz2MaxOut, out, used);
    case RomCodec::Lz2Be:
        return DecodeLz2(src, avail, Lz2MaxOut, out, used, Lz2Variant::Big);
    case RomCodec::Lz5:
        return DecodeLz2(src, avail, Lz2MaxOut, out, used, Lz2Variant::Metroid);
    case RomCodec::Lzss:
        return DecodeLzss(src, avail, Lz2MaxOut, out, used, true);
    case RomCodec::LzssPayload:
        return DecodeLzss(src, avail, Lz2MaxOut, out, used, false);
    case RomCodec::LzPackets:
        return DecodeLzPackets(src, avail, Lz2MaxOut, out, used);
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

// ---- Super Nintendo scanner ----------------------------------------------------------------

namespace
{
// File offset of a 24 bits address of the console, or -1
auto SnesFileOffset(uint32_t address, bool hiRom, size_t size) -> int64_t
{
    const uint32_t bank = address >> 16;
    const uint32_t low = address & 0xFFFF;
    if (bank == 0x7E || bank == 0x7F)
        return -1;
    int64_t file;
    if (hiRom)
    {
        // the data of HiROM games is addressed in the banks C0 to FF; accepting more makes almost
        // every 3 bytes of a ROM an address
        if (bank < 0xC0)
            return -1;
        file = static_cast<int64_t>(address & 0x3FFFFF);
    }
    else
    {
        if (low < 0x8000)
            return -1;
        file = static_cast<int64_t>(bank & 0x7F) * 0x8000 + (low - 0x8000);
    }
    return file < static_cast<int64_t>(size) ? file : -1;
}
} // namespace

namespace
{
// copyStreams receives how many of the streams use a copy command: the wrong spelling of the offsets
// still decodes some streams (the ones without copies, and some by chance) but not the others
// Where a stream may start: where a table of addresses points (3 bytes long addresses, in runs of
// four at least, packed or with a byte between: the targets of a lone window that happens to look
// like an address are a million in a 4 MB ROM and each costs a decoding), after the end of another
// stream (0xFF)
auto SnesCandidates(const uint8_t *rom, size_t size, bool hiRom, bool afterTerminators = true) -> std::vector<bool>
{
    std::vector<bool> candidate(size, false);
    std::vector<int64_t> target(size, -1);
    for (size_t at = 0; at + 3 <= size; ++at)
    {
        const uint32_t value = rom[at] | (uint32_t{rom[at + 1]} << 8) | (uint32_t{rom[at + 2]} << 16);
        target[at] = SnesFileOffset(value, hiRom, size);
    }
    for (const size_t stride : {size_t{3}, size_t{4}})
        for (size_t at = 0; at + (PointerRun - 1) * stride + 3 <= size; ++at)
        {
            bool run = true;
            for (size_t k = 0; run && k < PointerRun; ++k)
                run = target[at + k * stride] >= 0;
            if (run)
                for (size_t k = 0; k < PointerRun; ++k)
                    candidate[static_cast<size_t>(target[at + k * stride])] = true;
        }
    for (size_t at = 0; afterTerminators && at + 1 < size; ++at)
        if (rom[at] == 0xFF)
            candidate[at + 1] = true;
    return candidate;
}

auto ScanSnesVariant(const uint8_t *rom, size_t size, std::vector<bool> candidate, Lz2Variant variant,
                     size_t &copyStreams) -> std::vector<RomStream>
{
    copyStreams = 0;
    std::vector<RomStream> found;
    if (size < 0x200)
        return found;

    // Graphics are tiles (16, 24 or 32 bytes each) and are stored one after the other: a stream
    // is kept when it abuts another one, or when it is a big block of 4 bits tiles
    std::vector<RomStream> valid;
    std::vector<size_t> copiesOf;
    std::vector<uint8_t> out;
    for (size_t at = 0; at + 4 <= size;)
    {
        size_t used = 0;
        size_t copies = 0;
        // zero padding is valid code too (copy one zero byte): streams do not start with it
        if (!candidate[at] || (rom[at] == 0 && rom[at + 1] == 0) || !DecodeLz2(rom + at, std::min<size_t>(size - at, Lz2MaxOut), Lz2MaxOut, out, used, variant, &copies) ||
            used < 24 || out.size() <= used || out.size() < 128 || out.size() % 16 != 0 || !Plausible(out))
        {
            ++at;
            continue;
        }
        valid.push_back({variant == Lz2Variant::Big ? RomCodec::Lz2Be : variant == Lz2Variant::Metroid ? RomCodec::Lz5 : RomCodec::Lz2, static_cast<int64_t>(at), static_cast<int64_t>(used), static_cast<int64_t>(out.size())});
        copiesOf.push_back(copies);
        at += used;
        if (at < size)
            candidate[at] = true;
    }
    // A game that uses the scheme has its graphics in long runs of streams; isolated hits of
    // code and data that happen to decode are noise, so a ROM needs enough abutting streams
    std::vector<bool> abuts(valid.size(), false);
    size_t abutting = 0;
    for (size_t i = 0; i + 1 < valid.size(); ++i)
        if (valid[i].Offset + valid[i].Packed == valid[i + 1].Offset)
        {
            abutting += (abuts[i] ? 0 : 1) + 1;
            abuts[i] = abuts[i + 1] = true;
        }
    if (abutting < MinimumAbutting)
        return found;
    for (size_t i = 0; i < valid.size(); ++i)
        if (abuts[i] || (valid[i].Size % 32 == 0 && valid[i].Size >= 1024))
        {
            found.push_back(valid[i]);
            copyStreams += copiesOf[i] != 0 ? 1 : 0;
        }
    return found;
}
} // namespace

// Bits per byte of the data (0 for one repeated byte, 8 for noise; pictures are in between)
auto ByteEntropy(const std::vector<uint8_t> &data) -> double
{
    std::array<size_t, 256> histogram{};
    for (const uint8_t b : data)
        ++histogram[b];
    double bits = 0;
    for (const size_t count : histogram)
        if (count != 0)
        {
            const double p = static_cast<double>(count) / static_cast<double>(data.size());
            bits -= p * std::log2(p);
        }
    return bits;
}

// The streams that carry their own size (Final Fantasy VI's LZSS): they are not told by an end marker, and nearly any
// bytes parse as LZSS (a ROM that has none still gets dozens of "streams" that parse and end on tile rows), so the test
// is the one the real tables pass: the resources of a table of addresses are stored one after the other, so the next
// address of the table is this stream's address plus its size. That needs two addresses to land on exact places and a
// stream that parses to exactly its size, which chance does not give; a handful of entries must pass in the ROM.
auto ScanSnesSized(const uint8_t *rom, size_t size, bool hiRom) -> std::vector<RomStream>
{
    constexpr size_t MinimumEntries = 6;
    constexpr double MinimumMedianEntropy = 4.0;
    std::vector<int64_t> target(size, -1);
    for (size_t at = 0; at + 3 <= size; ++at)
    {
        const uint32_t value = rom[at] | (uint32_t{rom[at + 1]} << 8) | (uint32_t{rom[at + 2]} << 16);
        target[at] = SnesFileOffset(value, hiRom, size);
    }
    std::vector<RomStream> found;
    std::vector<double> entropies; // of the streams in found, in the same order, with the duplicates
    std::vector<uint8_t> out;
    struct Entry
    {
        bool Valid = false;
        RomStream Stream;
        double Entropy = 0;
    };
    for (const size_t stride : {size_t{3}, size_t{4}})
    {
        for (size_t at = 0; at + PointerRun * stride <= size;)
        {
            size_t length = 0;
            while (at + length * stride + 3 <= size && target[at + length * stride] >= 0)
                ++length;
            if (length < PointerRun)
            {
                ++at;
                continue;
            }
            // the cheap test first: the next address is this one plus the size in the header (the header counts
            // itself, or only what follows it); only the entries that pass are decoded
            std::vector<int> semantics(length, 0); // 1: the header counts itself, 2: it does not
            for (size_t k = 0; k + 1 < length; ++k)
            {
                const auto start = static_cast<size_t>(target[at + k * stride]);
                if (start + 2 > size)
                    continue;
                const int64_t header = rom[start] | (int64_t{rom[start + 1]} << 8);
                if (target[at + (k + 1) * stride] == static_cast<int64_t>(start) + header)
                    semantics[k] = 1;
                else if (target[at + (k + 1) * stride] == static_cast<int64_t>(start) + header + 2)
                    semantics[k] = 2;
            }
            std::vector<Entry> entries(length);
            auto decode = [&](size_t k, bool counts) {
                const auto start = static_cast<size_t>(target[at + k * stride]);
                size_t used = 0;
                if (start + 6 <= size && DecodeLzss(rom + start, std::min<size_t>(size - start, 0x10002), Lz2MaxOut, out, used, counts) &&
                    out.size() >= MinimumOut && out.size() % 8 == 0 && Plausible(out))
                    entries[k] = {true, {counts ? RomCodec::Lzss : RomCodec::LzssPayload, static_cast<int64_t>(start), static_cast<int64_t>(used), static_cast<int64_t>(out.size())}, ByteEntropy(out)};
            };
            std::vector<bool> keep(length, false);
            for (size_t k = 0; k + 1 < length; ++k)
            {
                if (semantics[k] == 0)
                    continue;
                decode(k, semantics[k] == 1);
                // the next entry has its own header: it counts in the same way as this one in a table
                decode(k + 1, semantics[k] == 1);
                if (entries[k].Valid && entries[k + 1].Valid &&
                    target[at + (k + 1) * stride] == entries[k].Stream.Offset + entries[k].Stream.Packed)
                    keep[k] = keep[k + 1] = true;
            }
            for (size_t k = 0; k < length; ++k)
                if (keep[k])
                {
                    found.push_back(entries[k].Stream);
                    entropies.push_back(entries[k].Entropy);
                }
            at += length * stride;
        }
    }
    // Records with a length in front also line up (the tables of a sprite animation, for one) and decode to specks:
    // pictures have a median entropy around 5.6 bits per byte in Final Fantasy VI, these 0.7 to 2.7
    if (entropies.empty())
        return {};
    std::sort(entropies.begin(), entropies.end());
    if (entropies[entropies.size() / 2] < MinimumMedianEntropy)
        return {};
    std::sort(found.begin(), found.end(), [](const RomStream &a, const RomStream &b) { return a.Offset < b.Offset; });
    found.erase(std::unique(found.begin(), found.end(), [](const RomStream &a, const RomStream &b) { return a.Offset == b.Offset; }), found.end());
    if (found.size() < MinimumEntries)
        found.clear();
    return found;
}

// Chrono Trigger's packet LZ: the resources of a table of addresses lie one after the other and a stream ends with its
// control byte, so the next address is this one plus the bytes used. A pointer run of which several consecutive
// entries decode and line up, in a ROM where that happens a handful of times, is the game's.
auto ScanSnesPackets(const uint8_t *rom, size_t size, bool hiRom) -> std::vector<RomStream>
{
    constexpr size_t MinimumEntries = 6;
    constexpr double MinimumMedianEntropy = 3.0;
    std::vector<int64_t> target(size, -1);
    for (size_t at = 0; at + 3 <= size; ++at)
    {
        const uint32_t value = rom[at] | (uint32_t{rom[at + 1]} << 8) | (uint32_t{rom[at + 2]} << 16);
        target[at] = SnesFileOffset(value, hiRom, size);
    }
    std::vector<RomStream> found;
    std::vector<double> entropies;
    std::vector<uint8_t> out;
    for (size_t at = 0; at + PointerRun * 3 <= size;)
    {
        size_t length = 0;
        while (at + length * 3 + 3 <= size && target[at + length * 3] >= 0)
            ++length;
        if (length < PointerRun)
        {
            ++at;
            continue;
        }
        std::vector<RomStream> entries(length);
        std::vector<double> entry(length, 0);
        std::vector<bool> valid(length, false);
        for (size_t k = 0; k < length; ++k)
        {
            const auto start = static_cast<size_t>(target[at + k * 3]);
            size_t used = 0;
            if (start + 4 <= size && DecodeLzPackets(rom + start, std::min<size_t>(size - start, 0x10002), Lz2MaxOut, out, used) &&
                out.size() >= MinimumOut && Plausible(out))
            {
                valid[k] = true;
                entries[k] = {RomCodec::LzPackets, static_cast<int64_t>(start), static_cast<int64_t>(used), static_cast<int64_t>(out.size())};
                entry[k] = ByteEntropy(out);
            }
        }
        std::vector<bool> keep(length, false);
        for (size_t k = 0; k + 1 < length; ++k)
            if (valid[k] && valid[k + 1] && target[at + (k + 1) * 3] == entries[k].Offset + entries[k].Packed)
                keep[k] = keep[k + 1] = true;
        for (size_t k = 0; k < length; ++k)
            if (keep[k])
            {
                found.push_back(entries[k]);
                entropies.push_back(entry[k]);
            }
        at += length * 3;
    }
    if (entropies.empty())
        return {};
    std::sort(entropies.begin(), entropies.end());
    if (entropies[entropies.size() / 2] < MinimumMedianEntropy)
        return {};
    std::sort(found.begin(), found.end(), [](const RomStream &a, const RomStream &b) { return a.Offset < b.Offset; });
    found.erase(std::unique(found.begin(), found.end(), [](const RomStream &a, const RomStream &b) { return a.Offset == b.Offset; }), found.end());
    if (found.size() < MinimumEntries)
        found.clear();
    return found;
}

// The games use one of the spellings of the commands: the ROM keeps the one that decodes more streams
// with copy commands (the Zelda 3 one on a tie, and Metroid's is a superset of it)
auto ScanSnes(const uint8_t *rom, size_t size, bool hiRom) -> std::vector<RomStream>
{
    const auto candidates = SnesCandidates(rom, size, hiRom);
    size_t bestCopies = 0;
    auto best = ScanSnesVariant(rom, size, candidates, Lz2Variant::Little, bestCopies);
    for (const auto variant : {Lz2Variant::Big, Lz2Variant::Metroid})
    {
        size_t copies = 0;
        auto candidate = ScanSnesVariant(rom, size, candidates, variant, copies);
        if (copies > bestCopies)
        {
            best = std::move(candidate);
            bestCopies = copies;
        }
    }
    // the sized streams too (a game uses one scheme or the other: a ROM that gives no streams of one gives none)
    auto sized = ScanSnesSized(rom, size, hiRom);
    best.insert(best.end(), sized.begin(), sized.end());
    auto packets = ScanSnesPackets(rom, size, hiRom);
    best.insert(best.end(), packets.begin(), packets.end());
    std::sort(best.begin(), best.end(), [](const RomStream &a, const RomStream &b) { return a.Offset < b.Offset; });
    return best;
}
} // namespace Dud
