// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// XNA / MonoGame content files (.xnb). Not in Dragon UnPACKer 5.
// Header: "XNB", platform letter (w Windows, x Xbox 360, m Windows Phone, d/a/i... MonoGame), version
// (4 = XNA 3.1, 5 = XNA 4 and MonoGame), flags (0x80: the content is LZX compressed, 0x40: LZ4
// compressed (recent MonoGame), 0x01: HiDef), size of the file (32 bits). Compressed content has the
// size of the unpacked content (32 bits) before it. The content: number of type readers (7 bits
// encoded integer), for each the name (7 bits length + UTF-8) and a version (32 bits); number of
// shared resources; then the main object: its type (index + 1 in the readers) and its data.
// A file is one object, so the archive has one entry, named after the file:
//  - Texture2D: Color/565/5551/4444/Alpha8 as TGA (rows from the top, as in XNA), DXT1/3/5 as DDS (first
//    mip level only; no flipping needed: XNA rows are top first like DDS)
//  - SoundEffect: a WAV file (the format block is the one of the file: PCM, MS ADPCM...)
//  - Effect: the compiled effect (.fxb)
//  - anything else (SpriteFont, custom readers...): the unpacked content as it is, "<name>.<type>.bin"
// Only version 5 textures are converted.

#include <cmath>
#include <cstring>

#include "PathUtil.h"
#include "Registry.h"
#include "codecs/Lz4.h"
#include "codecs/Lzx.h"

namespace Dud
{
namespace
{
constexpr int64_t MaxContent = 512 * 1024 * 1024;
constexpr int64_t ChunkSize = 65536;

enum class Kind
{
    Raw,
    Tga,
    Dds,
    Wav,
    Text
};

struct Plan
{
    Kind PlanKind = Kind::Raw;
    int Fmt = 0; // SurfaceFormat of the textures
    int W = 0, H = 0;
    int64_t Pos = 0, Len = 0; // the source bytes in the content
    int64_t WavFmt = 0;       // WAV: position and size of the format block
    int64_t WavFmtLen = 0;
    int64_t OutSize = 0;
    std::string Text; // Kind::Text: the bytes of the entry
};

auto BlockBytes(int fmt, int w, int h) -> int64_t
{
    switch (fmt)
    {
    case 0:
        return static_cast<int64_t>(w) * h * 4;
    case 1:
    case 2:
    case 3:
        return static_cast<int64_t>(w) * h * 2;
    case 12:
        return static_cast<int64_t>(w) * h;
    case 4:
        return static_cast<int64_t>((w + 3) / 4) * ((h + 3) / 4) * 8;
    case 5:
    case 6:
        return static_cast<int64_t>((w + 3) / 4) * ((h + 3) / 4) * 16;
    default:
        return -1;
    }
}

auto Num(float f) -> int
{
    if (f != f || std::fabs(f) > 1.0E6F)
        return 0;
    return static_cast<int>(std::nearbyint(f));
}

struct Rect
{
    int32_t X = 0, Y = 0, W = 0, H = 0;
};

class XnbArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (stream.Size() < 14)
            return false;
        stream.Seek(0);
        uint8_t h[10];
        if (stream.Read(h, 10) != 10)
            return false;
        if (h[0] != 'X' || h[1] != 'N' || h[2] != 'B')
            return false;
        const bool letter = (h[3] >= 'a' && h[3] <= 'z') || (h[3] >= 'A' && h[3] <= 'Z');
        if (!letter || h[4] < 3 || h[4] > 5)
            return false;
        uint32_t size;
        std::memcpy(&size, h + 6, 4);
        return size >= 14 && size <= stream.Size();
    }

