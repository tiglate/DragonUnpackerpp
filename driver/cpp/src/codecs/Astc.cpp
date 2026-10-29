// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Astc.h"

#include <array>
#include <cmath>
#include <cstring>
#include <memory>

namespace Dud
{
namespace
{
constexpr int WeightA[16] = {0, 0, 0, 3, 0, 5, 3, 0, 0, 0, 5, 3, 0, 5, 3, 0};
constexpr int WeightB[16] = {0, 0, 1, 0, 2, 0, 1, 3, 0, 0, 1, 2, 4, 2, 3, 5};
constexpr int CemA[19] = {0, 3, 5, 0, 3, 5, 0, 3, 5, 0, 3, 5, 0, 3, 5, 0, 3, 0, 0};
constexpr int CemB[19] = {8, 6, 5, 7, 5, 4, 6, 4, 3, 5, 3, 2, 4, 2, 1, 3, 1, 2, 1};
constexpr int Mt[5] = {0, 2, 4, 5, 7};
constexpr int Mq[3] = {0, 3, 5};
#include "AstcTables.inc"
constexpr int EndTrits[7] = {0, 204, 93, 44, 22, 11, 5};
constexpr int EndQuints[6] = {0, 113, 54, 26, 13, 6};
constexpr bool FuncC[16] = {false, false, true, true, false, false, false, true,
                            false, false, false, true, false, false, true, true};
constexpr bool FuncA[16] = {false, false, true, true, false, false, false, true,
                            false, false, false, true, false, false, false, true};

struct BadBlock
{
};

struct Seq
{
    int Bits = 0, NonBits = 0;
};
using SeqArr = std::array<Seq, 256>;

struct BlockData
{
    int BW = 0, BH = 0, Width = 0, Height = 0, PartNum = 0, DualPlane = 0, PlaneSel = 0, WeightRange = 0,
        WeightNum = 0;
    int Cem[4] = {};
    int CemRange = 0, EndpointValueNum = 0;
    int Endpoints[4][8] = {};
    int Weights[144][2] = {};
    int Partition[144] = {};
};

struct BitRevTable
{
    uint8_t Values[256];
    BitRevTable()
    {
        for (int i = 0; i < 256; ++i)
        {
            Values[i] = 0;
            for (int j = 0; j < 8; ++j)
                if ((i & (1 << j)) != 0)
                    Values[i] = static_cast<uint8_t>(Values[i] | (1 << (7 - j)));
        }
    }
};

auto Clamp8(int n) -> int
{
    return n < 0 ? 0 : n > 255 ? 255 : n;
}

auto ClampHdr(int n) -> int
{
    return n < 0 ? 0 : n > 0xFFF ? 0xFFF : n;
}

auto Pack(int r, int g, int b, int a) -> uint32_t
{
    return static_cast<uint32_t>(b & 255) | (static_cast<uint32_t>(g & 255) << 8) |
           (static_cast<uint32_t>(r & 255) << 16) | (static_cast<uint32_t>(a & 255) << 24);
}

auto Half(uint16_t h) -> float
{
    const int e = (h >> 10) & 0x1F;
    const int m = h & 0x3FF;
    float s;
    if (e == 0)
        s = std::ldexp(static_cast<float>(m), -24); // 2^-24 per unit of mantissa
    else if (e == 31)
    {
        const uint32_t bits = m == 0 ? 0x7F800000u : 0x7FC00000u;
        std::memcpy(&s, &bits, 4);
        if ((h & 0x8000) != 0)
            s = -s;
        return s;
    }
    else
    {
        const uint32_t bits = (static_cast<uint32_t>(e - 15 + 127) << 23) | (static_cast<uint32_t>(m) << 13);
        std::memcpy(&s, &bits, 4);
    }
    if ((h & 0x8000) != 0)
        s = -s;
    return s;
}

auto IsFinite(float f) -> bool
{
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return ((b >> 23) & 0xFF) != 0xFF;
}

// Trunc(F + 0.5) of the Pascal version, in single precision
auto RoundF(float f) -> int
{
    if (f >= 0)
        return static_cast<int>(f + 0.5F);
    return -static_cast<int>(-f + 0.5F);
}

auto F32ToU8(float f) -> int
{
    uint32_t b;
    std::memcpy(&b, &f, 4);
    if (((b >> 23) & 0xFF) == 0xFF)
    {
        // not finite: no arithmetic on it
        if ((b & 0x7FFFFF) != 0)
            return 0;
        if ((b >> 31) != 0)
            return 0;
        return 255;
    }
    const float c = f * 255;
    if (c < 0)
        return 0;
    if (c > 255)
        return 255;
    return RoundF(c);
}

auto F16ToU8(const uint8_t *p) -> int
{
    return F32ToU8(Half(static_cast<uint16_t>(p[0] | (p[1] << 8))));
}

auto SelectColor(int v0, int v1, int w) -> int
{
    return (((((v0 << 8) | v0) * (64 - w) + ((v1 << 8) | v1) * w + 32) >> 6) * 255 + 32768) / 65536;
}

auto SelectColorHdr(int v0, int v1, int w) -> int
{
    const int c = (((v0 << 4) * (64 - w) + (v1 << 4) * w + 32) >> 6) & 0xFFFF;
    int m = c & 0x7FF;
    if (m < 512)
        m = m * 3;
    else if (m < 1536)
        m = 4 * m - 512;
    else
        m = 5 * m - 2048;
    m &= 0xFFFF;
    const float f = Half(static_cast<uint16_t>((((c >> 1) & 0x7C00) | (m >> 3)) & 0xFFFF));
    if (IsFinite(f))
        return Clamp8(RoundF(f * 255));
    return 255;
}

void SetEp(int *e, int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2)
{
    e[0] = r1;
    e[1] = g1;
    e[2] = b1;
    e[3] = a1;
    e[4] = r2;
    e[5] = g2;
    e[6] = b2;
    e[7] = a2;
}

void SetEpClamp(int *e, int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2)
{
    SetEp(e, Clamp8(r1), Clamp8(g1), Clamp8(b1), Clamp8(a1), Clamp8(r2), Clamp8(g2), Clamp8(b2), Clamp8(a2));
}

// (the sums are shifted as the 64 bits expression of the Pascal version: an arithmetic shift)
void SetEpBlue(int *e, int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2)
{
    SetEp(e, (r1 + b1) >> 1, (g1 + b1) >> 1, b1, a1, (r2 + b2) >> 1, (g2 + b2) >> 1, b2, a2);
}

void SetEpBlueClamp(int *e, int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2)
{
    SetEp(e, Clamp8((r1 + b1) >> 1), Clamp8((g1 + b1) >> 1), Clamp8(b1), Clamp8(a1), Clamp8((r2 + b2) >> 1),
          Clamp8((g2 + b2) >> 1), Clamp8(b2), Clamp8(a2));
}

void SetEpHdrClamp(int *e, int r1, int g1, int b1, int a1, int r2, int g2, int b2, int a2)
{
    SetEp(e, ClampHdr(r1), ClampHdr(g1), ClampHdr(b1), ClampHdr(a1), ClampHdr(r2), ClampHdr(g2), ClampHdr(b2),
          ClampHdr(a2));
}

// arithmetic shift right of a possibly negative number
auto Sar(int v, int n) -> int
{
    if (v >= 0)
        return static_cast<int>(static_cast<uint32_t>(v) >> n);
    return -static_cast<int>(static_cast<uint32_t>(-v + (1 << n) - 1) >> n);
}

void BitTransferSigned(int &a, int &b)
{
    b = static_cast<int>(static_cast<uint32_t>(b) >> 1) | (a & 0x80);
    a = static_cast<int>(static_cast<uint32_t>(a) >> 1) & 0x3F;
    if ((a & 0x20) != 0)
        a -= 0x40;
}

auto BitRev64(uint64_t d, int bits) -> uint64_t
{
    uint64_t result = 0;
    for (int i = 0; i < bits; ++i)
        if (((d >> i) & 1) != 0)
            result |= static_cast<uint64_t>(1) << (bits - 1 - i);
    return result;
}

class Decoder
{
  public:
    Decoder(const uint8_t *buf)
    {
        for (int i = 7; i >= 0; --i)
        {
            _lo = (_lo << 8) | buf[i];
            _hi = (_hi << 8) | buf[8 + i];
        }
    }

