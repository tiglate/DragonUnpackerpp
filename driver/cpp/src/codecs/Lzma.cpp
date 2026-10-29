// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Lzma.h"

#include <array>
#include <vector>

namespace Dud
{
namespace
{
using Probs = std::vector<uint16_t>;

constexpr int NumStates = 12;
constexpr uint32_t MatchMinLen = 2;

class LzmaDecoder
{
  public:
    LzmaDecoder(const uint8_t *src, int srcLength, uint8_t *dst, int64_t dstLength)
        : _src(src), _srcLength(srcLength), _dst(dst), _dstLength(dstLength)
    {
    }

    auto Run() -> int
    {
        if (_srcLength < 13)
            return LzmaErrInput;
        int d = _src[0];
        if (d >= 9 * 5 * 5)
            return LzmaErrProps;
        const int lc = d % 9;
        d /= 9;
        const int lp = d % 5;
        const int pb = d / 5;
        _ip = 13;
        if (NextByte() != 0)
            return LzmaErrData;
        _range = 0xFFFFFFFF;
        _code = 0;
        for (int i = 1; i <= 4; ++i)
            _code = (_code << 8) | NextByte();
        if (_failed != 0)
            return _failed;
        if (_code == _range)
            return LzmaErrData;

        _isMatch.assign(NumStates << 4, 1024);
        _isRep.assign(NumStates, 1024);
        _isRepG0.assign(NumStates, 1024);
        _isRepG1.assign(NumStates, 1024);
        _isRepG2.assign(NumStates, 1024);
        _isRep0Long.assign(NumStates << 4, 1024);
        _posSlot.assign(4 * 64, 1024);
        _posDecoders.assign(115, 1024);
        _align.assign(16, 1024);
        for (int i = 0; i < 2; ++i)
        {
            _lenChoice[i].assign(1, 1024);
            _lenChoice2[i].assign(1, 1024);
            _lenLow[i].assign(16 << 3, 1024);
            _lenMid[i].assign(16 << 3, 1024);
            _lenHigh[i].assign(256, 1024);
        }
        _lit.assign(static_cast<size_t>(0x300) << (lc + lp), 1024);

        uint32_t state = 0;
        uint32_t rep0 = 0, rep1 = 0, rep2 = 0, rep3 = 0;
        int64_t pos = 0;
        while (pos < _dstLength && _failed == 0)
        {
            const int posState = static_cast<int>(pos & ((1 << pb) - 1));
            if (DecodeBit(_isMatch[(state << 4) + posState]) == 0)
            {
                const uint32_t prevByte = pos > 0 ? _dst[pos - 1] : 0;
                const uint32_t litState = ((static_cast<uint32_t>(pos) & ((1u << lp) - 1)) << lc) + (prevByte >> (8 - lc));
                const size_t litBase = 0x300u * litState;
                uint32_t symbol = 1;
                if (state >= 7)
                {
                    if (static_cast<int64_t>(rep0) >= pos)
                        return LzmaErrData;
                    uint32_t matchByte = _dst[pos - rep0 - 1];
                    do
                    {
                        const uint32_t matchBit = (matchByte >> 7) & 1;
                        matchByte <<= 1;
                        const uint32_t bit = DecodeBit(_lit[litBase + ((1 + matchBit) << 8) + symbol]);
                        symbol = (symbol << 1) | bit;
                        if (matchBit != bit)
                            break;
                    } while (symbol < 0x100);
                }
                while (symbol < 0x100)
                    symbol = (symbol << 1) | DecodeBit(_lit[litBase + symbol]);
                _dst[pos] = static_cast<uint8_t>(symbol);
                ++pos;
                if (state < 4)
                    state = 0;
                else if (state < 10)
                    state -= 3;
                else
                    state -= 6;
                continue;
            }
            uint32_t len;
            if (DecodeBit(_isRep[state]) != 0)
            {
                if (pos == 0)
                    return LzmaErrData;
                if (DecodeBit(_isRepG0[state]) == 0)
                {
                    if (DecodeBit(_isRep0Long[(state << 4) + posState]) == 0)
                    {
                        if (static_cast<int64_t>(rep0) >= pos)
                            return LzmaErrData;
                        state = state < 7 ? 9 : 11;
                        _dst[pos] = _dst[pos - rep0 - 1];
                        ++pos;
                        continue;
                    }
                }
                else
                {
                    uint32_t dist;
                    if (DecodeBit(_isRepG1[state]) == 0)
                        dist = rep1;
                    else
                    {
                        if (DecodeBit(_isRepG2[state]) == 0)
                            dist = rep2;
                        else
                        {
                            dist = rep3;
                            rep3 = rep2;
                        }
                        rep2 = rep1;
                    }
                    rep1 = rep0;
                    rep0 = dist;
                }
                len = DecodeLen(1, posState);
                state = state < 7 ? 8 : 11;
            }
            else
            {
                rep3 = rep2;
                rep2 = rep1;
                rep1 = rep0;
                len = DecodeLen(0, posState);
                state = state < 7 ? 7 : 10;
                rep0 = DecodeDistance(len);
                if (rep0 == 0xFFFFFFFF)
                {
                    // end marker
                    if (pos == _dstLength)
                        return LzmaOk;
                    return LzmaErrData;
                }
            }
            len += MatchMinLen;
            if (_failed != 0)
                break;
            if (static_cast<int64_t>(rep0) >= pos)
                return LzmaErrData;
            if (pos + len > _dstLength)
                return LzmaErrOutput;
            for (uint32_t k = 1; k <= len; ++k)
            {
                _dst[pos] = _dst[pos - rep0 - 1];
                ++pos;
            }
        }
        if (_failed != 0)
            return _failed;
        return LzmaOk;
    }