    void Parse() override
    {
        _formatId = "XNB";
        _dirSeparator = "/";
        _stream->Seek(0);
        uint8_t h[14] = {};
        _stream->ReadBuffer(h, 10);
        const uint8_t flags = h[5];
        uint32_t fileSize;
        std::memcpy(&fileSize, h + 6, 4);
        _compressed = (flags & 0xC0) != 0;
        if (_compressed)
        {
            if (fileSize < 14)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid size");
            uint32_t unpacked;
            _stream->ReadBuffer(&unpacked, 4);
            if (unpacked > MaxContent || unpacked < 2)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid unpacked size");
            std::vector<uint8_t> packed(static_cast<size_t>(fileSize) - 14 + 1);
            _content.assign(static_cast<size_t>(unpacked) + 1, 0);
            if (fileSize > 14)
                _stream->ReadBuffer(packed.data(), fileSize - 14);
            if ((flags & 0x80) != 0)
            {
                if (!LzxDecodeXna(packed.data(), static_cast<int64_t>(fileSize) - 14, _content.data(), unpacked))
                    Fail(DUDI7_ERR_NOTFORMAT, "Cannot unpack the LZX data");
            }
            else if (Lz4DecompressBlock(packed.data(), static_cast<int>(fileSize - 14), _content.data(),
                                        static_cast<int>(unpacked)) != static_cast<int>(unpacked))
                Fail(DUDI7_ERR_NOTFORMAT, "Cannot unpack the LZ4 data");
            _contentSize = unpacked;
        }
        else
            _contentSize = _stream->Size() - 10; // (the size of the header is wrong in some files)
        if (_contentSize < 2)
            Fail(DUDI7_ERR_NOTFORMAT, "Empty content");
        int64_t cur = 0;
        int64_t count;
        if (!R7(cur, count) || count > 1000)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid type readers");
        std::vector<std::string> readers(static_cast<size_t>(count));
        for (int64_t i = 0; i < count; ++i)
        {
            int64_t typeId;
            if (!R7(cur, typeId) || typeId > 1024 || cur + typeId + 4 > _contentSize)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid type reader");
            readers[static_cast<size_t>(i)].resize(static_cast<size_t>(typeId));
            if (typeId > 0)
                ReadC(cur, typeId, readers[static_cast<size_t>(i)].data());
            cur += typeId + 4;
        }
        int64_t shared, typeId;
        if (!R7(cur, shared) || !R7(cur, typeId))
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid content");
        const std::string path = _stream->Path();
        std::string base = FileNameOf(path);
        if (ToLower(ExtensionOf(base)) == ".xnb")
            base.resize(base.size() - 4);
        for (char &c : base)
            if (c == '/' || c == '\\' || static_cast<unsigned char>(c) < ' ')
                c = '_';
        if (base.empty())
            base = "content";
        Plan plan;
        std::string shortName;
        bool known = false;
        if (typeId >= 1 && typeId <= count)
        {
            std::string name = readers[static_cast<size_t>(typeId - 1)];
            const size_t comma = name.find(',');
            if (comma != std::string::npos)
                name.resize(comma);
            shortName = name;
            if (name == "Microsoft.Xna.Framework.Content.Texture2DReader")
                known = h[4] == 5 && PlanTexture(cur, plan);
            else if (name == "Microsoft.Xna.Framework.Content.SoundEffectReader")
                known = PlanSound(cur, plan);
            else if (name == "Microsoft.Xna.Framework.Content.SpriteFontReader" && h[4] == 5)
            {
                // two entries: the atlas and its description
                Plan fontPlan;
                if (PlanFont(cur, readers, base, plan, fontPlan))
                {
                    _plans = {plan, fontPlan};
                    const std::string ext = plan.PlanKind == Kind::Dds ? ".dds" : ".tga";
                    if (_compressed)
                        Add(base + ext, 0, plan.OutSize, 1);
                    else
                        Add(base + ext, 10 + plan.Pos, plan.OutSize, 1);
                    Add(base + ".fnt", 0, fontPlan.OutSize, 2);
                    return;
                }
            }
        }
        std::string ext = ".bin";
        switch (plan.PlanKind)
        {
        case Kind::Tga:
            ext = ".tga";
            break;
        case Kind::Dds:
            ext = ".dds";
            break;
        case Kind::Wav:
            ext = ".wav";
            break;
        default:
            break;
        }
        if (!known)
        {
            // the unpacked content as it is, named after the type
            plan.PlanKind = Kind::Raw;
            plan.Pos = 0;
            plan.Len = _contentSize;
            plan.OutSize = _contentSize;
            size_t j = shortName.size();
            while (j > 0 && shortName[j - 1] != '.')
                --j;
            shortName = ToLower(shortName.substr(j));
            if (shortName.size() > 6 && shortName.compare(shortName.size() - 6, 6, "reader") == 0)
                shortName.resize(shortName.size() - 6);
            for (char &c : shortName)
                if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
                    c = '_';
            if (shortName.empty())
                shortName = "content";
            ext = "." + shortName + ".bin";
            if (shortName == "effect")
            {
                // Effect: size (32 bits) and the compiled effect
                int32_t size = 0;
                if (cur + 4 <= _contentSize && ReadC(cur, 4, &size) && size > 0 && cur + 4 + size <= _contentSize)
                {
                    plan.Pos = cur + 4;
                    plan.Len = size;
                    plan.OutSize = size;
                    ext = ".fxb";
                }
            }
        }
        _plans = {plan};
        if (_compressed)
            Add(base + ext, 0, plan.OutSize, 1);
        else
            Add(base + ext, 10 + plan.Pos, plan.OutSize, 1);
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Plan plan = _plans[static_cast<size_t>(_entries[static_cast<size_t>(index)].DataX - 1)];
        if (plan.PlanKind == Kind::Text)
        {
            if (plan.Text.empty())
                return DUDI7_OK;
            return ExtractBytes(plan.Text.data(), static_cast<int64_t>(plan.Text.size()), sink);
        }
        uint8_t hdr[128] = {};
        int headerLength = 0;
        const auto put32 = [&](size_t p, uint32_t v) { std::memcpy(hdr + p, &v, 4); };
        switch (plan.PlanKind)
        {
        case Kind::Tga:
            headerLength = 18;
            hdr[2] = 2;
            hdr[12] = static_cast<uint8_t>(plan.W & 255);
            hdr[13] = static_cast<uint8_t>(plan.W >> 8);
            hdr[14] = static_cast<uint8_t>(plan.H & 255);
            hdr[15] = static_cast<uint8_t>(plan.H >> 8);
            if (plan.Fmt == 12)
            {
                hdr[2] = 3;
                hdr[16] = 8;
                hdr[17] = 0x20;
            }
            else
            {
                hdr[16] = 32;
                hdr[17] = 0x28; // 8 bits of alpha, top left origin
            }
            break;
        case Kind::Dds:
            headerLength = 128;
            std::memcpy(hdr, "DDS ", 4);
            put32(4, 124);
            put32(8, 0x81007);
            put32(12, static_cast<uint32_t>(plan.H));
            put32(16, static_cast<uint32_t>(plan.W));
            put32(20, static_cast<uint32_t>(plan.Len));
            put32(76, 32);
            put32(80, 4);
            std::memcpy(hdr + 84, plan.Fmt == 4 ? "DXT1" : plan.Fmt == 5 ? "DXT3" : "DXT5", 4);
            put32(108, 0x1000);
            break;
        case Kind::Wav:
            headerLength = 20 + static_cast<int>(plan.WavFmtLen) + 8;
            break;
        default:
            break;
        }
        int32_t result;
        if (plan.PlanKind == Kind::Wav)
        {
            // RIFF, fmt (as stored), data
            std::vector<uint8_t> buf(static_cast<size_t>(headerLength));
            std::memcpy(buf.data(), "RIFF", 4);
            const auto riff = static_cast<uint32_t>(plan.OutSize - 8);
            std::memcpy(&buf[4], &riff, 4);
            std::memcpy(&buf[8], "WAVEfmt ", 8);
            const auto fmtLen = static_cast<uint32_t>(plan.WavFmtLen);
            std::memcpy(&buf[16], &fmtLen, 4);
            if (!ReadC(plan.WavFmt, plan.WavFmtLen, &buf[20]))
            {
                LastError = "Cannot read the sound format";
                return DUDI7_ERR_IO;
            }
            std::memcpy(&buf[20 + static_cast<size_t>(plan.WavFmtLen)], "data", 4);
            const auto dataLen = static_cast<uint32_t>(plan.Len);
            std::memcpy(&buf[24 + static_cast<size_t>(plan.WavFmtLen)], &dataLen, 4);
            result = ExtractBytes(buf.data(), headerLength, sink);
        }
        else if (headerLength > 0)
            result = ExtractBytes(hdr, headerLength, sink);
        else
            result = DUDI7_OK;
        if (result != DUDI7_OK)
            return result;
        std::vector<uint8_t> buf(ChunkSize);
        std::vector<uint8_t> out(ChunkSize * 2);
        int64_t pos = plan.Pos;
        int64_t left = plan.Len;
        const bool isTga = plan.PlanKind == Kind::Tga;
        while (left > 0)
        {
            int64_t n = ChunkSize;
            if (n > left)
                n = left;
            if (isTga && (plan.Fmt == 1 || plan.Fmt == 2 || plan.Fmt == 3))
                n &= ~static_cast<int64_t>(1);
            if (isTga && plan.Fmt == 0)
                n &= ~static_cast<int64_t>(3);
            if (!ReadC(pos, n, buf.data()))
            {
                LastError = "Cannot read the data at " + std::to_string(pos);
                return DUDI7_ERR_IO;
            }
            if (isTga && plan.Fmt == 0)
            {
                for (int64_t k = 0; k < n; k += 4)
                    std::swap(buf[static_cast<size_t>(k)], buf[static_cast<size_t>(k) + 2]);
                result = ExtractBytes(buf.data(), n, sink);
            }
            else if (isTga && (plan.Fmt == 1 || plan.Fmt == 2 || plan.Fmt == 3))
            {
                int64_t px = 0;
                for (int64_t k = 0; k < n; k += 2)
                {
                    const uint32_t v = buf[static_cast<size_t>(k)] | (static_cast<uint32_t>(buf[static_cast<size_t>(k) + 1]) << 8);
                    const auto at = static_cast<size_t>(px);
                    switch (plan.Fmt)
                    {
                    case 1:
                        out[at] = Expand5(v & 31);
                        out[at + 1] = Expand6((v >> 5) & 63);
                        out[at + 2] = Expand5(v >> 11);
                        out[at + 3] = 255;
                        break;
                    case 2:
                        out[at] = Expand5(v & 31);
                        out[at + 1] = Expand5((v >> 5) & 31);
                        out[at + 2] = Expand5((v >> 10) & 31);
                        out[at + 3] = (v & 0x8000) != 0 ? 255 : 0;
                        break;
                    default:
                        out[at] = Expand4(v & 15);
                        out[at + 1] = Expand4((v >> 4) & 15);
                        out[at + 2] = Expand4((v >> 8) & 15);
                        out[at + 3] = Expand4(v >> 12);
                        break;
                    }
                    px += 4;
                }
                result = ExtractBytes(out.data(), px, sink);
            }
            else
                result = ExtractBytes(buf.data(), n, sink);
            if (result != DUDI7_OK)
                return result;
            pos += n;
            left -= n;
        }
        return DUDI7_OK;
    }

