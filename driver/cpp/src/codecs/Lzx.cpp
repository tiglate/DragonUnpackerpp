// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The bits are read as 16 bits little endian words, most significant bit first; the three block
// types are verbatim, aligned offset and uncompressed; the x86 call translation (E8) is undone
// when the stream asks for it. Written from the description of the format (the Microsoft LZX
// specification, as implemented by libmspack).

#include "Lzx.h"

#include <array>
#include <memory>

namespace Dud
{
namespace
{
constexpr int WindowSize = 65536;
constexpr int WindowMask = WindowSize - 1;
constexpr int NumPosSlots = 32;
constexpr int MainElements = 256 + NumPosSlots * 8;
constexpr int LenElements = 249;
constexpr int PreElements = 20;
constexpr int AlignedElements = 8;
constexpr int MaxBits = 16;
constexpr int MinMatch = 2;

struct BadData
{
};

struct Huff
{
    std::array<int, MaxBits + 1> Count{};
    std::array<uint16_t, MainElements> Sym{};
    bool Valid = false;
};

class Lzx
{
  public:
    Lzx()
    {
        int j = 0;
        for (int i = 0; i <= 50; i += 2)
        {
            _extraBits[i] = static_cast<uint8_t>(j);
            _extraBits[i + 1] = static_cast<uint8_t>(j);
            if (i != 0 && j < 17)
                ++j;
        }
        j = 0;
        for (int i = 0; i <= 50; ++i)
        {
            _posBase[i] = static_cast<uint32_t>(j);
            j += 1 << _extraBits[i];
        }
    }

    // Decode one frame of count bytes into dst from the len bytes at src (pos: position of the frame
    // in the whole output)
    void Frame(const uint8_t *src, int64_t len, uint8_t *dst, int count, int64_t pos)
    {
        _src = src;
        _srcLen = len;
        _srcPos = 0;
        _bitBuf = 0;
        _bitsLeft = 0;
        if (!_headerRead)
        {
            _headerRead = true;
            if (Bits(1) != 0)
            {
                const uint32_t i = Bits(16);
                const uint32_t j = Bits(16);
                _intelFileSize = (i << 16) | j;
            }
        }
        const int start = _wpos;
        int togo = count;
        while (togo > 0)
        {
            if (_blockRemaining == 0)
                ReadBlockHeader();
            const int run = _blockRemaining > togo ? togo : static_cast<int>(_blockRemaining);
            if (_blockType == 3)
            {
                if (_srcPos + run > _srcLen)
                    throw BadData();
                for (int k = 1; k <= run; ++k)
                {
                    _window[_wpos] = _src[_srcPos];
                    _wpos = (_wpos + 1) & WindowMask;
                    ++_srcPos;
                }
            }
            else
                DecodeRun(run);
            _blockRemaining -= run;
            togo -= run;
        }
        Align();
        int p = start;
        for (int k = 0; k < count; ++k)
        {
            dst[k] = _window[p];
            p = (p + 1) & WindowMask;
        }
        if (_intelStarted && _intelFileSize != 0 && pos / 32768 <= 32768 && count > 10)
        {
            // undo the translation of the x86 call targets
            int64_t cur = pos;
            int k = 0;
            while (k < count - 10)
            {
                if (dst[k] != 0xE8)
                {
                    ++cur;
                    ++k;
                    continue;
                }
                ++k;
                const auto abs = static_cast<int32_t>(static_cast<uint32_t>(dst[k]) | (static_cast<uint32_t>(dst[k + 1]) << 8) |
                                                      (static_cast<uint32_t>(dst[k + 2]) << 16) |
                                                      (static_cast<uint32_t>(dst[k + 3]) << 24));
                if (abs >= -cur && abs < static_cast<int64_t>(_intelFileSize))
                {
                    const auto rel = static_cast<uint32_t>(abs >= 0 ? static_cast<int32_t>(abs - cur)
                                                                    : static_cast<int32_t>(abs + static_cast<int64_t>(_intelFileSize)));
                    dst[k] = rel & 0xFF;
                    dst[k + 1] = (rel >> 8) & 0xFF;
                    dst[k + 2] = (rel >> 16) & 0xFF;
                    dst[k + 3] = (rel >> 24) & 0xFF;
                }
                k += 4;
                cur += 5;
            }
        }
    }

  private:
    void Ensure(int n)
    {
        while (_bitsLeft < n)
        {
            uint32_t w;
            if (_srcPos + 2 > _srcLen)
                w = 0; // the end of the data reads as zeros
            else
                w = static_cast<uint32_t>(_src[_srcPos]) | (static_cast<uint32_t>(_src[_srcPos + 1]) << 8);
            _srcPos += 2;
            _bitBuf |= w << (16 - _bitsLeft);
            _bitsLeft += 16;
        }
    }

