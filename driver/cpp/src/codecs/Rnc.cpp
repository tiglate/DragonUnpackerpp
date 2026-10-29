// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Rnc.h"

#include <array>
#include <string>

namespace Dud
{
namespace
{
auto MakeCrcTable() -> std::array<uint16_t, 256>
{
    std::array<uint16_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i)
    {
        auto c = static_cast<uint16_t>(i);
        for (int j = 0; j < 8; ++j)
            c = (c & 1) != 0 ? static_cast<uint16_t>((c >> 1) ^ 0xA001) : static_cast<uint16_t>(c >> 1);
        table[i] = c;
    }
    return table;
}

auto CrcBlock(const std::vector<uint8_t> &buffer, size_t start, size_t length) -> uint16_t
{
    static const std::array<uint16_t, 256> table = MakeCrcTable();
    uint16_t result = 0;
    for (size_t i = start; i < start + length; ++i)
        result = static_cast<uint16_t>(table[(result ^ buffer[i]) & 0xFF] ^ (result >> 8));
    return result;
}

struct HuffEntry
{
    int BitDepth = 0;
    uint32_t Code = 0;
};
using HuffTable = std::array<HuffEntry, 16>;

[[noreturn]] void Corrupt(const char *what)
{
    throw RncError(std::string("RNC data is corrupted (") + what + ")");
}

class RncDecoder
{
  public:
    auto Run(const std::vector<uint8_t> &packed, const RncHeader &h) -> std::vector<uint8_t>
    {
        if (h.UnpackedSize > RncMaxUnpackedSize)
            throw RncError("RNC unpacked size is too large");
        if (static_cast<int64_t>(h.PackedSize) + static_cast<int64_t>(RncHeaderSize) > static_cast<int64_t>(packed.size()))
            throw RncError("RNC packed data is truncated");
        if (CrcBlock(packed, RncHeaderSize, h.PackedSize) != h.PackedCrc)
            throw RncError("RNC packed data CRC error");

        _src = &packed;
        _srcPos = static_cast<int>(RncHeaderSize);
        _srcEnd = static_cast<int>(RncHeaderSize + h.PackedSize);
        _bitBuffer = 0;
        _bitCount = 0;
        _dest.assign(h.UnpackedSize, 0);
        _destPos = 0;

        // First bit: lock flag, ignored when unpacking.
        // Second bit: protected by a key (not supported)
        if (h.Method == 1)
        {
            BitsM1(1);
            if (BitsM1(1) != 0)
                throw RncError("RNC protected files (encryption key) are not supported");
            UnpackM1(h.UnpackedSize);
        }
        else
        {
            BitsM2(1);
            if (BitsM2(1) != 0)
                throw RncError("RNC protected files (encryption key) are not supported");
            UnpackM2(h.UnpackedSize);
        }

        if (CrcBlock(_dest, 0, static_cast<size_t>(_destPos)) != h.UnpackedCrc)
            throw RncError("RNC unpacked data CRC error");
        return std::move(_dest);
    }

  private:
    auto PeekByte(int offset) const -> uint8_t
    {
        return offset >= 0 && offset < _srcEnd ? (*_src)[static_cast<size_t>(offset)] : 0;
    }

    auto ReadByte() -> uint8_t
    {
        // Reading far past the end means the stream is corrupted
        if (_srcPos > _srcEnd + 8)
            Corrupt("unexpected end of data");
        return PeekByte(_srcPos++);
    }

    // Method 1: 16 bits little-endian words read LSB first, with the next word looked ahead
    // (kept in the upper half of the bit buffer)
    auto BitsM1(int count) -> uint32_t
    {
        uint32_t result = 0;
        uint32_t prev = 1;
        while (count > 0)
        {
            if (_bitCount == 0)
            {
                const uint8_t b1 = ReadByte();
                const uint8_t b2 = ReadByte();
                _bitBuffer = (static_cast<uint32_t>(PeekByte(_srcPos + 1)) << 24) |
                             (static_cast<uint32_t>(PeekByte(_srcPos)) << 16) | (static_cast<uint32_t>(b2) << 8) | b1;
                _bitCount = 16;
            }
            if ((_bitBuffer & 1) != 0)
                result |= prev;
            _bitBuffer >>= 1;
            prev <<= 1;
            --_bitCount;
            --count;
        }
        return result;
    }