  private:
    static auto Expand5(uint32_t x) -> uint8_t
    {
        return static_cast<uint8_t>((x << 3) | (x >> 2));
    }
    static auto Expand6(uint32_t x) -> uint8_t
    {
        return static_cast<uint8_t>((x << 2) | (x >> 4));
    }
    static auto Expand4(uint32_t x) -> uint8_t
    {
        return static_cast<uint8_t>(x * 17);
    }

    auto ReadC(int64_t pos, int64_t len, void *dst) -> bool
    {
        if (pos < 0 || len < 0 || pos + len > _contentSize)
            return false;
        if (len == 0)
            return true;
        if (_compressed)
            std::memcpy(dst, &_content[static_cast<size_t>(pos)], static_cast<size_t>(len));
        else
        {
            _stream->Seek(10 + pos);
            if (_stream->Read(dst, static_cast<size_t>(len)) != static_cast<size_t>(len))
                return false;
        }
        return true;
    }

    // a 7 bits encoded integer at pos (which moves)
    auto R7(int64_t &pos, int64_t &value) -> bool
    {
        value = 0;
        int shift = 0;
        uint8_t b;
        do
        {
            if (shift > 28 || !ReadC(pos, 1, &b))
                return false;
            ++pos;
            value |= static_cast<int64_t>(b & 127) << shift;
            shift += 7;
        } while ((b & 128) != 0);
        return true;
    }