    void DecodeAll(BlockData &bd, uint32_t *px)
    {
        DecodeBlockParams(bd);
        DecodeEndpoints(bd);
        DecodeWeights(bd);
        if (bd.PartNum > 1)
            SelectPartition(bd);
        ApplyColor(bd, px);
    }

  private:
    // len bits from the position bit of the block (bits outside of it read as 0)
    auto GB(int bit, int len) const -> uint64_t
    {
        if (len < 1)
            return 0;
        const uint64_t mask = len >= 64 ? ~static_cast<uint64_t>(0) : (static_cast<uint64_t>(1) << len) - 1;
        if (bit >= 128)
            return 0;
        if (bit >= 64)
            return (_hi >> (bit - 64)) & mask;
        if (bit <= 0)
        {
            if (-bit >= 64)
                return 0;
            return (_lo << (-bit)) & mask;
        }
        if (bit + len <= 64)
            return (_lo >> bit) & mask;
        return ((_lo >> bit) | (_hi << (64 - bit))) & mask;
    }

    // the same, for a read that the format only allows inside of the block
    auto GBS(int bit, int len) const -> int
    {
        if (bit < 0 || bit + len > 128)
            throw BadBlock();
        return static_cast<int>(GB(bit, len));
    }

    void DecodeIntSeq(int offset, int a, int b, int count, bool reverse, SeqArr &out, int maxOut) const
    {
        static const BitRevTable bitRev;
        if (count <= 0)
            return;
        if (count > maxOut)
            throw BadBlock();
        int n = 0;
        if (a == 3)
        {
            const int mask = (1 << b) - 1;
            const int blockCount = (count + 4) / 5;
            const int lastBlockCount = (count + 4) % 5 + 1;
            const int blockSize = 8 + 5 * b;
            const int lastBlockSize = (blockSize * lastBlockCount + 4) / 5;
            int p = offset;
            for (int i = 0; i < blockCount; ++i)
            {
                const int nowSize = i < blockCount - 1 ? blockSize : lastBlockSize;
                const uint64_t d = reverse ? BitRev64(GB(p - nowSize, nowSize), nowSize) : GB(p, nowSize);
                const int x = static_cast<int>((d >> b) & 3) | static_cast<int>((d >> (b * 2)) & 0xC) |
                              static_cast<int>((d >> (b * 3)) & 0x10) | static_cast<int>((d >> (b * 4)) & 0x60) |
                              static_cast<int>((d >> (b * 5)) & 0x80);
                for (int j = 0; j < 5 && n < count; ++j, ++n)
                {
                    out[static_cast<size_t>(n)].Bits = static_cast<int>((d >> (Mt[j] + b * j)) & static_cast<uint64_t>(mask));
                    out[static_cast<size_t>(n)].NonBits = TritsTab[j][x];
                }
                p += reverse ? -blockSize : blockSize;
            }
        }
        else if (a == 5)
        {
            const int mask = (1 << b) - 1;
            const int blockCount = (count + 2) / 3;
            const int lastBlockCount = (count + 2) % 3 + 1;
            const int blockSize = 7 + 3 * b;
            const int lastBlockSize = (blockSize * lastBlockCount + 2) / 3;
            int p = offset;
            for (int i = 0; i < blockCount; ++i)
            {
                const int nowSize = i < blockCount - 1 ? blockSize : lastBlockSize;
                const uint64_t d = reverse ? BitRev64(GB(p - nowSize, nowSize), nowSize) : GB(p, nowSize);
                const int x = static_cast<int>((d >> b) & 7) | static_cast<int>((d >> (b * 2)) & 0x18) |
                              static_cast<int>((d >> (b * 3)) & 0x60);
                for (int j = 0; j < 3 && n < count; ++j, ++n)
                {
                    out[static_cast<size_t>(n)].Bits = static_cast<int>((d >> (Mq[j] + b * j)) & static_cast<uint64_t>(mask));
                    out[static_cast<size_t>(n)].NonBits = QuintsTab[j][x];
                }
                p += reverse ? -blockSize : blockSize;
            }
        }
        else if (reverse)
        {
            int p = offset - b;
            while (n < count)
            {
                // bit_reverse_u8(getbits(buf, p, b), b)
                out[static_cast<size_t>(n)].Bits = bitRev.Values[GB(p, b) & 255] >> (8 - b);
                out[static_cast<size_t>(n)].NonBits = 0;
                ++n;
                p -= b;
            }
        }
        else
        {
            int p = offset;
            while (n < count)
            {
                out[static_cast<size_t>(n)].Bits = static_cast<int>(GB(p, b));
                out[static_cast<size_t>(n)].NonBits = 0;
                ++n;
                p += b;
            }
        }
    }

