// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "VorbisOgg.h"

#include <array>
#include <cstring>

namespace Dud
{
namespace
{
struct EndOfSetup
{
};

class BitReader
{
  public:
    explicit BitReader(const std::string &text) : _s(text)
    {
    }

    auto Read(int n) -> int64_t
    {
        int64_t result = 0;
        for (int i = 0; i < n; ++i)
        {
            const int64_t b = _p >> 3;
            if (b >= static_cast<int64_t>(_s.size()))
                throw EndOfSetup();
            result |= static_cast<int64_t>(((static_cast<uint8_t>(_s[static_cast<size_t>(b)]) >> (_p & 7)) & 1))
                      << i;
            ++_p;
        }
        return result;
    }

  private:
    const std::string &_s;
    int64_t _p = 0;
};

auto ILog(int64_t n) -> int
{
    int result = 0;
    while (n > 0)
    {
        ++result;
        n >>= 1;
    }
    return result;
}

void Need(bool condition)
{
    if (!condition)
        throw EndOfSetup();
}

struct CrcTable
{
    std::array<uint32_t, 256> Values{};
    CrcTable()
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i << 24;
            for (int j = 1; j <= 8; ++j)
                c = (c & 0x80000000) != 0 ? (c << 1) ^ 0x04C11DB7 : c << 1;
            Values[i] = c;
        }
    }
};

auto OggCrc(const uint8_t *p, int len, uint32_t crc) -> uint32_t
{
    static const CrcTable table;
    uint32_t result = crc;
    for (int i = 0; i < len; ++i)
        result = (result << 8) ^ table.Values[((result >> 24) & 255) ^ p[i]];
    return result;
}

class Pager
{
  public:
    bool Dry = false;
    std::vector<uint8_t> *Out = nullptr;
    int64_t Total = 0;
    uint32_t Seq = 0;
    uint32_t Serial = 0;

    auto Fits(int n) const -> bool
    {
        return _nsegs + n / 255 + 1 <= 255;
    }

    // a whole packet into the current page
    void Add(const uint8_t *p, int n)
    {
        int k = n;
        while (k >= 255)
        {
            _segs[static_cast<size_t>(_nsegs++)] = 255;
            k -= 255;
        }
        _segs[static_cast<size_t>(_nsegs++)] = static_cast<uint8_t>(k);
        if (!Dry && n > 0)
            _body.insert(_body.end(), p, p + n);
        _bodyLen += n;
    }

    void Flush(uint8_t flags, int64_t granule)
    {
        Total += 27 + _nsegs + _bodyLen;
        if (!Dry)
        {
            uint8_t h[27] = {};
            std::memcpy(h, "OggS", 4);
            h[5] = flags;
            for (int i = 0; i < 8; ++i)
                h[6 + i] = static_cast<uint8_t>((static_cast<uint64_t>(granule) >> (8 * i)) & 255);
            for (int i = 0; i < 4; ++i)
            {
                h[14 + i] = static_cast<uint8_t>((Serial >> (8 * i)) & 255);
                h[18 + i] = static_cast<uint8_t>((Seq >> (8 * i)) & 255);
            }
            h[26] = static_cast<uint8_t>(_nsegs);
            uint32_t c = OggCrc(h, 27, 0);
            c = OggCrc(_segs.data(), _nsegs, c);
            if (_bodyLen > 0)
                c = OggCrc(_body.data(), _bodyLen, c);
            for (int i = 0; i < 4; ++i)
                h[22 + i] = static_cast<uint8_t>((c >> (8 * i)) & 255);
            Out->insert(Out->end(), h, h + 27);
            Out->insert(Out->end(), _segs.begin(), _segs.begin() + _nsegs);
            if (_bodyLen > 0)
                Out->insert(Out->end(), _body.begin(), _body.begin() + _bodyLen);
        }
        ++Seq;
        _nsegs = 0;
        _bodyLen = 0;
        _body.clear();
    }

  private:
    std::array<uint8_t, 255> _segs{};
    int _nsegs = 0;
    std::vector<uint8_t> _body;
    int _bodyLen = 0;
};
} // namespace

