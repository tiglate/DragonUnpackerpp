// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "TexDecode.h"

#include <cstring>

#include "Astc.h"

namespace Dud
{
namespace
{
constexpr uint8_t Mod1[8][2] = {{2, 8}, {5, 17}, {9, 29}, {13, 42}, {18, 60}, {24, 80}, {33, 106}, {47, 183}};
constexpr uint8_t ModA1[2][8][2] = {
    {{0, 8}, {0, 17}, {0, 29}, {0, 42}, {0, 60}, {0, 80}, {0, 106}, {0, 183}},
    {{2, 8}, {5, 17}, {9, 29}, {13, 42}, {18, 60}, {24, 80}, {33, 106}, {47, 183}}};
constexpr uint8_t SubBlock[2][16] = {{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1},
                                     {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1}};
constexpr uint8_t Dist[8] = {3, 6, 11, 16, 23, 32, 41, 64};
constexpr int8_t AlphaMod[16][8] = {
    {-3, -6, -9, -15, 2, 5, 8, 14}, {-3, -7, -10, -13, 2, 6, 9, 12}, {-2, -5, -8, -13, 1, 4, 7, 12},
    {-2, -4, -6, -13, 1, 3, 5, 12}, {-3, -6, -8, -12, 2, 5, 7, 11},  {-3, -7, -9, -11, 2, 6, 8, 10},
    {-4, -7, -8, -11, 3, 6, 7, 10}, {-3, -5, -8, -11, 2, 4, 7, 10},  {-2, -6, -8, -10, 1, 5, 7, 9},
    {-2, -5, -8, -10, 1, 4, 7, 9},  {-2, -4, -8, -10, 1, 3, 7, 9},   {-2, -5, -7, -10, 1, 4, 6, 9},
    {-3, -4, -7, -10, 2, 3, 6, 9},  {-1, -2, -3, -10, 0, 1, 2, 9},   {-4, -6, -8, -9, 3, 5, 7, 8},
    {-3, -5, -7, -9, 2, 4, 6, 8}};
// pixel i of a block in the order of the bits (columns first) -> index in the block (rows first)
constexpr uint8_t WriteOrder[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
constexpr uint8_t WriteOrderRev[16] = {15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0};

enum class Kind
{
    Etc1,
    Etc2,
    Etc2a1,
    Etc2a8,
    EacR,
    EacRS,
    EacRG,
    EacRGS,
    Astc,
    Bc1,
    Bc3
};

auto KindOf(int unityFmt, Kind &k) -> bool
{
    switch (unityFmt)
    {
    case 34:
    case 60:
        k = Kind::Etc1;
        return true;
    case 45:
        k = Kind::Etc2;
        return true;
    case 46:
        k = Kind::Etc2a1;
        return true;
    case 47:
    case 61:
        k = Kind::Etc2a8;
        return true;
    case 41:
        k = Kind::EacR;
        return true;
    case 42:
        k = Kind::EacRS;
        return true;
    case 43:
        k = Kind::EacRG;
        return true;
    case 44:
        k = Kind::EacRGS;
        return true;
    case 10:
        k = Kind::Bc1;
        return true;
    case 12:
        k = Kind::Bc3;
        return true;
    default:
        if ((unityFmt >= 48 && unityFmt <= 59) || (unityFmt >= 66 && unityFmt <= 71))
        {
            k = Kind::Astc;
            return true;
        }
        return false;
    }
}

auto Clamp(int n) -> int
{
    return n < 0 ? 0 : n > 255 ? 255 : n;
}

auto Pack(int r, int g, int b, int a) -> uint32_t
{
    return static_cast<uint32_t>(b & 255) | (static_cast<uint32_t>(g & 255) << 8) |
           (static_cast<uint32_t>(r & 255) << 16) | (static_cast<uint32_t>(a & 255) << 24);
}

using C3 = std::array<int, 3>;

auto ApplyColor(const C3 &c, int m) -> uint32_t
{
    return Pack(Clamp(c[0] + m), Clamp(c[1] + m), Clamp(c[2] + m), 255);
}

auto RawColor(const C3 &c) -> uint32_t
{
    return Pack(c[0], c[1], c[2], 255);
}

void Bc1Block(const uint8_t *d, BlockPixels &out)
{
    const int q0 = d[0] | (d[1] << 8);
    const int q1 = d[2] | (d[3] << 8);
    const int r0 = ((q0 >> 8) & 0xF8) | (q0 >> 13);
    const int g0 = ((q0 >> 3) & 0xFC) | ((q0 >> 9) & 3);
    const int b0 = ((q0 << 3) & 255) | ((q0 >> 2) & 7);
    const int r1 = ((q1 >> 8) & 0xF8) | (q1 >> 13);
    const int g1 = ((q1 >> 3) & 0xFC) | ((q1 >> 9) & 3);
    const int b1 = ((q1 << 3) & 255) | ((q1 >> 2) & 7);
    uint32_t c[4];
    c[0] = Pack(r0, g0, b0, 255);
    c[1] = Pack(r1, g1, b1, 255);
    if (q0 > q1)
    {
        c[2] = Pack((r0 * 2 + r1) / 3, (g0 * 2 + g1) / 3, (b0 * 2 + b1) / 3, 255);
        c[3] = Pack((r0 + r1 * 2) / 3, (g0 + g1 * 2) / 3, (b0 + b1 * 2) / 3, 255);
    }
    else
    {
        c[2] = Pack((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
        c[3] = Pack(0, 0, 0, 255);
    }
    uint32_t bits = static_cast<uint32_t>(d[4]) | (static_cast<uint32_t>(d[5]) << 8) |
                    (static_cast<uint32_t>(d[6]) << 16) | (static_cast<uint32_t>(d[7]) << 24);
    for (int i = 0; i < 16; ++i)
    {
        out[static_cast<size_t>(i)] = c[bits & 3];
        bits >>= 2;
    }
}

// the alpha of a DXT5 block (8 bytes)
void Bc3Alpha(const uint8_t *d, BlockPixels &out)
{
    int a[8];
    a[0] = d[0];
    a[1] = d[1];
    if (a[0] > a[1])
        for (int i = 2; i <= 7; ++i)
            a[i] = (a[0] * (8 - i) + a[1] * (i - 1)) / 7;
    else
    {
        for (int i = 2; i <= 5; ++i)
            a[i] = (a[0] * (6 - i) + a[1] * (i - 1)) / 5;
        a[6] = 0;
        a[7] = 255;
    }
    uint64_t bits = 0;
    for (int i = 7; i >= 2; --i)
        bits = (bits << 8) | d[i];
    for (int i = 0; i < 16; ++i)
    {
        auto &px = out[static_cast<size_t>(i)];
        px = (px & 0x00FFFFFF) | (static_cast<uint32_t>(a[bits & 7]) << 24);
        bits >>= 3;
    }
}

void Etc1Block(const uint8_t *d, BlockPixels &out)
{
    int code[2];
    C3 c[2];
    code[0] = d[3] >> 5;
    code[1] = (d[3] >> 2) & 7;
    const int tbl = d[3] & 1;
    if ((d[3] & 2) != 0)
    {
        for (int i = 0; i < 3; ++i)
        {
            c[0][static_cast<size_t>(i)] = d[i] & 0xF8;
            // the value of the second base wraps on 8 bits when the delta overflows (invalid blocks)
            c[1][static_cast<size_t>(i)] =
                (c[0][static_cast<size_t>(i)] + ((d[i] << 3) & 0x18) - ((d[i] << 3) & 0x20)) & 255;
            c[0][static_cast<size_t>(i)] |= c[0][static_cast<size_t>(i)] >> 5;
            c[1][static_cast<size_t>(i)] |= c[1][static_cast<size_t>(i)] >> 5;
        }
    }
    else
        for (int i = 0; i < 3; ++i)
        {
            c[0][static_cast<size_t>(i)] = (d[i] & 0xF0) | (d[i] >> 4);
            c[1][static_cast<size_t>(i)] = (d[i] & 0x0F) | ((d[i] << 4) & 255);
        }
    int j = (d[6] << 8) | d[7];
    int k = (d[4] << 8) | d[5];
    for (int i = 0; i < 16; ++i)
    {
        const int s = SubBlock[tbl][i];
        int m = Mod1[code[s]][j & 1];
        if ((k & 1) != 0)
            m = -m;
        out[WriteOrder[i]] = ApplyColor(c[s], m);
        j >>= 1;
        k >>= 1;
    }
}

void Etc2Block(const uint8_t *d, BlockPixels &out, bool a1)
{
    int j = (d[6] << 8) | d[7];
    int k = (d[4] << 8) | d[5];
    C3 c[3] = {};
    const int obaq = (d[3] >> 1) & 1;
    if (a1 || (d[3] & 2) != 0)
    {
        const int r = d[0] & 0xF8;
        const int dr = ((d[0] << 3) & 0x18) - ((d[0] << 3) & 0x20);
        const int g = d[1] & 0xF8;
        const int dg = ((d[1] << 3) & 0x18) - ((d[1] << 3) & 0x20);
        const int b = d[2] & 0xF8;
        const int db = ((d[2] << 3) & 0x18) - ((d[2] << 3) & 0x20);
        if (r + dr < 0 || r + dr > 255)
        {
            // T mode
            c[0][0] = ((d[0] << 3) & 0xC0) | ((d[0] << 4) & 0x30) | ((d[0] >> 1) & 0xC) | (d[0] & 3);
            c[0][1] = (d[1] & 0xF0) | (d[1] >> 4);
            c[0][2] = (d[1] & 0x0F) | ((d[1] << 4) & 255);
            c[1][0] = (d[2] & 0xF0) | (d[2] >> 4);
            c[1][1] = (d[2] & 0x0F) | ((d[2] << 4) & 255);
            c[1][2] = (d[3] & 0xF0) | (d[3] >> 4);
            const int dd = Dist[((d[3] >> 1) & 6) | (d[3] & 1)];
            uint32_t colorSet[4];
            colorSet[0] = RawColor(c[0]);
            colorSet[1] = ApplyColor(c[1], dd);
            colorSet[2] = RawColor(c[1]);
            colorSet[3] = ApplyColor(c[1], -dd);
            k <<= 1;
            for (int i = 0; i < 16; ++i)
            {
                const int idx = (k & 2) | (j & 1);
                out[WriteOrder[i]] = colorSet[idx];
                if (a1 && obaq == 0 && idx == 2)
                    out[WriteOrder[i]] &= 0x00FFFFFF;
                j >>= 1;
                k >>= 1;
            }
        }
        else if (g + dg < 0 || g + dg > 255)
        {
            // H mode
            c[0][0] = ((d[0] << 1) & 0xF0) | ((d[0] >> 3) & 0xF);
            c[0][1] = ((d[0] << 5) & 0xE0) | (d[1] & 0x10);
            c[0][1] |= c[0][1] >> 4;
            c[0][2] = (d[1] & 8) | ((d[1] << 1) & 6) | (d[2] >> 7);
            c[0][2] |= (c[0][2] << 4) & 255;
            c[1][0] = ((d[2] << 1) & 0xF0) | ((d[2] >> 3) & 0xF);
            c[1][1] = ((d[2] << 5) & 0xE0) | ((d[3] >> 3) & 0x10);
            c[1][1] |= c[1][1] >> 4;
            c[1][2] = ((d[3] << 1) & 0xF0) | ((d[3] >> 3) & 0xF);
            int dd = (d[3] & 4) | ((d[3] << 1) & 2);
            if (c[0][0] > c[1][0] ||
                (c[0][0] == c[1][0] && (c[0][1] > c[1][1] || (c[0][1] == c[1][1] && c[0][2] >= c[1][2]))))
                ++dd;
            dd = Dist[dd];
            uint32_t colorSet[4];
            colorSet[0] = ApplyColor(c[0], dd);
            colorSet[1] = ApplyColor(c[0], -dd);
            colorSet[2] = ApplyColor(c[1], dd);
            colorSet[3] = ApplyColor(c[1], -dd);
            k <<= 1;
            for (int i = 0; i < 16; ++i)
            {
                const int idx = (k & 2) | (j & 1);
                out[WriteOrder[i]] = colorSet[idx];
                if (a1 && obaq == 0 && idx == 2)
                    out[WriteOrder[i]] &= 0x00FFFFFF;
                j >>= 1;
                k >>= 1;
            }
        }
        else if (b + db < 0 || b + db > 255)
        {
            // planar
            c[0][0] = ((d[0] << 1) & 0xFC) | ((d[0] >> 5) & 3);
            c[0][1] = ((d[0] << 7) & 0x80) | (d[1] & 0x7E) | (d[0] & 1);
            c[0][2] = ((d[1] << 7) & 0x80) | ((d[2] << 2) & 0x60) | ((d[2] << 3) & 0x18) | ((d[3] >> 5) & 4);
            c[0][2] |= c[0][2] >> 6;
            c[1][0] = ((d[3] << 1) & 0xF8) | ((d[3] << 2) & 4) | ((d[3] >> 5) & 3);
            c[1][1] = (d[4] & 0xFE) | (d[4] >> 7);
            c[1][2] = ((d[4] << 7) & 0x80) | ((d[5] >> 1) & 0x7C);
            c[1][2] |= c[1][2] >> 6;
            c[2][0] = ((d[5] << 5) & 0xE0) | ((d[6] >> 3) & 0x1C) | ((d[5] >> 1) & 3);
            c[2][1] = ((d[6] << 3) & 0xF8) | ((d[7] >> 5) & 6) | ((d[6] >> 4) & 1);
            c[2][2] = ((d[7] << 2) & 255) | ((d[7] >> 4) & 3);
            int i = 0;
            for (int y = 0; y <= 3; ++y)
                for (int x = 0; x <= 3; ++x)
                {
                    int v = x * (c[1][0] - c[0][0]) + y * (c[2][0] - c[0][0]) + 4 * c[0][0] + 2;
                    const int rr = v < 0 ? 0 : Clamp(v >> 2);
                    v = x * (c[1][1] - c[0][1]) + y * (c[2][1] - c[0][1]) + 4 * c[0][1] + 2;
                    const int gg = v < 0 ? 0 : Clamp(v >> 2);
                    v = x * (c[1][2] - c[0][2]) + y * (c[2][2] - c[0][2]) + 4 * c[0][2] + 2;
                    const int bb = v < 0 ? 0 : Clamp(v >> 2);
                    out[static_cast<size_t>(i)] = Pack(rr, gg, bb, 255);
                    ++i;
                }
        }
        else
        {
            // differential
            int code[2];
            code[0] = d[3] >> 5;
            code[1] = (d[3] >> 2) & 7;
            const int tbl = d[3] & 1;
            c[0][0] = r | (r >> 5);
            c[0][1] = g | (g >> 5);
            c[0][2] = b | (b >> 5);
            c[1][0] = r + dr;
            c[1][1] = g + dg;
            c[1][2] = b + db;
            c[1][0] |= c[1][0] >> 5;
            c[1][1] |= c[1][1] >> 5;
            c[1][2] |= c[1][2] >> 5;
            for (int i = 0; i < 16; ++i)
            {
                const int s = SubBlock[tbl][i];
                int m = a1 ? ModA1[obaq][code[s]][j & 1] : Mod1[code[s]][j & 1];
                if ((k & 1) != 0)
                    m = -m;
                if (a1)
                {
                    const bool transparent = obaq == 0 && (k & 1) != 0 && (j & 1) == 0;
                    if (transparent)
                        out[WriteOrder[i]] = Pack(Clamp(c[s][0] + m), Clamp(c[s][1] + m), Clamp(c[s][2] + m), 0);
                    else
                        out[WriteOrder[i]] = ApplyColor(c[s], m);
                }
                else
                    out[WriteOrder[i]] = ApplyColor(c[s], m);
                j >>= 1;
                k >>= 1;
            }
        }
    }
    else
    {
        // individual (diff bit 0)
        int code[2];
        code[0] = d[3] >> 5;
        code[1] = (d[3] >> 2) & 7;
        const int tbl = d[3] & 1;
        for (int i = 0; i < 3; ++i)
        {
            c[0][static_cast<size_t>(i)] = (d[i] & 0xF0) | (d[i] >> 4);
            c[1][static_cast<size_t>(i)] = (d[i] & 0x0F) | ((d[i] << 4) & 255);
        }
        for (int i = 0; i < 16; ++i)
        {
            const int s = SubBlock[tbl][i];
            int m = Mod1[code[s]][j & 1];
            if ((k & 1) != 0)
                m = -m;
            out[WriteOrder[i]] = ApplyColor(c[s], m);
            j >>= 1;
            k >>= 1;
        }
    }
}

auto Be64(const uint8_t *d) -> uint64_t
{
    uint64_t result = 0;
    for (int i = 0; i < 8; ++i)
        result = (result << 8) | d[i];
    return result;
}

void Etc2Alpha(const uint8_t *d, BlockPixels &out)
{
    if ((d[1] & 0xF0) != 0)
    {
        const int mult = d[1] >> 4;
        uint64_t l = Be64(d);
        for (int i = 0; i < 16; ++i)
        {
            const int a = Clamp(d[0] + mult * AlphaMod[d[1] & 0xF][l & 7]);
            auto &px = out[WriteOrderRev[i]];
            px = (px & 0x00FFFFFF) | (static_cast<uint32_t>(a) << 24);
            l >>= 3;
        }
    }
    else
        for (int i = 0; i < 16; ++i)
            out[static_cast<size_t>(i)] = (out[static_cast<size_t>(i)] & 0x00FFFFFF) | (static_cast<uint32_t>(d[0]) << 24);
}

// One EAC channel into the byte chan (0 = blue .. 2 = red) of the pixels
void EacChannel(const uint8_t *d, int chan, bool isSigned, BlockPixels &out)
{
    int mult = (d[1] >> 1) & 0x78;
    if (mult == 0)
        mult = 1;
    uint64_t l = Be64(d);
    const int base = d[0] > 127 ? d[0] - 256 : d[0];
    const int sh = chan * 8;
    for (int i = 0; i < 16; ++i)
    {
        const int val = isSigned ? base * 8 + mult * AlphaMod[d[1] & 0xF][l & 7] + 1023
                                 : d[0] * 8 + mult * AlphaMod[d[1] & 0xF][l & 7] + 4;
        int res;
        if (val < 0)
            res = 0;
        else if (val >= 2048)
            res = 255;
        else
            res = val >> 3;
        auto &px = out[WriteOrderRev[i]];
        px = (px & ~(static_cast<uint32_t>(255) << sh)) | (static_cast<uint32_t>(res) << sh);
        l >>= 3;
    }
}
} // namespace

auto BlockFormatInfo(int unityFmt, int &bw, int &bh, int &blockBytes) -> bool
{
    Kind k;
    const bool result = KindOf(unityFmt, k) && k != Kind::Bc1 && k != Kind::Bc3;
    bw = 4;
    bh = 4;
    blockBytes = 8;
    if (result && k == Kind::Astc)
    {
        switch (unityFmt)
        {
        case 48:
        case 54:
        case 66:
            bw = 4;
            break;
        case 49:
        case 55:
        case 67:
            bw = 5;
            break;
        case 50:
        case 56:
        case 68:
            bw = 6;
            break;
        case 51:
        case 57:
        case 69:
            bw = 8;
            break;
        case 52:
        case 58:
        case 70:
            bw = 10;
            break;
        default:
            bw = 12;
            break;
        }
        bh = bw;
        blockBytes = 16;
        return result;
    }
    if (result && (k == Kind::Etc2a8 || k == Kind::EacRG || k == Kind::EacRGS))
        blockBytes = 16;
    return result;
}

auto DecodableFormatInfo(int unityFmt, int &bw, int &bh, int &blockBytes) -> bool
{
    Kind k;
    bool result = KindOf(unityFmt, k);
    bw = 4;
    bh = 4;
    blockBytes = 8;
    if (!result)
        return false;
    if (k == Kind::Bc1)
        return true;
    if (k == Kind::Bc3)
    {
        blockBytes = 16;
        return true;
    }
    return BlockFormatInfo(unityFmt, bw, bh, blockBytes);
}

auto DecodeBlockOk(int unityFmt, const uint8_t *src, BlockPixels &px) -> bool
{
    Kind k;
    if (!KindOf(unityFmt, k))
    {
        px.fill(0);
        return false;
    }
    bool result = true;
    switch (k)
    {
    case Kind::Bc1:
        Bc1Block(src, px);
        break;
    case Kind::Bc3:
        Bc1Block(src + 8, px);
        Bc3Alpha(src, px);
        break;
    case Kind::Astc: {
        int bw, bh, bb;
        BlockFormatInfo(unityFmt, bw, bh, bb);
        result = AstcBlock(src, bw, bh, px.data());
        break;
    }
    case Kind::Etc1:
        Etc1Block(src, px);
        break;
    case Kind::Etc2:
        Etc2Block(src, px, false);
        break;
    case Kind::Etc2a1:
        Etc2Block(src, px, true);
        break;
    case Kind::Etc2a8:
        Etc2Block(src + 8, px, false);
        Etc2Alpha(src, px);
        break;
    default:
        for (int i = 0; i < 16; ++i)
            px[static_cast<size_t>(i)] = Pack(0, 0, 0, 255);
        EacChannel(src, 2, k == Kind::EacRS || k == Kind::EacRGS, px);
        if (k == Kind::EacRG || k == Kind::EacRGS)
            EacChannel(src + 8, 1, k == Kind::EacRGS, px);
        break;
    }
    return result;
}

void DecodeBlock(int unityFmt, const uint8_t *src, BlockPixels &px)
{
    DecodeBlockOk(unityFmt, src, px);
}

void DecodeBlockRow(int unityFmt, const uint8_t *src, int w, int h, int by, uint8_t *dst)
{
    int bw, bh, bb;
    if (!DecodableFormatInfo(unityFmt, bw, bh, bb))
        return;
    const int nx = (w + bw - 1) / bw;
    int lines = h - by * bh;
    if (lines > bh)
        lines = bh;
    BlockPixels px;
    for (int bx = 0; bx < nx; ++bx)
    {
        DecodeBlock(unityFmt, src + static_cast<int64_t>(bx) * bb, px);
        int cols = w - bx * bw;
        if (cols > bw)
            cols = bw;
        for (int y = 0; y < lines; ++y)
            for (int x = 0; x < cols; ++x)
            {
                const uint32_t value = px[static_cast<size_t>(y * bw + x)];
                std::memcpy(dst + (static_cast<int64_t>(y) * w + bx * bw + x) * 4, &value, 4);
            }
    }
}
} // namespace Dud