    // _bitBuf keeps the next bits at its top: 32 - _bitsLeft are free below them
    auto Bits(int n) -> uint32_t
    {
        if (n == 0)
            return 0;
        Ensure(n);
        const uint32_t result = _bitBuf >> (32 - n);
        _bitBuf <<= n;
        _bitsLeft -= n;
        return result;
    }

    void Build(Huff &h, const uint8_t *lens, int n)
    {
        h.Count.fill(0);
        for (int i = 0; i < n; ++i)
            ++h.Count[lens[i]];
        h.Count[0] = 0;
        int left = 1;
        for (int l = 1; l <= MaxBits; ++l)
        {
            left <<= 1;
            left -= h.Count[l];
            if (left < 0)
                throw BadData(); // over-subscribed
        }
        std::array<int, MaxBits + 2> offs{};
        offs[1] = 0;
        for (int l = 1; l <= MaxBits - 1; ++l)
            offs[l + 1] = offs[l] + h.Count[l];
        int used = 0;
        for (int i = 0; i < n; ++i)
            if (lens[i] != 0)
            {
                h.Sym[offs[lens[i]]] = static_cast<uint16_t>(i);
                ++offs[lens[i]];
                ++used;
            }
        h.Valid = used > 0;
    }

    auto Decode(const Huff &h) -> int
    {
        if (!h.Valid)
            throw BadData();
        int code = 0, first = 0, index = 0;
        for (int l = 1; l <= MaxBits; ++l)
        {
            code |= static_cast<int>(Bits(1));
            const int cnt = h.Count[l];
            if (code - cnt < first)
                return h.Sym[index + (code - first)];
            index += cnt;
            first += cnt;
            first <<= 1;
            code <<= 1;
        }
        throw BadData();
    }

    void ReadLengths(uint8_t *lens, int first, int last)
    {
        uint8_t preLens[PreElements];
        for (int x = 0; x < PreElements; ++x)
            preLens[x] = static_cast<uint8_t>(Bits(4));
        Build(_pre, preLens, PreElements);
        int x = first;
        while (x < last)
        {
            int z = Decode(_pre);
            if (z == 17)
            {
                const int y = static_cast<int>(Bits(4)) + 4;
                if (x + y > last)
                    throw BadData();
                for (int k = 1; k <= y; ++k)
                    lens[x++] = 0;
            }
            else if (z == 18)
            {
                const int y = static_cast<int>(Bits(5)) + 20;
                if (x + y > last)
                    throw BadData();
                for (int k = 1; k <= y; ++k)
                    lens[x++] = 0;
            }
            else if (z == 19)
            {
                const int y = static_cast<int>(Bits(1)) + 4;
                if (x + y > last)
                    throw BadData();
                z = Decode(_pre);
                z = lens[x] - z;
                if (z < 0)
                    z += 17;
                for (int k = 1; k <= y; ++k)
                    lens[x++] = static_cast<uint8_t>(z);
            }
            else
            {
                z = lens[x] - z;
                if (z < 0)
                    z += 17;
                lens[x++] = static_cast<uint8_t>(z);
            }
        }
    }

    void Align()
    {
        // re-align on a 16 bits boundary
        if (_bitsLeft > 0)
            Ensure(16);
        if ((_bitsLeft & 15) != 0)
            Bits(_bitsLeft & 15);
    }

    void ReadBlockHeader()
    {
        if (_blockType == 3 && _blockOdd)
            ++_srcPos; // padding after an uncompressed block of odd size
        _blockType = static_cast<int>(Bits(3));
        const uint32_t hi = Bits(16);
        const uint32_t lo = Bits(8);
        _blockRemaining = (static_cast<int64_t>(hi) << 8) | lo;
        _blockOdd = (_blockRemaining & 1) != 0;
        switch (_blockType)
        {
        case 1:
        case 2:
            if (_blockType == 2)
            {
                uint8_t al[AlignedElements];
                for (int i = 0; i < AlignedElements; ++i)
                    al[i] = static_cast<uint8_t>(Bits(3));
                Build(_aligned, al, AlignedElements);
            }
            ReadLengths(_mainLens.data(), 0, 256);
            ReadLengths(_mainLens.data(), 256, MainElements);
            Build(_main, _mainLens.data(), MainElements);
            if (_mainLens[0xE8] != 0)
                _intelStarted = true;
            ReadLengths(_lenLens.data(), 0, LenElements);
            Build(_lenTree, _lenLens.data(), LenElements);
            break;
        case 3:
            _intelStarted = true;
            if (_bitsLeft == 0)
                Ensure(16);
            _bitsLeft = 0;
            _bitBuf = 0;
            if (_srcPos + 12 > _srcLen)
                throw BadData();
            _r0 = Word32(_srcPos);
            _r1 = Word32(_srcPos + 4);
            _r2 = Word32(_srcPos + 8);
            _srcPos += 12;
            break;
        default:
            throw BadData();
        }
    }