    // Method 2: bytes read MSB first
    auto BitM2() -> uint32_t
    {
        if (_bitCount == 0)
        {
            _bitBuffer = ReadByte();
            _bitCount = 8;
        }
        const uint32_t result = (_bitBuffer >> 7) & 1;
        _bitBuffer = (_bitBuffer << 1) & 0xFF;
        --_bitCount;
        return result;
    }

    auto BitsM2(int count) -> uint32_t
    {
        uint32_t result = 0;
        while (count > 0)
        {
            result = (result << 1) | BitM2();
            --count;
        }
        return result;
    }

    void MakeTable(HuffTable &table)
    {
        table = {};
        int leaves = static_cast<int>(BitsM1(5));
        if (leaves == 0)
            return;
        if (leaves > 16)
            leaves = 16;
        for (int i = 0; i < leaves; ++i)
            table[static_cast<size_t>(i)].BitDepth = static_cast<int>(BitsM1(4));
        // Canonical codes, stored bit-reversed because they are matched against the LSB first buffer
        uint32_t val = 0;
        uint32_t div = 0x80000000u;
        for (int bits = 1; bits <= 16; ++bits)
        {
            for (int i = 0; i < leaves; ++i)
            {
                HuffEntry &entry = table[static_cast<size_t>(i)];
                if (entry.BitDepth != bits)
                    continue;
                uint32_t code = val / div;
                uint32_t rev = 0;
                for (int j = 0; j < bits; ++j)
                {
                    rev = (rev << 1) | (code & 1);
                    code >>= 1;
                }
                entry.Code = rev;
                val += div;
            }
            div >>= 1;
        }
    }

    auto DecodeTable(const HuffTable &table) -> uint32_t
    {
        for (int i = 0; i < 16; ++i)
        {
            const HuffEntry &entry = table[static_cast<size_t>(i)];
            if (entry.BitDepth != 0 && entry.Code == (_bitBuffer & ((1u << entry.BitDepth) - 1)))
            {
                BitsM1(entry.BitDepth);
                if (i < 2)
                    return static_cast<uint32_t>(i);
                return BitsM1(i - 1) | (1u << (i - 1));
            }
        }
        Corrupt("invalid Huffman code");
    }

    void PutByte(uint8_t b)
    {
        if (_destPos >= static_cast<int>(_dest.size()))
            Corrupt("output larger than announced");
        _dest[static_cast<size_t>(_destPos++)] = b;
    }

    void CopyMatch(int offset, int count)
    {
        if (offset <= 0 || offset > _destPos)
            Corrupt("invalid match offset");
        if (static_cast<int64_t>(_destPos) + count > static_cast<int64_t>(_dest.size()))
            Corrupt("output larger than announced");
        // Byte by byte: source and destination can overlap
        while (count > 0)
        {
            _dest[static_cast<size_t>(_destPos)] = _dest[static_cast<size_t>(_destPos - offset)];
            ++_destPos;
            --count;
        }
    }

    void UnpackM1(uint32_t unpackedSize)
    {
        HuffTable raw;
        HuffTable pos;
        HuffTable len;
        while (_destPos < static_cast<int>(unpackedSize))
        {
            MakeTable(raw);
            MakeTable(pos); // distances
            MakeTable(len); // lengths
            uint32_t subChunks = BitsM1(16);
            while (subChunks > 0)
            {
                --subChunks;
                uint32_t dataLength = DecodeTable(raw);
                if (dataLength > 0)
                {
                    while (dataLength > 0)
                    {
                        PutByte(ReadByte());
                        --dataLength;
                    }
                    // The bits looked ahead were read before the literals: reload them
                    _bitBuffer = (((static_cast<uint32_t>(PeekByte(_srcPos + 2)) << 16) |
                                   (static_cast<uint32_t>(PeekByte(_srcPos + 1)) << 8) | PeekByte(_srcPos))
                                  << _bitCount) |
                                 (_bitBuffer & ((1u << _bitCount) - 1));
                }
                if (subChunks > 0)
                {
                    const int matchOffset = static_cast<int>(DecodeTable(pos)) + 1;
                    const int matchCount = static_cast<int>(DecodeTable(len)) + 2;
                    CopyMatch(matchOffset, matchCount);
                }
            }
        }
    }