    void DecodeBlockParams(BlockData &bd) const
    {
        const int b0 = static_cast<int>(_lo & 0xFF);
        const int b1 = static_cast<int>((_lo >> 8) & 0xFF);
        const auto u16 = [](int lo8, int hi8) { return lo8 | (hi8 << 8); };
        const int w16 = u16(b0, b1);
        bd.DualPlane = (b1 >> 2) & 1;
        bd.WeightRange = ((b0 >> 4) & 1) | ((b1 << 2) & 8);
        if ((b0 & 3) != 0)
        {
            bd.WeightRange |= (b0 << 1) & 6;
            switch (b0 & 0xC)
            {
            case 0:
                bd.Width = ((w16 >> 7) & 3) + 4;
                bd.Height = ((b0 >> 5) & 3) + 2;
                break;
            case 4:
                bd.Width = ((w16 >> 7) & 3) + 8;
                bd.Height = ((b0 >> 5) & 3) + 2;
                break;
            case 8:
                bd.Width = ((b0 >> 5) & 3) + 2;
                bd.Height = ((w16 >> 7) & 3) + 8;
                break;
            default:
                if ((b1 & 1) != 0)
                {
                    bd.Width = ((b0 >> 7) & 1) + 2;
                    bd.Height = ((b0 >> 5) & 3) + 2;
                }
                else
                {
                    bd.Width = ((b0 >> 5) & 3) + 2;
                    bd.Height = ((b0 >> 7) & 1) + 6;
                }
                break;
            }
        }
        else
        {
            bd.WeightRange |= (b0 >> 1) & 6;
            switch (w16 & 0x180)
            {
            case 0:
                bd.Width = 12;
                bd.Height = ((b0 >> 5) & 3) + 2;
                break;
            case 0x80:
                bd.Width = ((b0 >> 5) & 3) + 2;
                bd.Height = 12;
                break;
            case 0x100:
                bd.Width = ((b0 >> 5) & 3) + 6;
                bd.Height = ((b1 >> 1) & 3) + 6;
                bd.DualPlane = 0;
                bd.WeightRange &= 7;
                break;
            default:
                if ((b0 & 0x20) != 0)
                {
                    bd.Width = 10;
                    bd.Height = 6;
                }
                else
                {
                    bd.Width = 6;
                    bd.Height = 10;
                }
                break;
            }
        }
        bd.PartNum = ((b1 >> 3) & 3) + 1;
        bd.WeightNum = bd.Width * bd.Height;
        if (bd.DualPlane != 0)
            bd.WeightNum *= 2;
        if (bd.WeightNum > 128)
            throw BadBlock(); // more weights than the decoder keeps (and than the 128 bits can hold)
        int weightBits;
        switch (WeightA[bd.WeightRange])
        {
        case 3:
            weightBits = bd.WeightNum * WeightB[bd.WeightRange] + (bd.WeightNum * 8 + 4) / 5;
            break;
        case 5:
            weightBits = bd.WeightNum * WeightB[bd.WeightRange] + (bd.WeightNum * 7 + 2) / 3;
            break;
        default:
            weightBits = bd.WeightNum * WeightB[bd.WeightRange];
            break;
        }
        int cemBase = 0;
        int configBits;
        const int byte3 = static_cast<int>((_lo >> 24) & 0xFF);
        if (bd.PartNum == 1)
        {
            bd.Cem[0] = (u16(b1, static_cast<int>((_lo >> 16) & 0xFF)) >> 5) & 0xF;
            configBits = 17;
        }
        else
        {
            cemBase = (u16(static_cast<int>((_lo >> 16) & 0xFF), byte3) >> 7) & 3;
            if (cemBase == 0)
            {
                const int cem = (byte3 >> 1) & 0xF;
                for (int i = 0; i < bd.PartNum; ++i)
                    bd.Cem[i] = cem;
                configBits = 29;
            }
            else
            {
                for (int i = 0; i < bd.PartNum; ++i)
                    bd.Cem[i] = (((byte3 >> (i + 1)) & 1) + cemBase - 1) << 2;
                switch (bd.PartNum)
                {
                case 2:
                    bd.Cem[0] |= (byte3 >> 3) & 3;
                    bd.Cem[1] |= GBS(126 - weightBits, 2);
                    break;
                case 3:
                    bd.Cem[0] |= (byte3 >> 4) & 1;
                    bd.Cem[0] |= GBS(122 - weightBits, 2) & 2;
                    bd.Cem[1] |= GBS(124 - weightBits, 2);
                    bd.Cem[2] |= GBS(126 - weightBits, 2);
                    break;
                case 4:
                    for (int i = 0; i < 4; ++i)
                        bd.Cem[i] |= GBS(120 + i * 2 - weightBits, 2);
                    break;
                default:
                    break;
                }
                configBits = 25 + bd.PartNum * 3;
            }
        }
        if (bd.DualPlane != 0)
        {
            configBits += 2;
            const int sw = cemBase != 0 ? 130 - weightBits - bd.PartNum * 3 : 126 - weightBits;
            bd.PlaneSel = GBS(sw, 2);
        }
        bd.EndpointValueNum = 0;
        for (int i = 0; i < bd.PartNum; ++i)
            bd.EndpointValueNum += ((bd.Cem[i] >> 1) & 6) + 2;
        bd.CemRange = -1;
        for (int i = 0; i < 19; ++i)
        {
            int endpointBits;
            switch (CemA[i])
            {
            case 3:
                endpointBits = bd.EndpointValueNum * CemB[i] + (bd.EndpointValueNum * 8 + 4) / 5;
                break;
            case 5:
                endpointBits = bd.EndpointValueNum * CemB[i] + (bd.EndpointValueNum * 7 + 2) / 3;
                break;
            default:
                endpointBits = bd.EndpointValueNum * CemB[i];
                break;
            }
            if (endpointBits <= 128 - configBits - weightBits)
            {
                bd.CemRange = i;
                break;
            }
        }
        if (bd.CemRange < 0)
            throw BadBlock();
    }