    auto Word32(int64_t at) const -> uint32_t
    {
        return static_cast<uint32_t>(_src[at]) | (static_cast<uint32_t>(_src[at + 1]) << 8) |
               (static_cast<uint32_t>(_src[at + 2]) << 16) | (static_cast<uint32_t>(_src[at + 3]) << 24);
    }

    void DecodeRun(int run)
    {
        while (run > 0)
        {
            int elem = Decode(_main);
            if (elem < 256)
            {
                _window[_wpos] = static_cast<uint8_t>(elem);
                _wpos = (_wpos + 1) & WindowMask;
                --run;
                continue;
            }
            elem -= 256;
            int ml = elem & 7;
            if (ml == 7)
                ml += Decode(_lenTree);
            ml += MinMatch;
            const int mo = elem >> 3;
            uint32_t off;
            if (mo > 2)
            {
                if (mo != 3)
                {
                    const int extra = _extraBits[mo];
                    if (_blockType == 2 && extra >= 3)
                    {
                        if (extra > 3)
                        {
                            const int v = static_cast<int>(Bits(extra - 3)) << 3;
                            const int a = Decode(_aligned);
                            off = _posBase[mo] - 2 + static_cast<uint32_t>(v) + static_cast<uint32_t>(a);
                        }
                        else
                            off = _posBase[mo] - 2 + static_cast<uint32_t>(Decode(_aligned));
                    }
                    else if (extra > 0)
                        off = _posBase[mo] - 2 + Bits(extra);
                    else
                        off = _posBase[mo] - 2;
                }
                else
                    off = 1;
                _r2 = _r1;
                _r1 = _r0;
                _r0 = off;
            }
            else if (mo == 0)
                off = _r0;
            else if (mo == 1)
            {
                off = _r1;
                _r1 = _r0;
                _r0 = off;
            }
            else
            {
                off = _r2;
                _r2 = _r0;
                _r0 = off;
            }
            if (ml > run || off == 0 || off > static_cast<uint32_t>(WindowSize))
                throw BadData();
            int from = (_wpos - static_cast<int>(off)) & WindowMask;
            for (int k = 1; k <= ml; ++k)
            {
                _window[_wpos] = _window[from];
                _wpos = (_wpos + 1) & WindowMask;
                from = (from + 1) & WindowMask;
            }
            run -= ml;
        }
    }

    const uint8_t *_src = nullptr;
    int64_t _srcLen = 0, _srcPos = 0;
    uint32_t _bitBuf = 0;
    int _bitsLeft = 0;
    std::array<uint8_t, WindowSize> _window{};
    int _wpos = 0;
    uint32_t _r0 = 1, _r1 = 1, _r2 = 1;
    std::array<uint8_t, MainElements> _mainLens{};
    std::array<uint8_t, LenElements> _lenLens{};
    Huff _main, _lenTree, _pre, _aligned;
    int64_t _blockRemaining = 0;
    int _blockType = 0;
    bool _headerRead = false;
    bool _blockOdd = false;
    bool _intelStarted = false;
    uint32_t _intelFileSize = 0;
    std::array<uint8_t, 52> _extraBits{}; // the Pascal loop also writes index 51 (harmless there)
    std::array<uint32_t, 51> _posBase{};
};
} // namespace

auto LzxDecodeXna(const uint8_t *src, int64_t srcLength, uint8_t *dst, int64_t dstLength) -> bool
{
    try
    {
        auto lzx = std::make_unique<Lzx>();
        int64_t p = 0, outPos = 0;
        while (outPos < dstLength)
        {
            if (p + 2 > srcLength)
                return false;
            int frame, blk;
            if (src[p] == 0xFF)
            {
                if (p + 5 > srcLength)
                    return false;
                frame = (src[p + 1] << 8) | src[p + 2];
                blk = (src[p + 3] << 8) | src[p + 4];
                p += 5;
            }
            else
            {
                frame = 32768;
                blk = (src[p] << 8) | src[p + 1];
                p += 2;
            }
            if (blk == 0 || frame == 0 || p + blk > srcLength)
                return false;
            if (frame > dstLength - outPos)
                return false;
            lzx->Frame(src + p, blk, dst + outPos, frame, outPos);
            p += blk;
            outPos += frame;
        }
        return true;
    }
    catch (const BadData &)
    {
        return false;
    }
}
} // namespace Dud
