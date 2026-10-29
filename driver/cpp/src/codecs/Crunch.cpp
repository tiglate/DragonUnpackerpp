// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Crunch.h"

#include <array>
#include <vector>

namespace Dud
{
namespace
{
constexpr int MaxCodeSize = 16;
constexpr int MaxSyms = 8192;
constexpr int CodelenCodes = 21;
constexpr int HeaderMin = 74;
constexpr uint8_t Probable[21] = {17, 18, 19, 20, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15, 16};
constexpr uint8_t Dxt5FromLinear[8] = {0, 2, 3, 4, 5, 6, 7, 1};

struct BadCrunch
{
};

struct Huff
{
    int Total = 0; // symbols of the model
    int Used = 0;
    std::array<uint32_t, MaxCodeSize + 1> MaxCodes{}; // left justified on 16 bits, + 1 (0: no code of that size)
    std::array<int, MaxCodeSize + 1> ValPtr{};
    std::vector<uint16_t> Sorted;
    std::vector<uint8_t> Sizes;
};

struct Palette
{
    int Ofs = 0, Size = 0, Num = 0;
};

struct Header
{
    int HeaderSize = 0, DataSize = 0, Width = 0, Height = 0, Levels = 0, Faces = 0, Format = 0, Flags = 0;
    Palette Color, ColorSel, Alpha, AlphaSel;
    int TablesSize = 0, TablesOfs = 0;
    std::vector<int> LevelOfs;
};

auto BE(const uint8_t *p, int n) -> uint32_t
{
    uint32_t result = 0;
    for (int i = 0; i < n; ++i)
        result = (result << 8) | p[i];
    return result;
}

// avail: bytes at src that can be read; len: size of the whole file (the streams are checked against it)
auto ReadHeader(const uint8_t *src, int64_t avail, int64_t len, Header &h) -> bool
{
    if (src == nullptr || avail < HeaderMin)
        return false;
    if (src[0] != 'H' || src[1] != 'x')
        return false;
    const auto pal = [&](int off, Palette &q) {
        q.Ofs = static_cast<int>(BE(src + off, 3));
        q.Size = static_cast<int>(BE(src + off + 3, 3));
        q.Num = static_cast<int>(BE(src + off + 6, 2));
    };
    h.HeaderSize = static_cast<int>(BE(src + 2, 2));
    h.DataSize = static_cast<int32_t>(BE(src + 6, 4));
    h.Width = static_cast<int>(BE(src + 12, 2));
    h.Height = static_cast<int>(BE(src + 14, 2));
    h.Levels = src[16];
    h.Faces = src[17];
    h.Format = src[18];
    h.Flags = static_cast<int>(BE(src + 19, 2));
    pal(33, h.Color);
    pal(41, h.ColorSel);
    pal(49, h.Alpha);
    pal(57, h.AlphaSel);
    h.TablesSize = static_cast<int>(BE(src + 65, 2));
    h.TablesOfs = static_cast<int>(BE(src + 67, 3));
    if (h.HeaderSize < HeaderMin || h.DataSize > len || h.DataSize < h.HeaderSize || h.Levels < 1 || h.Levels > 16 ||
        h.Faces != 1 || h.Width < 1 || h.Height < 1 || h.Width > 8192 || h.Height > 8192)
        return false;
    if (h.HeaderSize < 70 + 4 * h.Levels || avail < 70 + 4 * h.Levels)
        return false;
    h.LevelOfs.resize(static_cast<size_t>(h.Levels));
    for (int i = 0; i < h.Levels; ++i)
        h.LevelOfs[static_cast<size_t>(i)] = static_cast<int32_t>(BE(src + 70 + 4 * i, 4));
    // every stream must be inside the file
    if (static_cast<int64_t>(h.TablesOfs) + h.TablesSize > len || static_cast<int64_t>(h.Color.Ofs) + h.Color.Size > len ||
        static_cast<int64_t>(h.ColorSel.Ofs) + h.ColorSel.Size > len ||
        static_cast<int64_t>(h.Alpha.Ofs) + h.Alpha.Size > len ||
        static_cast<int64_t>(h.AlphaSel.Ofs) + h.AlphaSel.Size > len)
        return false;
    for (int i = 0; i < h.Levels; ++i)
        if (h.LevelOfs[static_cast<size_t>(i)] >= len ||
            (i > 0 && h.LevelOfs[static_cast<size_t>(i)] <= h.LevelOfs[static_cast<size_t>(i) - 1]))
            return false;
    return h.Format == 0 || (h.Format >= 2 && h.Format <= 6);
}

void BuildHuff(Huff &h)
{
    std::array<int, MaxCodeSize + 1> num{};
    std::array<int, MaxCodeSize + 1> pos{};
    for (int i = 0; i < h.Total; ++i)
        if (h.Sizes[static_cast<size_t>(i)] > 0)
        {
            if (h.Sizes[static_cast<size_t>(i)] > MaxCodeSize)
                throw BadCrunch();
            ++num[h.Sizes[static_cast<size_t>(i)]];
        }
    int cur = 0;
    int usedSyms = 0;
    for (int l = 1; l <= MaxCodeSize; ++l)
    {
        const int n = num[static_cast<size_t>(l)];
        if (n == 0)
            h.MaxCodes[static_cast<size_t>(l)] = 0;
        else
        {
            h.MaxCodes[static_cast<size_t>(l)] =
                1 + ((static_cast<uint32_t>(cur + n - 1) << (16 - l)) | static_cast<uint32_t>((1 << (16 - l)) - 1));
            h.ValPtr[static_cast<size_t>(l)] = usedSyms - cur;
            pos[static_cast<size_t>(l)] = usedSyms;
            cur += n;
            usedSyms += n;
        }
        cur <<= 1;
    }
    if (usedSyms == 0)
        throw BadCrunch();
    h.Used = usedSyms;
    h.Sorted.assign(static_cast<size_t>(usedSyms), 0);
    for (int i = 0; i < h.Total; ++i)
        if (h.Sizes[static_cast<size_t>(i)] > 0)
        {
            h.Sorted[static_cast<size_t>(pos[h.Sizes[static_cast<size_t>(i)]])] = static_cast<uint16_t>(i);
            ++pos[h.Sizes[static_cast<size_t>(i)]];
        }
}

class Bits
{
  public:
    void Start(const uint8_t *src, int64_t len)
    {
        if (len <= 0)
            throw BadCrunch();
        _p = src;
        _e = src + len;
        _buf = 0;
        _cnt = 0;
    }