    static void DecodeEndpointsHdr7(int *endpoints, const int *v)
    {
        const int modeVal = ((v[2] >> 4) & 8) | ((v[1] >> 5) & 4) | (v[0] >> 6);
        int major, mode;
        if ((modeVal & 0xC) != 0xC)
        {
            major = modeVal >> 2;
            mode = modeVal & 3;
        }
        else if (modeVal != 0xF)
        {
            major = modeVal & 3;
            mode = 4;
        }
        else
        {
            major = 0;
            mode = 5;
        }
        int c[4];
        c[0] = v[0] & 0x3F;
        c[1] = v[1] & 0x1F;
        c[2] = v[2] & 0x1F;
        c[3] = v[3] & 0x1F;
        const auto shiftAll = [&](int n) {
            for (int i = 0; i < 4; ++i)
                c[i] <<= n;
        };
        switch (mode)
        {
        case 0:
            c[3] |= v[3] & 0x60;
            c[0] |= (v[3] >> 1) & 0x40;
            c[0] |= (v[2] << 1) & 0x80;
            c[0] |= (v[1] << 3) & 0x300;
            c[0] |= (v[2] << 5) & 0x400;
            shiftAll(1);
            break;
        case 1:
            c[1] |= v[1] & 0x20;
            c[2] |= v[2] & 0x20;
            c[0] |= (v[3] >> 1) & 0x40;
            c[0] |= (v[2] << 1) & 0x80;
            c[0] |= (v[1] << 2) & 0x100;
            c[0] |= (v[3] << 4) & 0x600;
            shiftAll(1);
            break;
        case 2:
            c[3] |= v[3] & 0xE0;
            c[0] |= (v[2] << 1) & 0xC0;
            c[0] |= (v[1] << 3) & 0x300;
            shiftAll(2);
            break;
        case 3:
            c[1] |= v[1] & 0x20;
            c[2] |= v[2] & 0x20;
            c[3] |= v[3] & 0x60;
            c[0] |= (v[3] >> 1) & 0x40;
            c[0] |= (v[2] << 1) & 0x80;
            c[0] |= (v[1] << 2) & 0x100;
            shiftAll(3);
            break;
        case 4:
            c[1] |= v[1] & 0x60;
            c[2] |= v[2] & 0x60;
            c[3] |= v[3] & 0x20;
            c[0] |= (v[3] >> 1) & 0x40;
            c[0] |= (v[3] << 1) & 0x80;
            shiftAll(4);
            break;
        case 5:
            c[1] |= v[1] & 0x60;
            c[2] |= v[2] & 0x60;
            c[3] |= v[3] & 0x60;
            c[0] |= (v[3] >> 1) & 0x40;
            shiftAll(5);
            break;
        default:
            break;
        }
        if (mode != 5)
        {
            c[1] = c[0] - c[1];
            c[2] = c[0] - c[2];
        }
        if (major == 1)
            SetEpHdrClamp(endpoints, c[1] - c[3], c[0] - c[3], c[2] - c[3], 0x780, c[1], c[0], c[2], 0x780);
        else if (major == 2)
            SetEpHdrClamp(endpoints, c[2] - c[3], c[1] - c[3], c[0] - c[3], 0x780, c[2], c[1], c[0], 0x780);
        else
            SetEpHdrClamp(endpoints, c[0] - c[3], c[1] - c[3], c[2] - c[3], 0x780, c[0], c[1], c[2], 0x780);
    }