    // Texture2D data at pos: format, width, height, number of mips, then each mip (size + data)
    auto PlanTexture(int64_t pos, Plan &p) -> bool
    {
        uint32_t b[5];
        if (!ReadC(pos, 20, b))
            return false;
        const uint32_t fmt = b[0], w = b[1], h = b[2], mips = b[3], size = b[4];
        if (mips == 0 || w == 0 || h == 0 || w > 32768 || h > 32768 || fmt > 12)
            return false;
        const int64_t need = BlockBytes(static_cast<int>(fmt), static_cast<int>(w), static_cast<int>(h));
        if (need < 0 || need != size || pos + 20 + size > _contentSize)
            return false;
        p.Fmt = static_cast<int>(fmt);
        p.W = static_cast<int>(w);
        p.H = static_cast<int>(h);
        p.Pos = pos + 20;
        p.Len = size;
        if (fmt >= 4 && fmt <= 6)
        {
            p.PlanKind = Kind::Dds;
            p.OutSize = 128 + static_cast<int64_t>(size);
        }
        else
        {
            p.PlanKind = Kind::Tga;
            if (fmt == 12)
                p.OutSize = 18 + static_cast<int64_t>(size);
            else
                p.OutSize = 18 + static_cast<int64_t>(w) * h * 4;
            if (w > 65535)
                return false;
        }
        return true;
    }