    // N <= 16 bits
    auto Get(int n) -> uint32_t
    {
        if (n == 0)
            return 0;
        while (_cnt < n)
        {
            uint32_t c = 0;
            if (_p != _e)
                c = *_p++;
            _cnt += 8;
            _buf |= c << (32 - _cnt);
        }
        const uint32_t result = _buf >> (32 - n);
        _buf <<= n;
        _cnt -= n;
        return result;
    }

    auto Read(int n) -> uint32_t
    {
        if (n > 16)
        {
            const uint32_t a = Get(n - 16);
            const uint32_t b = Get(16);
            return (a << 16) | b;
        }
        return Get(n);
    }

    auto Decode(const Huff &h) -> int
    {
        if (_cnt < 24)
        {
            if (_cnt < 16)
            {
                uint32_t c0 = 0, c1 = 0;
                if (_p < _e)
                    c0 = *_p++;
                if (_p < _e)
                    c1 = *_p++;
                _cnt += 16;
                _buf |= ((c0 << 8) | c1) << (32 - _cnt);
            }
            else
            {
                uint32_t c0 = 0;
                if (_p < _e)
                    c0 = *_p++;
                _cnt += 8;
                _buf |= c0 << (32 - _cnt);
            }
        }
        const uint32_t k = (_buf >> 16) + 1;
        int l = 1;
        while (l <= MaxCodeSize && k > h.MaxCodes[static_cast<size_t>(l)])
            ++l;
        if (l > MaxCodeSize)
            throw BadCrunch();
        const int idx = h.ValPtr[static_cast<size_t>(l)] + static_cast<int>(_buf >> (32 - l));
        if (idx < 0 || idx >= h.Used)
            throw BadCrunch();
        const int result = h.Sorted[static_cast<size_t>(idx)];
        _buf <<= l;
        _cnt -= l;
        return result;
    }