auto ParseVorbisSetup(const std::string &setup, int channels, VorbisModes &modes) -> bool
{
    modes = VorbisModes();
    BitReader r(setup);
    try
    {
        int cl[32] = {};
        int dims[16] = {};
        int casc[64] = {};
        Need(r.Read(8) == 5 && setup.size() > 8);
        for (int i = 1; i <= 6; ++i)
            r.Read(8); // "vorbis"
        const int numBooks = static_cast<int>(r.Read(8)) + 1;
        for (int i = 1; i <= numBooks; ++i)
        {
            Need(r.Read(24) == 0x564342);
            const int dim = static_cast<int>(r.Read(16));
            const int ent = static_cast<int>(r.Read(24));
            const int ordered = static_cast<int>(r.Read(1));
            if (ordered != 0)
            {
                r.Read(5);
                int64_t i64 = 0;
                while (i64 < ent)
                    i64 += r.Read(ILog(ent - i64));
            }
            else
            {
                const int sparse = static_cast<int>(r.Read(1));
                for (int j = 1; j <= ent; ++j)
                    if (sparse != 0)
                    {
                        if (r.Read(1) != 0)
                            r.Read(5);
                    }
                    else
                        r.Read(5);
            }
            const int lookupType = static_cast<int>(r.Read(4));
            if (lookupType == 1 || lookupType == 2)
            {
                r.Read(32);
                r.Read(32);
                const int valueBits = static_cast<int>(r.Read(4)) + 1;
                r.Read(1);
                int64_t v;
                if (lookupType == 1)
                {
                    v = 0;
                    int64_t i64;
                    do
                    {
                        i64 = 1;
                        for (int j = 1; j <= dim; ++j)
                        {
                            i64 = i64 * (v + 1);
                            if (i64 > ent)
                                break;
                        }
                        if (i64 <= ent)
                            ++v;
                    } while (i64 <= ent);
                }
                else
                    v = static_cast<int64_t>(ent) * dim;
                for (int64_t i64 = 1; i64 <= v; ++i64)
                    r.Read(valueBits);
            }
            else
                Need(lookupType == 0);
        }
        int k = static_cast<int>(r.Read(6)) + 1;
        for (int i = 1; i <= k; ++i)
            Need(r.Read(16) == 0);
        const int numFloors = static_cast<int>(r.Read(6)) + 1;
        for (int i = 1; i <= numFloors; ++i)
        {
            const int type = static_cast<int>(r.Read(16));
            if (type == 1)
            {
                const int parts = static_cast<int>(r.Read(5));
                int maxClass = 0;
                for (int j = 0; j < parts; ++j)
                {
                    cl[j] = static_cast<int>(r.Read(4));
                    if (cl[j] > maxClass)
                        maxClass = cl[j];
                }
                for (int j = 0; j <= maxClass; ++j)
                {
                    dims[j] = static_cast<int>(r.Read(3)) + 1;
                    const int sub = static_cast<int>(r.Read(2));
                    if (sub != 0)
                        r.Read(8);
                    for (k = 1; k <= 1 << sub; ++k)
                        r.Read(8);
                }
                r.Read(2);
                const int rangeBits = static_cast<int>(r.Read(4));
                for (int j = 0; j < parts; ++j)
                    for (k = 1; k <= dims[cl[j]]; ++k)
                        r.Read(rangeBits);
            }
            else if (type == 0)
            {
                r.Read(8);
                r.Read(16);
                r.Read(16);
                r.Read(6);
                r.Read(8);
                const int books = static_cast<int>(r.Read(4)) + 1;
                for (int j = 1; j <= books; ++j)
                    r.Read(8);
            }
            else
                Need(false);
        }
        const int numRes = static_cast<int>(r.Read(6)) + 1;
        for (int i = 1; i <= numRes; ++i)
        {
            Need(r.Read(16) <= 2);
            r.Read(24);
            r.Read(24);
            r.Read(24);
            const int cascades = static_cast<int>(r.Read(6)) + 1;
            r.Read(8);
            for (int j = 0; j < cascades; ++j)
            {
                k = static_cast<int>(r.Read(3));
                if (r.Read(1) != 0)
                    k += static_cast<int>(r.Read(5)) * 8;
                casc[j] = k;
            }
            for (int j = 0; j < cascades; ++j)
                for (k = 0; k <= 7; ++k)
                    if ((casc[j] & (1 << k)) != 0)
                        r.Read(8);
        }
        const int numMaps = static_cast<int>(r.Read(6)) + 1;
        for (int i = 1; i <= numMaps; ++i)
        {
            Need(r.Read(16) == 0);
            const int subs = r.Read(1) != 0 ? static_cast<int>(r.Read(4)) + 1 : 1;
            if (r.Read(1) != 0)
            {
                const int steps = static_cast<int>(r.Read(8)) + 1;
                for (int j = 1; j <= steps; ++j)
                {
                    r.Read(ILog(channels - 1));
                    r.Read(ILog(channels - 1));
                }
            }
            Need(r.Read(2) == 0);
            if (subs > 1)
                for (int j = 1; j <= channels; ++j)
                    r.Read(4);
            for (int j = 1; j <= subs; ++j)
            {
                r.Read(8);
                r.Read(8);
                r.Read(8);
            }
        }
        const int numModes = static_cast<int>(r.Read(6)) + 1;
        Need(numModes <= 64);
        modes.Count = numModes;
        for (int i = 0; i < numModes; ++i)
        {
            modes.Long[i] = r.Read(1) != 0;
            Need(r.Read(16) == 0 && r.Read(16) == 0);
            r.Read(8);
        }
        Need(r.Read(1) == 1);
        return true;
    }
    catch (const EndOfSetup &)
    {
        return false;
    }
}