    static void DecodeEndpointsHdr11(int *endpoints, const int *v, int alpha1, int alpha2)
    {
        const int major = (v[4] >> 7) | ((v[5] >> 6) & 2);
        if (major == 3)
        {
            SetEp(endpoints, v[0] << 4, v[2] << 4, (v[4] << 5) & 0xFE0, alpha1, v[1] << 4, v[3] << 4,
                  (v[5] << 5) & 0xFE0, alpha2);
            return;
        }
        const int mode = (v[1] >> 7) | ((v[2] >> 6) & 2) | ((v[3] >> 5) & 4);
        int va = v[0] | ((v[1] << 2) & 0x100);
        int vb0 = v[2] & 0x3F;
        int vb1 = v[3] & 0x3F;
        int vc = v[1] & 0x3F;
        int vd0, vd1;
        switch (mode)
        {
        case 0:
        case 2:
            vd0 = v[4] & 0x7F;
            if ((vd0 & 0x40) != 0)
                vd0 -= 128;
            vd1 = v[5] & 0x7F;
            if ((vd1 & 0x40) != 0)
                vd1 -= 128;
            break;
        case 1:
        case 3:
        case 5:
        case 7:
            vd0 = v[4] & 0x3F;
            if ((vd0 & 0x20) != 0)
                vd0 -= 64;
            vd1 = v[5] & 0x3F;
            if ((vd1 & 0x20) != 0)
                vd1 -= 64;
            break;
        default:
            vd0 = v[4] & 0x1F;
            if ((vd0 & 0x10) != 0)
                vd0 -= 32;
            vd1 = v[5] & 0x1F;
            if ((vd1 & 0x10) != 0)
                vd1 -= 32;
            break;
        }
        switch (mode)
        {
        case 0:
            vb0 |= v[2] & 0x40;
            vb1 |= v[3] & 0x40;
            break;
        case 1:
            vb0 |= v[2] & 0x40;
            vb1 |= v[3] & 0x40;
            vb0 |= (v[4] << 1) & 0x80;
            vb1 |= (v[5] << 1) & 0x80;
            break;
        case 2:
            va |= (v[2] << 3) & 0x200;
            vc |= v[3] & 0x40;
            break;
        case 3:
            va |= (v[4] << 3) & 0x200;
            vc |= v[5] & 0x40;
            vb0 |= v[2] & 0x40;
            vb1 |= v[3] & 0x40;
            break;
        case 4:
            va |= (v[4] << 4) & 0x200;
            va |= (v[5] << 5) & 0x400;
            vb0 |= v[2] & 0x40;
            vb1 |= v[3] & 0x40;
            vb0 |= (v[4] << 1) & 0x80;
            vb1 |= (v[5] << 1) & 0x80;
            break;
        case 5:
            va |= (v[2] << 3) & 0x200;
            va |= (v[3] << 4) & 0x400;
            vc |= v[5] & 0x40;
            vc |= (v[4] << 1) & 0x80;
            break;
        case 6:
            va |= (v[4] << 4) & 0x200;
            va |= (v[5] << 5) & 0x400;
            va |= (v[4] << 5) & 0x800;
            vc |= v[5] & 0x40;
            vb0 |= v[2] & 0x40;
            vb1 |= v[3] & 0x40;
            break;
        case 7:
            va |= (v[2] << 3) & 0x200;
            va |= (v[3] << 4) & 0x400;
            va |= (v[4] << 5) & 0x800;
            vc |= v[5] & 0x40;
            break;
        default:
            break;
        }
        const int shamt = (mode >> 1) ^ 3;
        va <<= shamt;
        vb0 <<= shamt;
        vb1 <<= shamt;
        vc <<= shamt;
        const int mult = 1 << shamt;
        vd0 *= mult;
        vd1 *= mult;
        if (major == 1)
            SetEpHdrClamp(endpoints, va - vb0 - vc - vd0, va - vc, va - vb1 - vc - vd1, alpha1, va - vb0, va,
                          va - vb1, alpha2);
        else if (major == 2)
            SetEpHdrClamp(endpoints, va - vb1 - vc - vd1, va - vb0 - vc - vd0, va - vc, alpha1, va - vb1, va - vb0,
                          va, alpha2);
        else
            SetEpHdrClamp(endpoints, va - vc, va - vb0 - vc - vd0, va - vb1 - vc - vd1, alpha1, va, va - vb0,
                          va - vb1, alpha2);
    }