    // The table of a model: number of symbols, then the code lengths (themselves coded)
    void Model(Huff &h)
    {
        const int used = static_cast<int>(Read(14));
        if (used == 0)
        {
            h.Total = 0;
            return;
        }
        if (used > MaxSyms)
            throw BadCrunch();
        h.Total = used;
        h.Sizes.assign(static_cast<size_t>(used), 0);
        const int numSend = static_cast<int>(Read(5));
        if (numSend < 1 || numSend > CodelenCodes)
            throw BadCrunch();
        Huff dm;
        dm.Total = CodelenCodes;
        dm.Sizes.assign(CodelenCodes, 0);
        for (int i = 0; i < numSend; ++i)
            dm.Sizes[Probable[i]] = static_cast<uint8_t>(Read(3));
        BuildHuff(dm);
        int ofs = 0;
        while (ofs < used)
        {
            const int rem = used - ofs;
            const int code = Decode(dm);
            if (code <= 16)
            {
                h.Sizes[static_cast<size_t>(ofs)] = static_cast<uint8_t>(code);
                ++ofs;
            }
            else if (code == 17)
            {
                const int len = static_cast<int>(Read(3)) + 3;
                if (len > rem)
                    throw BadCrunch();
                ofs += len;
            }
            else if (code == 18)
            {
                const int len = static_cast<int>(Read(7)) + 11;
                if (len > rem)
                    throw BadCrunch();
                ofs += len;
            }
            else
            {
                const int len = code == 19 ? static_cast<int>(Read(2)) + 3 : static_cast<int>(Read(6)) + 7;
                if (ofs == 0 || len > rem)
                    throw BadCrunch();
                const uint8_t prev = h.Sizes[static_cast<size_t>(ofs) - 1];
                if (prev == 0)
                    throw BadCrunch();
                for (int i = 1; i <= len; ++i)
                    h.Sizes[static_cast<size_t>(ofs++)] = prev;
            }
        }
        BuildHuff(h);
    }

  private:
    const uint8_t *_p = nullptr;
    const uint8_t *_e = nullptr;
    uint32_t _buf = 0;
    int _cnt = 0;
};

void PutDW(uint8_t *p, uint32_t v)
{
    p[0] = v & 255;
    p[1] = (v >> 8) & 255;
    p[2] = (v >> 16) & 255;
    p[3] = (v >> 24) & 255;
}

class Unpacker
{
  public:
    Unpacker(const uint8_t *src, int64_t len) : _src(src), _len(len)
    {
        if (!ReadHeader(_src, _len, _len, H))
            throw BadCrunch();
    }

    Header H;

    void InitTables()
    {
        _bits.Start(_src + H.TablesOfs, H.TablesSize);
        _bits.Model(_refDM);
        if (H.Color.Num == 0 && H.Alpha.Num == 0)
            throw BadCrunch();
        if (H.Color.Num != 0)
        {
            _bits.Model(_endDM[0]);
            _bits.Model(_selDM[0]);
        }
        if (H.Alpha.Num != 0)
        {
            _bits.Model(_endDM[1]);
            _bits.Model(_selDM[1]);
        }
    }