    // SoundEffect: size and bytes of the WAVEFORMATEX, size and bytes of the data
    auto PlanSound(int64_t pos, Plan &p) -> bool
    {
        uint32_t l, d;
        if (!ReadC(pos, 4, &l) || l < 14 || l > 1024)
            return false;
        if (!ReadC(pos + 4 + l, 4, &d) || pos + 8 + l + d > _contentSize)
            return false;
        p.PlanKind = Kind::Wav;
        p.WavFmt = pos + 4;
        p.WavFmtLen = l;
        p.Pos = pos + 8 + l;
        p.Len = d;
        p.OutSize = 12 + 8 + l + 8 + static_cast<int64_t>(d);
        return true;
    }

    // a .NET char as BinaryReader reads it: UTF-8 of one to three bytes
    auto Chr1(int64_t &pos, uint32_t &c) -> bool
    {
        uint8_t b[3] = {};
        c = 0;
        if (!ReadC(pos, 1, &b[0]))
            return false;
        int l;
        if (b[0] < 0x80)
            l = 1;
        else if (b[0] < 0xE0)
            l = 2;
        else
            l = 3;
        if (l > 1 && !ReadC(pos + 1, l - 1, &b[1]))
            return false;
        if (l == 1)
            c = b[0];
        else if (l == 2)
            c = (static_cast<uint32_t>(b[0] & 0x1F) << 6) | (b[1] & 0x3F);
        else
            c = (static_cast<uint32_t>(b[0] & 0x0F) << 12) | (static_cast<uint32_t>(b[1] & 0x3F) << 6) | (b[2] & 0x3F);
        pos += l;
        return true;
    }

    auto I32(int64_t &pos, int32_t &v) -> bool
    {
        const bool ok = ReadC(pos, 4, &v);
        pos += 4;
        return ok;
    }

    // a List<Rectangle>: type id, count, the rectangles (x, y, width, height)
    auto RectList(int64_t &p, std::vector<Rect> &list, int32_t &count) -> bool
    {
        int64_t t;
        if (!(R7(p, t) && I32(p, count)) || count < 0 || count > 200000 ||
            count > static_cast<int64_t>(list.size()) || p + static_cast<int64_t>(count) * 16 > _contentSize)
            return false;
        for (int32_t k = 0; k < count; ++k)
        {
            Rect &r = list[static_cast<size_t>(k)];
            if (!(I32(p, r.X) && I32(p, r.Y) && I32(p, r.W) && I32(p, r.H)))
                return false;
        }
        return true;
    }