    void DecodeEndpoints(BlockData &bd) const
    {
        const int cr = bd.CemRange;
        SeqArr seq;
        const int start = bd.PartNum == 1 ? 17 : 29;
        DecodeIntSeq(start, CemA[cr], CemB[cr], bd.EndpointValueNum, false, seq, 32);
        int ev[64] = {};
        const auto bitsAt = [&](int i) { return seq[static_cast<size_t>(i)].Bits; };
        const auto nonAt = [&](int i) { return seq[static_cast<size_t>(i)].NonBits; };
        switch (CemA[cr])
        {
        case 3: {
            const int c = EndTrits[CemB[cr]];
            for (int i = 0; i < bd.EndpointValueNum; ++i)
            {
                const int a = (bitsAt(i) & 1) * 0x1FF;
                const int x = bitsAt(i) >> 1;
                int bv = 0;
                switch (CemB[cr])
                {
                case 1:
                    bv = 0;
                    break;
                case 2:
                    bv = 0x116 * x; // 0b100010110
                    break;
                case 3:
                    bv = (x << 7) | (x << 2) | x;
                    break;
                case 4:
                    bv = (x << 6) | x;
                    break;
                case 5:
                    bv = (x << 5) | (x >> 2);
                    break;
                case 6:
                    bv = (x << 4) | (x >> 4);
                    break;
                default:
                    break;
                }
                ev[i] = (a & 0x80) | (((nonAt(i) * c + bv) ^ a) >> 2);
            }
            break;
        }
        case 5: {
            const int c = EndQuints[CemB[cr]];
            for (int i = 0; i < bd.EndpointValueNum; ++i)
            {
                const int a = (bitsAt(i) & 1) * 0x1FF;
                const int x = bitsAt(i) >> 1;
                int bv = 0;
                switch (CemB[cr])
                {
                case 1:
                    bv = 0;
                    break;
                case 2:
                    bv = 0x10C * x; // 0b100001100
                    break;
                case 3:
                    bv = (x << 7) | (x << 1) | (x >> 1);
                    break;
                case 4:
                    bv = (x << 6) | (x >> 1);
                    break;
                case 5:
                    bv = (x << 5) | (x >> 3);
                    break;
                default:
                    break;
                }
                ev[i] = (a & 0x80) | (((nonAt(i) * c + bv) ^ a) >> 2);
            }
            break;
        }
        default:
            for (int i = 0; i < bd.EndpointValueNum; ++i)
            {
                const int b = bitsAt(i);
                switch (CemB[cr])
                {
                case 1:
                    ev[i] = b * 0xFF;
                    break;
                case 2:
                    ev[i] = b * 0x55;
                    break;
                case 3:
                    ev[i] = (b << 5) | (b << 2) | (b >> 1);
                    break;
                case 4:
                    ev[i] = (b << 4) | b;
                    break;
                case 5:
                    ev[i] = (b << 3) | (b >> 2);
                    break;
                case 6:
                    ev[i] = (b << 2) | (b >> 4);
                    break;
                case 7:
                    ev[i] = (b << 1) | (b >> 6);
                    break;
                case 8:
                    ev[i] = b;
                    break;
                default:
                    break;
                }
            }
            break;
        }
        int off = 0;
        for (int cem = 0; cem < bd.PartNum; ++cem)
        {
            int v[16] = {};
            for (int i = 0; i < 16; ++i)
                if (off + i < 64)
                    v[i] = ev[off + i];
            int *ep = bd.Endpoints[cem];
            switch (bd.Cem[cem])
            {
            case 0:
                SetEp(ep, v[0], v[0], v[0], 255, v[1], v[1], v[1], 255);
                break;
            case 1: {
                const int l0 = (v[0] >> 2) | (v[1] & 0xC0);
                const int l1 = Clamp8(l0 + (v[1] & 0x3F));
                SetEp(ep, l0, l0, l0, 255, l1, l1, l1, 255);
                break;
            }
            case 2: {
                int y0, y1;
                if (v[0] <= v[1])
                {
                    y0 = v[0] << 4;
                    y1 = v[1] << 4;
                }
                else
                {
                    y0 = (v[1] << 4) + 8;
                    y1 = (v[0] << 4) - 8;
                }
                SetEp(ep, y0, y0, y0, 0x780, y1, y1, y1, 0x780);
                break;
            }
            case 3: {
                int y0, d;
                if ((v[0] & 0x80) != 0)
                {
                    y0 = ((v[1] & 0xE0) << 4) | ((v[0] & 0x7F) << 2);
                    d = (v[1] & 0x1F) << 2;
                }
                else
                {
                    y0 = ((v[1] & 0xF0) << 4) | ((v[0] & 0x7F) << 1);
                    d = (v[1] & 0x0F) << 1;
                }
                const int y1 = ClampHdr(y0 + d);
                SetEp(ep, y0, y0, y0, 0x780, y1, y1, y1, 0x780);
                break;
            }
            case 4:
                SetEp(ep, v[0], v[0], v[0], v[2], v[1], v[1], v[1], v[3]);
                break;
            case 5:
                BitTransferSigned(v[1], v[0]);
                BitTransferSigned(v[3], v[2]);
                v[1] += v[0];
                SetEpClamp(ep, v[0], v[0], v[0], v[2], v[1], v[1], v[1], v[2] + v[3]);
                break;
            case 6:
                SetEp(ep, Sar(v[0] * v[3], 8), Sar(v[1] * v[3], 8), Sar(v[2] * v[3], 8), 255, v[0], v[1], v[2], 255);
                break;
            case 7:
                DecodeEndpointsHdr7(ep, v);
                break;
            case 8:
                if (v[0] + v[2] + v[4] <= v[1] + v[3] + v[5])
                    SetEp(ep, v[0], v[2], v[4], 255, v[1], v[3], v[5], 255);
                else
                    SetEpBlue(ep, v[1], v[3], v[5], 255, v[0], v[2], v[4], 255);
                break;
            case 9:
                BitTransferSigned(v[1], v[0]);
                BitTransferSigned(v[3], v[2]);
                BitTransferSigned(v[5], v[4]);
                if (v[1] + v[3] + v[5] >= 0)
                    SetEpClamp(ep, v[0], v[2], v[4], 255, v[0] + v[1], v[2] + v[3], v[4] + v[5], 255);
                else
                    SetEpBlueClamp(ep, v[0] + v[1], v[2] + v[3], v[4] + v[5], 255, v[0], v[2], v[4], 255);
                break;
            case 10:
                SetEp(ep, Sar(v[0] * v[3], 8), Sar(v[1] * v[3], 8), Sar(v[2] * v[3], 8), v[4], v[0], v[1], v[2], v[5]);
                break;
            case 11:
                DecodeEndpointsHdr11(ep, v, 0x780, 0x780);
                break;
            case 12:
                if (v[0] + v[2] + v[4] <= v[1] + v[3] + v[5])
                    SetEp(ep, v[0], v[2], v[4], v[6], v[1], v[3], v[5], v[7]);
                else
                    SetEpBlue(ep, v[1], v[3], v[5], v[7], v[0], v[2], v[4], v[6]);
                break;
            case 13:
                BitTransferSigned(v[1], v[0]);
                BitTransferSigned(v[3], v[2]);
                BitTransferSigned(v[5], v[4]);
                BitTransferSigned(v[7], v[6]);
                if (v[1] + v[3] + v[5] >= 0)
                    SetEpClamp(ep, v[0], v[2], v[4], v[6], v[0] + v[1], v[2] + v[3], v[4] + v[5], v[6] + v[7]);
                else
                    SetEpBlueClamp(ep, v[0] + v[1], v[2] + v[3], v[4] + v[5], v[6] + v[7], v[0], v[2], v[4], v[6]);
                break;
            case 14:
                DecodeEndpointsHdr11(ep, v, v[6], v[7]);
                break;
            case 15: {
                const int mode = ((v[6] >> 7) & 1) | ((v[7] >> 6) & 2);
                v[6] &= 0x7F;
                v[7] &= 0x7F;
                if (mode == 3)
                    DecodeEndpointsHdr11(ep, v, v[6] << 5, v[7] << 5);
                else
                {
                    v[6] |= (v[7] << (mode + 1)) & 0x780;
                    v[7] = ((v[7] & (0x3F >> mode)) ^ (0x20 >> mode)) - (0x20 >> mode);
                    v[6] <<= 4 - mode;
                    v[7] <<= 4 - mode;
                    DecodeEndpointsHdr11(ep, v, v[6], ClampHdr(v[6] + v[7]));
                }
                break;
            }
            default:
                break;
            }
            off += (bd.Cem[cem] / 4 + 1) * 2;
        }
    }