    void DecodePalettes()
    {
        Huff dm[2], dm2[2];
        if (H.Color.Num != 0)
        {
            // endpoints
            _colorEnd.assign(static_cast<size_t>(H.Color.Num), 0);
            _bits.Start(_src + H.Color.Ofs, H.Color.Size);
            _bits.Model(dm[0]);
            _bits.Model(dm[1]);
            uint32_t a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
            for (int i = 0; i < H.Color.Num; ++i)
            {
                a = (a + static_cast<uint32_t>(_bits.Decode(dm[0]))) & 31;
                b = (b + static_cast<uint32_t>(_bits.Decode(dm[1]))) & 63;
                c = (c + static_cast<uint32_t>(_bits.Decode(dm[0]))) & 31;
                d = (d + static_cast<uint32_t>(_bits.Decode(dm[0]))) & 31;
                e = (e + static_cast<uint32_t>(_bits.Decode(dm[1]))) & 63;
                f = (f + static_cast<uint32_t>(_bits.Decode(dm[0]))) & 31;
                _colorEnd[static_cast<size_t>(i)] = c | (b << 5) | (a << 11) | (f << 16) | (e << 21) | (d << 27);
            }
            // selectors
            _colorSel.assign(static_cast<size_t>(H.ColorSel.Num), 0);
            _bits.Start(_src + H.ColorSel.Ofs, H.ColorSel.Size);
            _bits.Model(dm2[0]);
            uint32_t s = 0;
            for (int i = 0; i < H.ColorSel.Num; ++i)
            {
                for (int j = 0; j < 32; j += 4)
                    s ^= static_cast<uint32_t>(_bits.Decode(dm2[0])) << j;
                _colorSel[static_cast<size_t>(i)] = ((s ^ (s << 1)) & 0xAAAAAAAA) | ((s >> 1) & 0x55555555);
            }
        }
        if (H.Alpha.Num != 0)
        {
            _alphaEnd.assign(static_cast<size_t>(H.Alpha.Num), 0);
            _bits.Start(_src + H.Alpha.Ofs, H.Alpha.Size);
            _bits.Model(dm[0]);
            uint32_t a = 0, b = 0;
            for (int i = 0; i < H.Alpha.Num; ++i)
            {
                a = (a + static_cast<uint32_t>(_bits.Decode(dm[0]))) & 255;
                b = (b + static_cast<uint32_t>(_bits.Decode(dm[0]))) & 255;
                _alphaEnd[static_cast<size_t>(i)] = static_cast<uint16_t>(a | (b << 8));
            }
            _alphaSel.assign(static_cast<size_t>(H.AlphaSel.Num) * 3, 0);
            _bits.Start(_src + H.AlphaSel.Ofs, H.AlphaSel.Size);
            _bits.Model(dm2[0]);
            uint8_t lin[64];
            for (int i = 0; i < 64; ++i)
                lin[i] = static_cast<uint8_t>(Dxt5FromLinear[i & 7] | (Dxt5FromLinear[i >> 3] << 3));
            uint32_t s0Lin = 0, s1Lin = 0;
            for (size_t i = 0; i < _alphaSel.size(); i += 3)
            {
                uint32_t s0 = 0, s1 = 0;
                for (int j = 0; j < 24; j += 6)
                {
                    s0Lin ^= static_cast<uint32_t>(_bits.Decode(dm2[0])) << j;
                    s0 |= static_cast<uint32_t>(lin[(s0Lin >> j) & 0x3F]) << j;
                }
                for (int j = 0; j < 24; j += 6)
                {
                    s1Lin ^= static_cast<uint32_t>(_bits.Decode(dm2[0])) << j;
                    s1 |= static_cast<uint32_t>(lin[(s1Lin >> j) & 0x3F]) << j;
                }
                _alphaSel[i] = static_cast<uint16_t>(s0);
                _alphaSel[i + 1] = static_cast<uint16_t>((s0 >> 16) | (s1 << 8));
                _alphaSel[i + 2] = static_cast<uint16_t>(s1 >> 8);
            }
        }
    }