    // SpriteFont at cur (after its type id): the atlas texture and a BMFont text describing the glyphs
    auto PlanFont(int64_t cur, const std::vector<std::string> &readers, const std::string &base, Plan &tex,
                  Plan &fnt) -> bool
    {
        int64_t p = cur, t;
        if (!R7(p, t) || t < 1 || t > static_cast<int64_t>(readers.size()) ||
            readers[static_cast<size_t>(t - 1)].rfind("Microsoft.Xna.Framework.Content.Texture2DReader", 0) != 0)
            return false;
        tex = Plan();
        uint8_t hd[16];
        if (!PlanTexture(p, tex) || !ReadC(p, 16, hd))
            return false;
        uint32_t mips;
        std::memcpy(&mips, hd + 12, 4);
        if (mips < 1 || mips > 32)
            return false;
        p += 16;
        for (uint32_t i = 1; i <= mips; ++i)
        {
            uint32_t size;
            if (!ReadC(p, 4, &size) || p + 4 + static_cast<int64_t>(size) > _contentSize)
                return false;
            p += 4 + static_cast<int64_t>(size);
        }
        std::vector<Rect> glyphRects(200001), cropRects(200001);
        int32_t glyphs, crops;
        if (!RectList(p, glyphRects, glyphs) || !RectList(p, cropRects, crops) || crops != glyphs)
            return false;
        // the characters: List<char>
        int32_t numChars;
        if (!(R7(p, t) && I32(p, numChars)) || numChars != glyphs)
            return false;
        std::vector<uint32_t> chars(static_cast<size_t>(numChars) + 1);
        for (int32_t i = 0; i < numChars; ++i)
            if (!Chr1(p, chars[static_cast<size_t>(i)]))
                return false;
        int32_t lineSpacing;
        float spacing;
        if (!(I32(p, lineSpacing) && ReadC(p, 4, &spacing)))
            return false;
        p += 4;
        // kerning: List<Vector3> (left bearing, width, right bearing of every glyph)
        int32_t numKern;
        if (!(R7(p, t) && I32(p, numKern)) || numKern != glyphs || p + static_cast<int64_t>(numKern) * 12 > _contentSize)
            return false;
        std::vector<float> kern(static_cast<size_t>(numKern) * 3 + 3);
        for (int32_t i = 0; i < numKern; ++i)
        {
            if (!ReadC(p, 12, &kern[static_cast<size_t>(i) * 3]))
                return false;
            p += 12;
        }
        // the default character: a bool then the char
        uint8_t has;
        uint32_t def = 0;
        if (!ReadC(p, 1, &has))
            return false;
        ++p;
        if (has != 0)
            if (!Chr1(p, def))
                return false;
        const std::string ext = tex.PlanKind == Kind::Dds ? ".dds" : ".tga";
        // BMFont text: the pixels of a glyph are its rectangle of the atlas; it is drawn at the cropping
        // offset plus the left bearing, and the pen advances by left bearing + width + right bearing + spacing
        std::string txt = "info face=\"" + base + "\" size=" + std::to_string(lineSpacing) +
                          " bold=0 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=0 aa=1 padding=0,0,0,0 spacing=0,0\n"
                          "common lineHeight=" + std::to_string(lineSpacing) + " base=" + std::to_string(lineSpacing) +
                          " scaleW=" + std::to_string(tex.W) + " scaleH=" + std::to_string(tex.H) +
                          " pages=1 packed=0\n"
                          "page id=0 file=\"" + base + ext + "\"\n"
                          "chars count=" + std::to_string(glyphs) + "\n";
        for (int32_t i = 0; i < glyphs; ++i)
        {
            const auto at = static_cast<size_t>(i);
            const float *k = &kern[at * 3];
            const float advance = k[0] + k[1] + k[2] + spacing;
            txt += "char id=" + std::to_string(chars[at]) + " x=" + std::to_string(glyphRects[at].X) +
                   " y=" + std::to_string(glyphRects[at].Y) + " width=" + std::to_string(glyphRects[at].W) +
                   " height=" + std::to_string(glyphRects[at].H) +
                   " xoffset=" + std::to_string(cropRects[at].X + Num(k[0])) +
                   " yoffset=" + std::to_string(cropRects[at].Y) + " xadvance=" + std::to_string(Num(advance)) +
                   " page=0 chnl=15\n";
        }
        fnt = Plan();
        fnt.PlanKind = Kind::Text;
        fnt.Text = txt;
        fnt.OutSize = static_cast<int64_t>(txt.size());
        return true;
    }

    bool _compressed = false;
    std::vector<uint8_t> _content; // the unpacked content when the file is compressed
    int64_t _contentSize = 0;
    std::vector<Plan> _plans;
};
} // namespace

void RegisterXnb()
{
    Register<XnbArchive>("*.XNB", "XNA content file (*.XNB)");
}
} // namespace Dud