    void DecodeWeights(BlockData &bd) const
    {
        SeqArr seq;
        std::array<int, 512> wv{};
        const int wa = WeightA[bd.WeightRange];
        const int wb = WeightB[bd.WeightRange];
        DecodeIntSeq(128, wa, wb, bd.WeightNum, true, seq, 128);
        const auto bitsAt = [&](int i) { return seq[static_cast<size_t>(i)].Bits; };
        const auto nonAt = [&](int i) { return seq[static_cast<size_t>(i)].NonBits; };
        if (wa == 0)
        {
            for (int i = 0; i < bd.WeightNum; ++i)
            {
                const int b = bitsAt(i);
                switch (wb)
                {
                case 1:
                    wv[static_cast<size_t>(i)] = b != 0 ? 63 : 0;
                    break;
                case 2:
                    wv[static_cast<size_t>(i)] = (b << 4) | (b << 2) | b;
                    break;
                case 3:
                    wv[static_cast<size_t>(i)] = (b << 3) | b;
                    break;
                case 4:
                    wv[static_cast<size_t>(i)] = (b << 2) | (b >> 2);
                    break;
                case 5:
                    wv[static_cast<size_t>(i)] = (b << 1) | (b >> 4);
                    break;
                default:
                    break;
                }
            }
            for (int i = 0; i < bd.WeightNum; ++i)
                if (wv[static_cast<size_t>(i)] > 32)
                    ++wv[static_cast<size_t>(i)];
        }
        else if (wb == 0)
        {
            const int s = wa == 3 ? 32 : 16;
            for (int i = 0; i < bd.WeightNum; ++i)
                wv[static_cast<size_t>(i)] = nonAt(i) * s;
        }
        else
        {
            if (wa == 3)
            {
                for (int i = 0; i < bd.WeightNum; ++i)
                {
                    int &w = wv[static_cast<size_t>(i)];
                    switch (wb)
                    {
                    case 1:
                        w = nonAt(i) * 50;
                        break;
                    case 2:
                        w = nonAt(i) * 23;
                        if ((bitsAt(i) & 2) != 0)
                            w += 0x45; // 0b1000101
                        break;
                    case 3:
                        w = nonAt(i) * 11 + (((bitsAt(i) << 4) | (bitsAt(i) >> 1)) & 0x63);
                        break;
                    default:
                        break;
                    }
                }
            }
            else if (wa == 5)
            {
                for (int i = 0; i < bd.WeightNum; ++i)
                {
                    int &w = wv[static_cast<size_t>(i)];
                    switch (wb)
                    {
                    case 1:
                        w = nonAt(i) * 28;
                        break;
                    case 2:
                        w = nonAt(i) * 13;
                        if ((bitsAt(i) & 2) != 0)
                            w += 0x42; // 0b1000010
                        break;
                    default:
                        break;
                    }
                }
            }
            for (int i = 0; i < bd.WeightNum; ++i)
            {
                int &w = wv[static_cast<size_t>(i)];
                const int a = (bitsAt(i) & 1) * 0x7F;
                w = (a & 0x20) | ((w ^ a) >> 2);
                if (w > 32)
                    ++w;
            }
        }
        const int ds = (1024 + bd.BW / 2) / (bd.BW - 1);
        const int dt = (1024 + bd.BH / 2) / (bd.BH - 1);
        const int pn = bd.DualPlane != 0 ? 2 : 1;
        int i = 0;
        for (int t = 0; t < bd.BH; ++t)
            for (int s = 0; s < bd.BW; ++s)
            {
                const int gs = (ds * s * (bd.Width - 1) + 32) >> 6;
                const int gt = (dt * t * (bd.Height - 1) + 32) >> 6;
                const int fs = gs & 0xF;
                const int ft = gt & 0xF;
                const int v = (gs >> 4) + (gt >> 4) * bd.Width;
                const int w11 = (fs * ft + 8) >> 4;
                const int w10 = ft - w11;
                const int w01 = fs - w11;
                const int w00 = 16 - fs - ft + w11;
                for (int p = 0; p < pn; ++p)
                {
                    const int p00 = wv[static_cast<size_t>(v * pn + p)];
                    const int p01 = wv[static_cast<size_t>((v + 1) * pn + p)];
                    const int p10 = wv[static_cast<size_t>((v + bd.Width) * pn + p)];
                    const int p11 = wv[static_cast<size_t>((v + bd.Width + 1) * pn + p)];
                    bd.Weights[i][p] = (p00 * w00 + p01 * w01 + p10 * w10 + p11 * w11 + 8) >> 4;
                }
                ++i;
            }
    }