    void Level(int l, uint8_t *dst, int64_t dstLen)
    {
        int w = H.Width >> l;
        int hh = H.Height >> l;
        if (w < 1)
            w = 1;
        if (hh < 1)
            hh = 1;
        const int bx = (w + 3) >> 2;
        const int by = (hh + 3) >> 2;
        const bool isDxt5 = H.Format != 0;
        const int blockBytes = isDxt5 ? 16 : 8;
        const int pitch = bx * blockBytes;
        if (dstLen < static_cast<int64_t>(pitch) * by)
            throw BadCrunch();
        const int64_t levelOfs = H.LevelOfs[static_cast<size_t>(l)];
        const int64_t nextOfs = l + 1 < H.Levels ? H.LevelOfs[static_cast<size_t>(l) + 1] : _len;
        if (nextOfs <= levelOfs)
            throw BadCrunch();
        _bits.Start(_src + levelOfs, nextOfs - levelOfs);
        const int width = (bx + 1) & ~1;
        const int height = (by + 1) & ~1;
        std::vector<uint16_t> bufRef(static_cast<size_t>(width)), bufColor(static_cast<size_t>(width)),
            bufAlpha(static_cast<size_t>(width));
        const int nc = static_cast<int>(_colorEnd.size());
        const int na = static_cast<int>(_alphaEnd.size());
        if (nc == 0 || _colorSel.empty())
            throw BadCrunch();
        if (isDxt5 && (na == 0 || _alphaSel.empty()))
            throw BadCrunch();
        int colorIdx = 0, alphaIdx = 0, refGroup = 0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                const bool visible = y < by && x < bx;
                if ((y & 1) == 0 && (x & 1) == 0)
                    refGroup = _bits.Decode(_refDM);
                int ref;
                if ((y & 1) != 0)
                    ref = bufRef[static_cast<size_t>(x)];
                else
                {
                    ref = refGroup & 3;
                    refGroup >>= 2;
                    bufRef[static_cast<size_t>(x)] = static_cast<uint16_t>(refGroup & 3);
                    refGroup >>= 2;
                }
                if (ref == 0)
                {
                    colorIdx += _bits.Decode(_endDM[0]);
                    if (colorIdx >= nc)
                        colorIdx -= nc;
                    bufColor[static_cast<size_t>(x)] = static_cast<uint16_t>(colorIdx);
                    if (isDxt5)
                    {
                        alphaIdx += _bits.Decode(_endDM[1]);
                        if (alphaIdx >= na)
                            alphaIdx -= na;
                        bufAlpha[static_cast<size_t>(x)] = static_cast<uint16_t>(alphaIdx);
                    }
                }
                else if (ref == 1)
                {
                    bufColor[static_cast<size_t>(x)] = static_cast<uint16_t>(colorIdx);
                    bufAlpha[static_cast<size_t>(x)] = static_cast<uint16_t>(alphaIdx);
                }
                else
                {
                    colorIdx = bufColor[static_cast<size_t>(x)];
                    alphaIdx = bufAlpha[static_cast<size_t>(x)];
                }
                const int colorSelIdx = _bits.Decode(_selDM[0]);
                const int alphaSelIdx = isDxt5 ? _bits.Decode(_selDM[1]) : 0;
                if (visible)
                {
                    if (colorIdx >= nc || colorSelIdx >= static_cast<int>(_colorSel.size()))
                        throw BadCrunch();
                    uint8_t *q = dst + static_cast<int64_t>(y) * pitch + x * blockBytes;
                    if (isDxt5)
                    {
                        if (alphaIdx >= na || alphaSelIdx * 3 + 2 >= static_cast<int>(_alphaSel.size()))
                            throw BadCrunch();
                        const size_t sel = static_cast<size_t>(alphaSelIdx) * 3;
                        PutDW(q, static_cast<uint32_t>(_alphaEnd[static_cast<size_t>(alphaIdx)]) |
                                     (static_cast<uint32_t>(_alphaSel[sel]) << 16));
                        PutDW(q + 4, static_cast<uint32_t>(_alphaSel[sel + 1]) |
                                         (static_cast<uint32_t>(_alphaSel[sel + 2]) << 16));
                        PutDW(q + 8, _colorEnd[static_cast<size_t>(colorIdx)]);
                        PutDW(q + 12, _colorSel[static_cast<size_t>(colorSelIdx)]);
                    }
                    else
                    {
                        PutDW(q, _colorEnd[static_cast<size_t>(colorIdx)]);
                        PutDW(q + 4, _colorSel[static_cast<size_t>(colorSelIdx)]);
                    }
                }
            }
    }

  private:
    const uint8_t *_src;
    int64_t _len;
    Bits _bits;
    Huff _refDM;
    Huff _endDM[2], _selDM[2];
    std::vector<uint32_t> _colorEnd, _colorSel;
    std::vector<uint16_t> _alphaEnd, _alphaSel;
};
} // namespace

auto CrunchInfo(const uint8_t *src, int64_t avail, int64_t len, CrunchHeaderInfo &info) -> bool
{
    info = CrunchHeaderInfo();
    Header h;
    const bool result = ReadHeader(src, avail, len, h);
    if (result)
    {
        info.Format = h.Format;
        info.Width = h.Width;
        info.Height = h.Height;
        info.Levels = h.Levels;
        info.Faces = h.Faces;
    }
    return result;
}

auto CrunchLevelSize(const CrunchHeaderInfo &info, int level) -> int64_t
{
    int w = info.Width >> level;
    int h = info.Height >> level;
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    int64_t result = static_cast<int64_t>((w + 3) >> 2) * ((h + 3) >> 2);
    return info.Format == 0 ? result * 8 : result * 16;
}

auto CrunchUnpack(const uint8_t *src, int64_t len, int level, uint8_t *dst, int64_t dstLen) -> bool
{
    try
    {
        Unpacker u(src, len);
        if (level < 0 || level >= u.H.Levels)
            return false;
        u.InitTables();
        u.DecodePalettes();
        u.Level(level, dst, dstLen);
        return true;
    }
    catch (const BadCrunch &)
    {
        return false;
    }
}
} // namespace Dud
