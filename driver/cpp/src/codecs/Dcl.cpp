// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// See Dcl.h.

#include "Dcl.h"

#include <array>

namespace Dud
{
namespace
{
// Code length tables of the format: each byte is (repeat - 1) << 4 | length
constexpr uint8_t LiteralRep[] = {
    11,  124, 8,   7,   28,  7,   188, 13,  76,  4,   10,  8,   12,  10,  12, 10, 8,   23,  8,   9,
    7,   6,   7,   8,   7,   6,   55,  8,   23,  24,  12,  11,  7,   9,   11, 12, 6,   7,   22,  5,
    7,   24,  6,   11,  9,   6,   7,   22,  7,   11,  38,  7,   9,   8,   25, 11, 8,   11,  9,   12,
    8,   12,  5,   38,  5,   38,  5,   11,  7,   5,   6,   21,  6,   10,  53, 8,  7,   24,  10,  27,
    44,  253, 253, 253, 252, 252, 252, 13,  12,  45,  12,  45,  12,  61,  12, 45, 44,  173};
constexpr uint8_t LengthRep[] = {2, 35, 36, 53, 38, 23};
constexpr uint8_t DistanceRep[] = {2, 20, 53, 230, 247, 151, 248};

constexpr uint16_t LengthBase[16] = {3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264};
constexpr uint8_t LengthExtra[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};
constexpr uint16_t EndLength = 519;

// Canonical Huffman table: codes of the same length are consecutive, in symbol order; the bits are read from the
// stream inverted
struct Huffman
{
    std::array<uint16_t, 17> Count{};
    std::vector<uint16_t> Symbols;

    template <size_t N> explicit Huffman(const uint8_t (&rep)[N])
    {
        std::vector<uint8_t> lengths;
        for (const uint8_t b : rep)
            for (int i = 0; i <= (b >> 4); ++i)
                lengths.push_back(b & 15);
        for (const uint8_t l : lengths)
            ++Count[l];
        Count[0] = 0;
        std::array<uint16_t, 17> offsets{};
        for (int l = 1; l < 16; ++l)
            offsets[l + 1] = static_cast<uint16_t>(offsets[l] + Count[l]);
        Symbols.resize(lengths.size());
        for (size_t s = 0; s < lengths.size(); ++s)
            if (lengths[s] != 0)
                Symbols[offsets[lengths[s]]++] = static_cast<uint16_t>(s);
    }
};

class BitReader
{
  public:
    BitReader(const uint8_t *data, size_t size) : _data(data), _size(size)
    {
    }

    // False at the end of the input
    auto Bits(int count, uint32_t &value) -> bool
    {
        value = 0;
        for (int i = 0; i < count; ++i)
        {
            if (_pos >> 3 >= _size)
                return false;
            value |= static_cast<uint32_t>((_data[_pos >> 3] >> (_pos & 7)) & 1) << i;
            ++_pos;
        }
        return true;
    }

    auto Symbol(const Huffman &table, int &symbol) -> bool
    {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= 16; ++len)
        {
            uint32_t bit = 0;
            if (!Bits(1, bit))
                return false;
            code |= static_cast<int>(bit ^ 1);
            const int count = table.Count[len];
            if (code - count < first)
            {
                symbol = table.Symbols[static_cast<size_t>(index + (code - first))];
                return true;
            }
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return false;
    }

  private:
    const uint8_t *_data;
    size_t _size;
    size_t _pos = 16; // after the two header bytes
};
} // namespace

auto DclExplode(const uint8_t *in, size_t inSize, size_t maxSize, std::vector<uint8_t> &out, bool prefix) -> bool
{
    static const Huffman literals(LiteralRep);
    static const Huffman lengths(LengthRep);
    static const Huffman distances(DistanceRep);

    out.clear();
    if (inSize < 4 || in[0] > 1 || in[1] < 4 || in[1] > 6)
        return false;
    const bool coded = in[0] == 1;
    const int dictBits = in[1];
    BitReader bits(in, inSize);
    for (;;)
    {
        uint32_t flag = 0;
        if (!bits.Bits(1, flag))
            return false;
        if (flag == 0)
        {
            int value = 0;
            if (coded)
            {
                if (!bits.Symbol(literals, value))
                    return false;
            }
            else
            {
                uint32_t raw = 0;
                if (!bits.Bits(8, raw))
                    return false;
                value = static_cast<int>(raw);
            }
            if (out.size() >= maxSize)
                return prefix;
            out.push_back(static_cast<uint8_t>(value));
            if (prefix && out.size() == maxSize)
                return true;
            continue;
        }
        int sym = 0;
        uint32_t extra = 0;
        if (!bits.Symbol(lengths, sym) || !bits.Bits(LengthExtra[sym], extra))
            return false;
        size_t length = LengthBase[sym] + extra;
        if (length == EndLength)
            return true;
        int highDist = 0;
        uint32_t lowDist = 0;
        if (!bits.Symbol(distances, highDist) || !bits.Bits(length == 2 ? 2 : dictBits, lowDist))
            return false;
        const size_t distance = (static_cast<size_t>(highDist) << (length == 2 ? 2 : dictBits)) + lowDist + 1;
        if (distance > out.size())
            return false;
        if (out.size() + length > maxSize)
        {
            if (!prefix)
                return false;
            length = maxSize - out.size();
        }
        for (size_t i = 0; i < length; ++i)
            out.push_back(out[out.size() - distance]);
        if (prefix && out.size() == maxSize)
            return true;
    }
}
} // namespace Dud