    auto MatchOffsetM2() -> int
    {
        int o = 0;
        if (BitM2() != 0)
        {
            o = static_cast<int>(BitM2());
            if (BitM2() != 0)
            {
                o = ((o << 1) | static_cast<int>(BitM2())) | 4;
                if (BitM2() == 0)
                    o = (o << 1) | static_cast<int>(BitM2());
            }
            else if (o == 0)
            {
                o = static_cast<int>(BitM2()) + 2;
            }
        }
        return ((o << 8) | ReadByte()) + 1;
    }

    void UnpackM2(uint32_t unpackedSize)
    {
        while (_destPos < static_cast<int>(unpackedSize))
        {
            bool done = false;
            while (!done)
            {
                if (BitM2() == 0)
                {
                    PutByte(ReadByte());
                }
                else if (BitM2() != 0)
                {
                    int matchCount;
                    int matchOffset;
                    if (BitM2() != 0)
                    {
                        if (BitM2() != 0)
                        {
                            matchCount = ReadByte() + 8;
                            if (matchCount == 8)
                            {
                                BitM2();
                                done = true;
                                continue;
                            }
                        }
                        else
                        {
                            matchCount = 3;
                        }
                        matchOffset = MatchOffsetM2();
                    }
                    else
                    {
                        matchCount = 2;
                        matchOffset = ReadByte() + 1;
                    }
                    CopyMatch(matchOffset, matchCount);
                }
                else
                {
                    int matchCount = static_cast<int>(BitM2()) + 4;
                    if (BitM2() != 0)
                        matchCount = ((matchCount - 1) << 1) + static_cast<int>(BitM2());
                    if (matchCount != 9)
                    {
                        const int matchOffset = MatchOffsetM2();
                        CopyMatch(matchOffset, matchCount);
                    }
                    else
                    {
                        int dataLength = (static_cast<int>(BitsM2(4)) << 2) + 12;
                        while (dataLength > 0)
                        {
                            PutByte(ReadByte());
                            --dataLength;
                        }
                    }
                }
            }
        }
    }

    const std::vector<uint8_t> *_src = nullptr;
    int _srcPos = 0;
    int _srcEnd = 0; // end of the packed data (the stream is padded with 0 after)
    uint32_t _bitBuffer = 0;
    int _bitCount = 0;
    std::vector<uint8_t> _dest;
    int _destPos = 0;
};
} // namespace

auto RncReadHeader(const uint8_t *data, size_t size, RncHeader &header) -> bool
{
    if (size < RncHeaderSize)
        return false;
    if (data[0] != 'R' || data[1] != 'N' || data[2] != 'C' || (data[3] != 1 && data[3] != 2))
        return false;
    const auto be32 = [data](size_t o) {
        return (static_cast<uint32_t>(data[o]) << 24) | (static_cast<uint32_t>(data[o + 1]) << 16) |
               (static_cast<uint32_t>(data[o + 2]) << 8) | data[o + 3];
    };
    header.Method = data[3];
    header.UnpackedSize = be32(4);
    header.PackedSize = be32(8);
    header.UnpackedCrc = static_cast<uint16_t>((data[12] << 8) | data[13]);
    header.PackedCrc = static_cast<uint16_t>((data[14] << 8) | data[15]);
    return true;
}

auto RncUnpack(const std::vector<uint8_t> &packed, const RncHeader &header) -> std::vector<uint8_t>
{
    RncDecoder decoder;
    return decoder.Run(packed, header);
}
} // namespace Dud