    void SelectPartition(BlockData &bd) const
    {
        const bool small = bd.BW * bd.BH < 31;
        const int seed = static_cast<int>((_lo >> 13) & 0x3FF) | ((bd.PartNum - 1) << 10);
        uint32_t rnum = static_cast<uint32_t>(seed);
        rnum ^= rnum >> 15;
        rnum -= rnum << 17;
        rnum += rnum << 7;
        rnum += rnum << 4;
        rnum ^= rnum >> 5;
        rnum += rnum << 16;
        rnum ^= rnum >> 7;
        rnum ^= rnum >> 3;
        rnum ^= rnum << 6;
        rnum ^= rnum >> 17;
        int seeds[8];
        for (int i = 0; i < 8; ++i)
        {
            seeds[i] = static_cast<int>((rnum >> (i * 4)) & 0xF);
            seeds[i] = seeds[i] * seeds[i];
        }
        int sh[2];
        sh[0] = (seed & 2) != 0 ? 4 : 5;
        sh[1] = bd.PartNum == 3 ? 6 : 5;
        if ((seed & 1) != 0)
            for (int i = 0; i < 8; ++i)
                seeds[i] >>= sh[i % 2];
        else
            for (int i = 0; i < 8; ++i)
                seeds[i] >>= sh[1 - i % 2];
        int idx = 0;
        for (int t = 0; t < bd.BH; ++t)
            for (int s = 0; s < bd.BW; ++s)
            {
                const int x = small ? s << 1 : s;
                const int y = small ? t << 1 : t;
                const int a = static_cast<int>((static_cast<uint32_t>(seeds[0] * x + seeds[1] * y) + (rnum >> 14)) & 0x3F);
                const int b = static_cast<int>((static_cast<uint32_t>(seeds[2] * x + seeds[3] * y) + (rnum >> 10)) & 0x3F);
                const int c = bd.PartNum < 3
                                  ? 0
                                  : static_cast<int>((static_cast<uint32_t>(seeds[4] * x + seeds[5] * y) + (rnum >> 6)) & 0x3F);
                const int d = bd.PartNum < 4
                                  ? 0
                                  : static_cast<int>((static_cast<uint32_t>(seeds[6] * x + seeds[7] * y) + (rnum >> 2)) & 0x3F);
                if (a >= b && a >= c && a >= d)
                    bd.Partition[idx] = 0;
                else if (b >= c && b >= d)
                    bd.Partition[idx] = 1;
                else if (c >= d)
                    bd.Partition[idx] = 2;
                else
                    bd.Partition[idx] = 3;
                ++idx;
            }
    }

    static void ApplyColor(const BlockData &bd, uint32_t *px)
    {
        int ps[4] = {};
        if (bd.DualPlane != 0)
            ps[bd.PlaneSel] = 1;
        const auto pick = [](bool hdr, int v0, int v1, int w) {
            return hdr ? SelectColorHdr(v0, v1, w) : SelectColor(v0, v1, w);
        };
        for (int i = 0; i < bd.BW * bd.BH; ++i)
        {
            const int p = bd.PartNum > 1 ? bd.Partition[i] : 0;
            const int *e = bd.Endpoints[p];
            const bool fc = FuncC[bd.Cem[p]];
            const bool fa = FuncA[bd.Cem[p]];
            int r, g, b, a;
            if (bd.DualPlane != 0)
            {
                r = pick(fc, e[0], e[4], bd.Weights[i][ps[0]]);
                g = pick(fc, e[1], e[5], bd.Weights[i][ps[1]]);
                b = pick(fc, e[2], e[6], bd.Weights[i][ps[2]]);
                a = pick(fa, e[3], e[7], bd.Weights[i][ps[3]]);
            }
            else
            {
                r = pick(fc, e[0], e[4], bd.Weights[i][0]);
                g = pick(fc, e[1], e[5], bd.Weights[i][0]);
                b = pick(fc, e[2], e[6], bd.Weights[i][0]);
                a = pick(fa, e[3], e[7], bd.Weights[i][0]);
            }
            px[i] = Pack(r, g, b, a);
        }
    }

    uint64_t _lo = 0, _hi = 0; // the block being decoded
};
} // namespace

auto AstcBlock(const uint8_t *src, int bw, int bh, uint32_t *px) -> bool
{
    uint8_t buf[16];
    std::memcpy(buf, src, 16);
    if (buf[0] == 0xFC && (buf[1] & 1) == 1)
    {
        // void extent: one color
        const uint32_t c = (buf[1] & 2) != 0
                               ? Pack(F16ToU8(&buf[8]), F16ToU8(&buf[10]), F16ToU8(&buf[12]), F16ToU8(&buf[14]))
                               : Pack(buf[9], buf[11], buf[13], buf[15]);
        for (int i = 0; i < bw * bh; ++i)
            px[i] = c;
        return true;
    }
    bool result = true;
    if (((buf[0] & 0xC3) == 0xC0 && (buf[1] & 1) == 1) || (buf[0] & 0xF) == 0)
        result = false;
    else
    {
        auto bd = std::make_unique<BlockData>();
        bd->BW = bw;
        bd->BH = bh;
        try
        {
            Decoder(buf).DecodeAll(*bd, px);
        }
        catch (const BadBlock &)
        {
            result = false;
        }
    }
    if (!result)
    {
        const uint32_t c = Pack(255, 0, 255, 255);
        for (int i = 0; i < bw * bh; ++i)
            px[i] = c;
    }
    return result;
}
} // namespace Dud