auto VorbisToOgg(Reader &source, int64_t start, int64_t length, int channels, int rate, int64_t samples,
                 const std::string &setup, const VorbisModes &modes, bool dry, std::vector<uint8_t> *out)
    -> int64_t
{
    if (channels < 1 || channels > 255 || modes.Count < 1 || setup.empty())
        return -1;
    Pager pg;
    pg.Dry = dry;
    pg.Out = out;
    pg.Serial = 1;
    uint8_t ident[30] = {};
    ident[0] = 1;
    std::memcpy(ident + 1, "vorbis", 6);
    ident[11] = static_cast<uint8_t>(channels);
    for (int i = 0; i < 4; ++i)
        ident[12 + i] = static_cast<uint8_t>((static_cast<uint32_t>(rate) >> (8 * i)) & 255);
    ident[28] = 0xB8; // blocksizes: 256 (short) and 2048 (long)
    ident[29] = 1;
    uint8_t comment[16] = {};
    comment[0] = 3;
    std::memcpy(comment + 1, "vorbis", 6);
    comment[15] = 1;
    pg.Add(ident, 30);
    pg.Flush(2, 0);
    pg.Add(comment, 16);
    pg.Add(reinterpret_cast<const uint8_t *>(setup.data()), static_cast<int>(setup.size()));
    pg.Flush(0, 0);
    const int modeBits = ILog(modes.Count - 1);
    int prev = 0;
    int64_t cum = 0;
    int64_t granule = 0;
    bool havePage = false;
    // the whole sound is read at once (a few hundred KB at most)
    if (length < 0)
        return -1;
    std::vector<uint8_t> data(static_cast<size_t>(length) + 8);
    if (length > 0)
    {
        source.Seek(start);
        if (source.Read(data.data(), static_cast<size_t>(length)) != static_cast<size_t>(length))
            return -1;
    }
    int64_t pos = 0;
    const int64_t endPos = length;
    while (pos + 2 <= endPos)
    {
        const int n = data[static_cast<size_t>(pos)] | (data[static_cast<size_t>(pos) + 1] << 8);
        if (n == 0 || n == 0xFFFF || pos + 2 + n > endPos)
            break;
        pos += 2;
        if (!dry)
        {
            if ((data[static_cast<size_t>(pos)] & 1) != 0)
                return -1; // not an audio packet
            const int mode = modeBits > 0 ? (data[static_cast<size_t>(pos)] >> 1) & ((1 << modeBits) - 1) : 0;
            if (mode >= modes.Count)
                return -1;
            const int bs = modes.Long[mode] ? 2048 : 256;
            if (prev != 0)
                cum += prev / 4 + bs / 4;
            prev = bs;
        }
        if (!pg.Fits(n))
        {
            if (granule > samples)
                granule = samples; // (a page never gives more than the sound has)
            pg.Flush(0, granule);
            havePage = false;
        }
        pg.Add(&data[static_cast<size_t>(pos)], n);
        havePage = true;
        granule = cum;
        pos += n;
    }
    if (granule > samples)
        granule = samples;
    if (havePage)
        pg.Flush(4, granule);
    else
        pg.Flush(4, 0);
    return pg.Total;
}
} // namespace Dud
