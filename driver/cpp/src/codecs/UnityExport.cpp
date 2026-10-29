// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "UnityExport.h"

#include <cstring>

#include "TexDecode.h"

namespace Dud
{
namespace
{
auto FormatName(int fmt) -> std::string
{
    switch (fmt)
    {
    case 2:
        return "argb4444";
    case 7:
        return "rgb565";
    case 9:
        return "r16";
    case 13:
        return "rgba4444";
    case 28:
    case 29:
    case 64:
    case 65:
        return "crunch";
    case 62:
        return "rg16";
    default:
        break;
    }
    if ((fmt >= 15 && fmt <= 20) || fmt == 22)
        return "float";
    if (fmt >= 30 && fmt <= 33)
        return "pvrtc";
    if (fmt == 34 || fmt == 60 || fmt == 61)
        return "etc";
    if (fmt >= 41 && fmt <= 47)
        return "etc2";
    if ((fmt >= 48 && fmt <= 59) || (fmt >= 66 && fmt <= 71))
        return "astc";
    return "f" + std::to_string(fmt);
}

auto BlockBytesOf(int fmt) -> int
{
    switch (fmt)
    {
    case 10:
    case 26:
        return 8;
    case 12:
    case 24:
    case 25:
    case 27:
        return 16;
    default:
        return 0;
    }
}

void Put32(uint8_t *b, int pos, uint32_t v)
{
    b[pos] = v & 0xFF;
    b[pos + 1] = (v >> 8) & 0xFF;
    b[pos + 2] = (v >> 16) & 0xFF;
    b[pos + 3] = (v >> 24) & 0xFF;
}
} // namespace

auto SourceBytesPerPixel(int fmt) -> int
{
    switch (fmt)
    {
    case 1:
    case 63:
        return 1;
    case 3:
        return 3;
    case 2:
    case 7:
    case 13:
        return 2;
    case 4:
    case 5:
    case 14:
        return 4;
    default:
        return 0;
    }
}

auto OutBytesPerPixel(int fmt) -> int
{
    if (fmt == 34 || (fmt >= 41 && fmt <= 61) || (fmt >= 66 && fmt <= 71))
        return 4;
    if (fmt == 2 || fmt == 13)
        return 4;
    if (fmt == 7)
        return 3;
    return SourceBytesPerPixel(fmt);
}

auto PlanTexture(int fmt, int w, int h, int64_t avail) -> UxPlan
{
    UxPlan result;
    result.Fmt = fmt;
    result.W = w;
    result.H = h;
    result.Kind = UxKind::Raw;
    result.Ext = "." + FormatName(fmt) + ".bin";
    result.HeaderLen = 0;
    result.PayloadLen = avail;
    result.OutSize = avail;
    if (w < 1 || h < 1 || w > 16384 || h > 16384)
        return result;
    const int bpp = SourceBytesPerPixel(fmt);
    const int bb = BlockBytesOf(fmt);
    int dw, dh, db;
    if (bpp > 0)
    {
        const int64_t need = static_cast<int64_t>(w) * h * bpp;
        if (need > avail)
            return result;
        result.Kind = UxKind::Tga;
        result.Ext = ".tga";
        result.HeaderLen = 18;
        result.PayloadLen = need;
        result.OutSize = 18 + static_cast<int64_t>(w) * h * OutBytesPerPixel(fmt);
    }
    else if (BlockFormatInfo(fmt, dw, dh, db))
    {
        // ETC, EAC, ASTC: decoded
        const int64_t need = static_cast<int64_t>((w + dw - 1) / dw) * ((h + dh - 1) / dh) * db;
        if (need > avail)
            return result;
        result.Kind = UxKind::Decode;
        result.Ext = ".tga";
        result.HeaderLen = 18;
        result.PayloadLen = need;
        result.OutSize = 18 + static_cast<int64_t>(w) * h * 4;
    }
    else if (bb > 0)
    {
        const int64_t need = static_cast<int64_t>((w + 3) / 4) * ((h + 3) / 4) * bb;
        if (need > avail)
            return result;
        result.Kind = UxKind::Dds;
        result.Ext = ".dds";
        result.HeaderLen = 128;
        if (fmt == 24 || fmt == 25)
            result.HeaderLen = 148;
        result.PayloadLen = need;
        result.OutSize = result.HeaderLen + need;
    }
    return result;
}

auto PlanCrunch(int fmt, int w, int h, int crunchFormat, int crunchW, int crunchH, int64_t avail) -> UxPlan
{
    UxPlan result = PlanTexture(fmt, w, h, avail); // raw unless proven otherwise
    if (w < 1 || h < 1 || w != crunchW || h != crunchH)
        return result;
    if (!((fmt == 28 && crunchFormat == 0) || (fmt == 29 && crunchFormat != 0)))
        return result;
    int64_t blocks = static_cast<int64_t>((w + 3) / 4) * ((h + 3) / 4);
    blocks *= fmt == 28 ? 8 : 16;
    result.Kind = UxKind::Crunch;
    result.Ext = ".dds";
    result.HeaderLen = 128;
    result.PayloadLen = blocks;
    result.OutSize = 128 + blocks;
    return result;
}

void BuildHeader(const UxPlan &p, uint8_t *hdr)
{
    std::memset(hdr, 0, static_cast<size_t>(p.HeaderLen));
    switch (p.Kind)
    {
    case UxKind::Tga:
    case UxKind::Decode:
    case UxKind::Sprite: {
        const int selector =
            p.Kind == UxKind::Sprite ? 4 : OutBytesPerPixel(p.Fmt);
        if (selector == 1)
        {
            hdr[2] = 3;
            hdr[16] = 8;
        }
        else if (selector == 3)
        {
            hdr[2] = 2;
            hdr[16] = 24;
        }
        else
        {
            hdr[2] = 2;
            hdr[16] = 32;
            hdr[17] = 8; // 8 bits of alpha, origin at the bottom left
        }
        hdr[12] = static_cast<uint8_t>(p.W & 0xFF);
        hdr[13] = static_cast<uint8_t>(p.W >> 8);
        hdr[14] = static_cast<uint8_t>(p.H & 0xFF);
        hdr[15] = static_cast<uint8_t>(p.H >> 8);
        break;
    }
    case UxKind::Dds:
    case UxKind::Crunch: {
        std::memcpy(hdr, "DDS ", 4);
        Put32(hdr, 4, 124);
        Put32(hdr, 8, 0x00081007); // caps, height, width, pixel format, linear size
        Put32(hdr, 12, static_cast<uint32_t>(p.H));
        Put32(hdr, 16, static_cast<uint32_t>(p.W));
        Put32(hdr, 20, static_cast<uint32_t>(p.PayloadLen));
        Put32(hdr, 28, 1);  // mip levels
        Put32(hdr, 76, 32); // size of the pixel format
        Put32(hdr, 80, 4);  // it is a four character code
        const char *fourCC;
        switch (p.Fmt)
        {
        case 10:
        case 28:
            fourCC = "DXT1";
            break;
        case 12:
        case 29:
            fourCC = "DXT5";
            break;
        case 26:
            fourCC = "BC4U";
            break;
        case 27:
            fourCC = "BC5U";
            break;
        default:
            fourCC = "DX10";
            break;
        }
        std::memcpy(hdr + 84, fourCC, 4);
        Put32(hdr, 108, 0x1000); // texture
        if (p.HeaderLen == 148)
        {
            Put32(hdr, 128, p.Fmt == 24 ? 95 : 98); // BC6H_UF16 / BC7_UNORM
            Put32(hdr, 132, 3);                     // 2D texture
            Put32(hdr, 140, 1);                     // array size
        }
        break;
    }
    default:
        break;
    }
}

void ConvertPixels(int fmt, const uint8_t *src, uint8_t *dst, int pixels)
{
    switch (fmt)
    {
    case 4:
        for (int i = 0; i < pixels; ++i) // RGBA -> BGRA
        {
            const uint8_t a = src[0], b = src[1], c = src[2], d = src[3];
            dst[0] = c;
            dst[1] = b;
            dst[2] = a;
            dst[3] = d;
            src += 4;
            dst += 4;
        }
        break;
    case 5:
        for (int i = 0; i < pixels; ++i) // ARGB -> BGRA
        {
            const uint8_t a = src[0], b = src[1], c = src[2], d = src[3];
            dst[0] = d;
            dst[1] = c;
            dst[2] = b;
            dst[3] = a;
            src += 4;
            dst += 4;
        }
        break;
    case 2:
    case 13:
    case 7:
        for (int i = 0; i < pixels; ++i) // 16 bits to BGRA / BGR (dst must not overlap src)
        {
            const uint32_t v = src[0] | (static_cast<uint32_t>(src[1]) << 8);
            if (fmt == 2)
            {
                dst[0] = static_cast<uint8_t>((v & 15) * 17);
                dst[1] = static_cast<uint8_t>(((v >> 4) & 15) * 17);
                dst[2] = static_cast<uint8_t>(((v >> 8) & 15) * 17);
                dst[3] = static_cast<uint8_t>((v >> 12) * 17);
                dst += 4;
            }
            else if (fmt == 13)
            {
                dst[0] = static_cast<uint8_t>(((v >> 4) & 15) * 17);
                dst[1] = static_cast<uint8_t>(((v >> 8) & 15) * 17);
                dst[2] = static_cast<uint8_t>((v >> 12) * 17);
                dst[3] = static_cast<uint8_t>((v & 15) * 17);
                dst += 4;
            }
            else
            {
                dst[0] = static_cast<uint8_t>(((v & 31) << 3) | ((v & 31) >> 2));
                dst[1] = static_cast<uint8_t>((((v >> 5) & 63) << 2) | (((v >> 5) & 63) >> 4));
                dst[2] = static_cast<uint8_t>((((v >> 11) & 31) << 3) | (((v >> 11) & 31) >> 2));
                dst += 3;
            }
            src += 2;
        }
        break;
    case 3:
        for (int i = 0; i < pixels; ++i) // RGB -> BGR
        {
            const uint8_t a = src[0], b = src[1], c = src[2];
            dst[0] = c;
            dst[1] = b;
            dst[2] = a;
            src += 3;
            dst += 3;
        }
        break;
    default:
        if (src != dst)
            std::memmove(dst, src, static_cast<size_t>(pixels) * static_cast<size_t>(SourceBytesPerPixel(fmt)));
        break;
    }
}

auto AudioExt(const std::string &head) -> std::string
{
    if (head.compare(0, 4, "FSB5") == 0 || head.compare(0, 4, "FSB4") == 0)
        return ".fsb";
    if (head.compare(0, 4, "RIFF") == 0)
        return ".wav";
    if (head.compare(0, 4, "OggS") == 0)
        return ".ogg";
    if (head.compare(0, 3, "ID3") == 0)
        return ".mp3";
    if (head.size() >= 2 && static_cast<uint8_t>(head[0]) == 0xFF && (static_cast<uint8_t>(head[1]) & 0xE0) == 0xE0)
        return ".mp3";
    return ".bin";
}

auto TextExt(const std::string &head, int64_t len) -> std::string
{
    size_t start = 0;
    if (head.compare(0, 3, "\xEF\xBB\xBF") == 0)
        start = 3;
    for (size_t i = start; i < head.size(); ++i)
    {
        const auto c = static_cast<uint8_t>(head[i]);
        if (c < 32 && !(c == 9 || c == 10 || c == 13))
            return ".bytes";
    }
    if (start < head.size() && len > 0)
        switch (head[start])
        {
        case '{':
        case '[':
            return ".json";
        case '<':
            return ".xml";
        default:
            break;
        }
    return ".txt";
}

auto FontExt(const std::string &head) -> std::string
{
    if (head.compare(0, 4, "OTTO") == 0)
        return ".otf";
    if (head.compare(0, 4, std::string("\0\1\0\0", 4)) == 0 || head.compare(0, 4, "true") == 0)
        return ".ttf";
    if (head.compare(0, 4, "ttcf") == 0)
        return ".ttc";
    return ".bin";
}
} // namespace Dud