  private:
    auto NextByte() -> uint8_t
    {
        if (_ip >= _srcLength)
        {
            _failed = LzmaErrInput;
            return 0;
        }
        return _src[_ip++];
    }

    void Normalize()
    {
        if (_range < (1u << 24))
        {
            _range <<= 8;
            _code = (_code << 8) | NextByte();
        }
    }

    auto DecodeBit(uint16_t &prob) -> uint32_t
    {
        const uint32_t bound = (_range >> 11) * prob;
        uint32_t result;
        if (_code < bound)
        {
            _range = bound;
            prob = static_cast<uint16_t>(prob + ((2048 - prob) >> 5));
            result = 0;
        }
        else
        {
            _range -= bound;
            _code -= bound;
            prob = static_cast<uint16_t>(prob - (prob >> 5));
            result = 1;
        }
        Normalize();
        return result;
    }

    auto DirectBits(int n) -> uint32_t
    {
        uint32_t result = 0;
        while (n > 0)
        {
            _range >>= 1;
            _code -= _range;
            const uint32_t t = 0 - (_code >> 31);
            _code += _range & t;
            if (_code == _range)
                _failed = LzmaErrData;
            Normalize();
            result = (result << 1) + t + 1;
            --n;
        }
        return result;
    }

    auto BitTree(Probs &p, uint32_t base, int numBits) -> uint32_t
    {
        uint32_t m = 1;
        for (int i = 1; i <= numBits; ++i)
            m = (m << 1) + DecodeBit(p[base + m]);
        return m - (1u << numBits);
    }

    auto ReverseBitTree(Probs &p, uint32_t base, int numBits) -> uint32_t
    {
        uint32_t m = 1, result = 0;
        for (int i = 0; i < numBits; ++i)
        {
            const uint32_t bit = DecodeBit(p[base + m]);
            m = (m << 1) + bit;
            result |= bit << i;
        }
        return result;
    }

    auto DecodeLen(int coder, int posState) -> uint32_t
    {
        if (DecodeBit(_lenChoice[coder][0]) == 0)
            return BitTree(_lenLow[coder], static_cast<uint32_t>(posState) << 3, 3);
        if (DecodeBit(_lenChoice2[coder][0]) == 0)
            return 8 + BitTree(_lenMid[coder], static_cast<uint32_t>(posState) << 3, 3);
        return 16 + BitTree(_lenHigh[coder], 0, 8);
    }

    auto DecodeDistance(uint32_t len) -> uint32_t
    {
        const uint32_t lenState = len > 3 ? 3 : len;
        const uint32_t slot = BitTree(_posSlot, lenState << 6, 6);
        if (slot < 4)
            return slot;
        const int numDirect = static_cast<int>((slot >> 1) - 1);
        uint32_t result = (2 | (slot & 1)) << numDirect;
        if (slot < 14)
            result += ReverseBitTree(_posDecoders, result - slot, numDirect);
        else
        {
            result += DirectBits(numDirect - 4) << 4;
            result += ReverseBitTree(_align, 0, 4);
        }
        return result;
    }

    const uint8_t *_src;
    int _srcLength;
    uint8_t *_dst;
    int64_t _dstLength;
    int _ip = 0;
    uint32_t _range = 0, _code = 0;
    int _failed = 0;
    Probs _isMatch, _isRep, _isRepG0, _isRepG1, _isRepG2, _isRep0Long, _posSlot, _posDecoders, _align, _lit;
    std::array<Probs, 2> _lenChoice, _lenChoice2, _lenLow, _lenMid, _lenHigh;
};
} // namespace

auto LzmaAloneSize(const uint8_t *src, int srcLength) -> int64_t
{
    if (srcLength < 13)
        return -1;
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i)
        v = (v << 8) | src[5 + i];
    if (v >= static_cast<uint64_t>(INT64_MAX))
        return -1;
    return static_cast<int64_t>(v);
}

auto LzmaAloneDecode(const uint8_t *src, int srcLength, uint8_t *dst, int64_t dstLength) -> int
{
    return LzmaDecoder(src, srcLength, dst, dstLength).Run();
}
} // namespace Dud
