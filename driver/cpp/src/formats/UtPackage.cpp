// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Unreal engine packages (.UAX .UMX .UMS .UTX .UKX .USX .U): Unreal, Unreal Tournament, Rune, Deus Ex, Harry Potter,
// Undying, UT2003/2004... Port of drv_ut of Dragon UnPACKer 5, which used the TUTPackage unit of Antonio Cordero Balcazar
// (utpackages, MPL 1.1); written again here from that unit, for the parts the driver used:
//   header: 0x9E2A83C1, version, licensee, flags, then the counts and offsets of the name, export and import tables
//   (a table of GUIDs and generations follows, not needed). Names: size and characters (version 64 and later) or
//   a null terminated text, then flags. Exports: class, super, group, name, flags, size, offset (compact indexes).
// The entries are the exports other than packages, in the folders of their groups. Sounds (the data is the WAV file),
// music (the module: it, s3m, xm, umx...) and textures (as a .bmp: the first mip map, palettised, DXT or RGBA) are
// read from the objects; all other objects are given as they are in the package.
// Not supported: packages encrypted (Undying, America's Army, Lineage 2) and the objects of UT2003 and later, Unreal 2
// and Splinter Cell (given as they are). No real package was available: only fixtures made after the unit.

#include <cstring>
#include <map>

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t Magic = 0x9E2A83C1;
constexpr uint32_t PkgEncrypted = 0x20;
constexpr uint32_t MaxItems = 4000000;
constexpr int64_t MaxObject = 512LL * 1024 * 1024;

enum Kind : int64_t
{
    KRaw = 0,
    KSound = 1,
    KMusic = 2,
    KTexture = 3
};

constexpr int TEXF_P8 = 0, TEXF_RGBA7 = 1, TEXF_RGB16 = 2, TEXF_DXT1 = 3, TEXF_RGB8 = 4, TEXF_RGBA8 = 5, TEXF_DXT3 = 7, TEXF_DXT5 = 8, TEXF_L8 = 9, TEXF_G16 = 10;

// Reads the bytes of an object (the same reading as the one of the package: compact indexes, little endian)
class Cursor
{
  public:
    Cursor(const uint8_t *data, size_t size) : _data(data), _size(size)
    {
    }
    auto Pos() const -> int64_t
    {
        return static_cast<int64_t>(_p);
    }
    void Seek(int64_t position)
    {
        if (position < 0 || static_cast<size_t>(position) > _size)
            throw Error(DUDI7_ERR_IO, "Position outside of the object");
        _p = static_cast<size_t>(position);
    }
    auto U8() -> uint8_t
    {
        Need(1);
        return _data[_p++];
    }
    auto U16() -> uint32_t
    {
        const uint32_t a = U8();
        return a | (static_cast<uint32_t>(U8()) << 8);
    }
    auto U32() -> uint32_t
    {
        const uint32_t a = U16();
        return a | (U16() << 16);
    }
    auto Idx() -> int32_t
    {
        const uint8_t b0 = U8();
        uint32_t result = 0;
        if ((b0 & 0x40) != 0)
        {
            const uint8_t b1 = U8();
            if ((b1 & 0x80) != 0)
            {
                const uint8_t b2 = U8();
                if ((b2 & 0x80) != 0)
                {
                    const uint8_t b3 = U8();
                    if ((b3 & 0x80) != 0)
                        result = U8();
                    result = (result << 7) | (b3 & 0x7F);
                }
                result = (result << 7) | (b2 & 0x7F);
            }
            result = (result << 7) | (b1 & 0x7F);
        }
        result = (result << 6) | (b0 & 0x3F);
        return (b0 & 0x80) != 0 ? -static_cast<int32_t>(result) : static_cast<int32_t>(result);
    }
    void Bytes(void *to, size_t count)
    {
        Need(count);
        std::memcpy(to, _data + _p, count);
        _p += count;
    }
    auto Left() const -> size_t
    {
        return _size - _p;
    }
    auto Data() const -> const uint8_t *
    {
        return _data + _p;
    }

  private:
    void Need(size_t n) const
    {
        if (n > _size - _p)
            throw Error(DUDI7_ERR_IO, "Unexpected end of the object");
    }
    const uint8_t *_data;
    size_t _size;
    size_t _p = 0;
};

struct Properties
{
    std::map<std::string, int64_t> Values; // lower case names: the first element of arrays, true = 1
};

void Le(std::vector<uint8_t> &out, uint32_t v, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

// A BMP file: 8 bits with a palette, 24 or 32 bits; rows[0] is the top row, size per row = width * bytesPerPixel
auto MakeBmp(int width, int height, int bpp, const std::vector<uint8_t> &pixels, const std::vector<uint32_t> &palette) -> std::vector<uint8_t>
{
    const int rowBytes = (width * bpp / 8 + 3) / 4 * 4;
    const int paletteBytes = bpp == 8 ? 1024 : 0;
    const uint32_t dataAt = 14 + 40 + static_cast<uint32_t>(paletteBytes);
    std::vector<uint8_t> out;
    out.reserve(dataAt + static_cast<size_t>(rowBytes) * static_cast<size_t>(height));
    out.push_back('B');
    out.push_back('M');
    Le(out, dataAt + static_cast<uint32_t>(rowBytes) * static_cast<uint32_t>(height), 4);
    Le(out, 0, 4);
    Le(out, dataAt, 4);
    Le(out, 40, 4);
    Le(out, static_cast<uint32_t>(width), 4);
    Le(out, static_cast<uint32_t>(height), 4);
    Le(out, 1, 2);
    Le(out, static_cast<uint32_t>(bpp), 2);
    Le(out, 0, 4);
    Le(out, static_cast<uint32_t>(rowBytes) * static_cast<uint32_t>(height), 4);
    Le(out, 2835, 4);
    Le(out, 2835, 4);
    Le(out, bpp == 8 ? 256 : 0, 4);
    Le(out, 0, 4);
    if (bpp == 8)
        for (int i = 0; i < 256; ++i)
        {
            const uint32_t c = static_cast<size_t>(i) < palette.size() ? palette[static_cast<size_t>(i)] : static_cast<uint32_t>(i) * 0x010101u;
            out.push_back(static_cast<uint8_t>(c >> 16)); // the colors are stored as red, green, blue, alpha: 0xAABBGGRR
            out.push_back(static_cast<uint8_t>(c >> 8));
            out.push_back(static_cast<uint8_t>(c));
            out.push_back(0);
        }
    const size_t pixelRow = static_cast<size_t>(width) * static_cast<size_t>(bpp / 8);
    for (int y = height - 1; y >= 0; --y)
    {
        const size_t start = out.size();
        out.insert(out.end(), pixels.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(y) * pixelRow),
                   pixels.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(y + 1) * pixelRow));
        out.resize(start + static_cast<size_t>(rowBytes), 0);
    }
    return out;
}

// DXT1, DXT3 and DXT5 into BGRA, as the unit did (the colors of a block with the transparent index for DXT1)
void DecodeDxt(int format, Cursor &c, int width, int height, std::vector<uint8_t> &bgra)
{
    const int by4 = std::max(1, height / 4), bx4 = std::max(1, width / 4);
    for (int by = 0; by < by4; ++by)
        for (int bx = 0; bx < bx4; ++bx)
        {
            uint8_t alpha[4][4];
            std::memset(alpha, 0xFF, sizeof alpha);
            if (format == TEXF_DXT3)
            {
                for (int row = 0; row < 4; ++row)
                {
                    uint32_t av = c.U16();
                    for (int col = 0; col < 4; ++col)
                    {
                        alpha[row][col] = static_cast<uint8_t>((av & 0x0F) << 4);
                        av >>= 4;
                    }
                }
            }
            else if (format == TEXF_DXT5)
            {
                const uint8_t a0 = c.U8(), a1 = c.U8();
                uint8_t values[8];
                values[0] = a0;
                values[1] = a1;
                if (a0 > a1)
                    for (int i = 0; i < 6; ++i)
                        values[2 + i] = static_cast<uint8_t>((a0 * (6 - i) + a1 * (i + 1)) / 7);
                else
                {
                    for (int i = 0; i < 4; ++i)
                        values[2 + i] = static_cast<uint8_t>((a0 * (4 - i) + a1 * (i + 1)) / 5);
                    values[6] = 0;
                    values[7] = 0xFF;
                }
                for (int half = 0; half < 2; ++half)
                {
                    uint32_t bits = c.U16();
                    bits |= static_cast<uint32_t>(c.U8()) << 16;
                    for (int i = 0; i < 8; ++i)
                    {
                        alpha[half * 2 + i / 4][i % 4] = values[bits & 7];
                        bits >>= 3;
                    }
                }
            }
            const uint32_t c0 = c.U16(), c1 = c.U16();
            uint8_t colors[4][4]; // b, g, r, a
            auto expand = [&](uint32_t v, uint8_t *to) {
                to[0] = static_cast<uint8_t>((v & 0x1F) << 3);
                to[1] = static_cast<uint8_t>((v & 0x07E0) >> 3);
                to[2] = static_cast<uint8_t>((v & 0xF800) >> 8);
                to[3] = 0xFF;
            };
            expand(c0, colors[0]);
            expand(c1, colors[1]);
            for (int k = 0; k < 3; ++k)
            {
                if (c0 > c1 || format != TEXF_DXT1)
                {
                    colors[2][k] = static_cast<uint8_t>((2 * colors[0][k] + colors[1][k]) / 3);
                    colors[3][k] = static_cast<uint8_t>((colors[0][k] + 2 * colors[1][k]) / 3);
                }
                else
                {
                    colors[2][k] = static_cast<uint8_t>((colors[0][k] + colors[1][k]) / 2);
                    colors[3][k] = 0;
                }
            }
            colors[2][3] = 0xFF;
            colors[3][3] = (c0 > c1 || format != TEXF_DXT1) ? 0xFF : 0;
            for (int row = 0; row < 4; ++row)
            {
                uint8_t indexes = c.U8();
                for (int col = 0; col < 4; ++col)
                {
                    const int y = by * 4 + row, x = bx * 4 + col;
                    if (y < height && x < width)
                    {
                        uint8_t *px = &bgra[(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4];
                        std::memcpy(px, colors[indexes & 3], 3);
                        px[3] = format == TEXF_DXT1 ? colors[indexes & 3][3] : alpha[row][col];
                    }
                    indexes >>= 2;
                }
            }
        }
}

class UtPackageArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        return stream.Size() >= 36 && stream.U32() == Magic;
    }

    void Parse() override
    {
        _stream->Seek(4);
        _version = _stream->U16();
        _licensee = _stream->U16();
        const uint32_t flags = _stream->U32();
        if ((flags & PkgEncrypted) != 0 && _licensee == 0 && _version >= 60)
            Fail(DUDI7_ERR_NOTFORMAT, "Encrypted package");
        const uint32_t nameCount = _stream->U32(), nameOffset = _stream->U32();
        const uint32_t exportCount = _stream->U32(), exportOffset = _stream->U32();
        const uint32_t importCount = _stream->U32(), importOffset = _stream->U32();
        const int64_t total = _stream->Size();
        if (nameCount > MaxItems || exportCount > MaxItems || importCount > MaxItems || nameOffset >= total || exportOffset >= total || importOffset >= total)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid tables");

        _stream->Seek(nameOffset);
        for (uint32_t i = 0; i < nameCount; ++i)
        {
            _names.push_back(ReadName());
            _stream->U32();
        }
        _stream->Seek(exportOffset);
        for (uint32_t i = 0; i < exportCount; ++i)
        {
            Export e;
            e.Class = Index();
            Index(); // super
            e.Group = static_cast<int32_t>(_stream->U32());
            e.Object = Index();
            e.Flags = _stream->U32();
            e.Size = Index();
            e.Offset = e.Size > 0 ? Index() : 0;
            _exports.push_back(e);
        }
        _stream->Seek(importOffset);
        for (uint32_t i = 0; i < importCount; ++i)
        {
            Index(); // package of the class
            Import im;
            im.Class = Index();
            _stream->U32(); // package
            im.Object = Index();
            _imports.push_back(im);
        }

        _formatId = "UTPKG";
        _dirSeparator = "\\";
        _extractInternal = true;
        for (size_t i = 0; i < _exports.size(); ++i)
        {
            const Export &e = _exports[i];
            const std::string cls = ClassName(e.Class);
            if (cls == "Package")
                continue;
            if (e.Size < 0 || e.Offset < 0 || static_cast<int64_t>(e.Offset) + e.Size > total)
                continue; // (an object that is not in this package)
            std::string name = Name(e.Object);
            std::string ext = cls;
            int64_t kind = KRaw;
            if (cls == "Sound")
            {
                ext = "wav";
                kind = KSound;
            }
            else if (cls == "Music")
            {
                ext = _names.empty() ? "music" : _names[0];
                kind = KMusic;
            }
            else if (cls == "Texture")
            {
                ext = "bmp";
                kind = KTexture;
            }
            else if (cls == "Palette")
                ext = "pal";
            else if (cls == "SkelModel")
                ext = "skl";
            else if (cls == "FireTexture")
                ext = "fire";
            Add(GroupPath(e.Group) + name + "." + ext, e.Offset, kind == KSound || kind == KMusic ? SoundSize(e, kind, e.Size) : e.Size, static_cast<int64_t>(i), kind);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataY != KRaw && _version < 100 && _licensee == 0)
        {
            try
            {
                std::vector<uint8_t> out;
                if (Convert(entry, out))
                    return ExtractBytes(out.data(), static_cast<int64_t>(out.size()), sink);
            }
            catch (const Error &)
            {
            }
            catch (const ReadError &)
            {
            }
        }
        return Archive::Extract(index, sink); // (the object as it is)
    }

  private:
    struct Export
    {
        int32_t Class = 0, Group = 0, Object = 0, Size = 0, Offset = 0;
        uint32_t Flags = 0;
    };
    struct Import
    {
        int32_t Class = 0, Object = 0;
    };

    auto Index() -> int32_t
    {
        // the package is read through the stream: bytes one by one, as the unit does
        uint8_t b[5];
        b[0] = _stream->U8();
        size_t n = 1;
        if ((b[0] & 0x40) != 0)
        {
            b[n++] = _stream->U8();
            if ((b[1] & 0x80) != 0)
            {
                b[n++] = _stream->U8();
                if ((b[2] & 0x80) != 0)
                {
                    b[n++] = _stream->U8();
                    if ((b[3] & 0x80) != 0)
                        b[n++] = _stream->U8();
                }
            }
        }
        Cursor parse(b, n);
        return parse.Idx();
    }

    auto ReadName() -> std::string
    {
        if (_version >= 64)
        {
            int32_t size = Index();
            if (size < 0) // UTF-16 (later engines)
            {
                std::u16string text;
                for (int32_t i = 0; i < -size; ++i)
                    text.push_back(static_cast<char16_t>(_stream->U16()));
                if (!text.empty() && text.back() == 0)
                    text.pop_back();
                return Utf16ToUtf8(text);
            }
            if (size > 1024)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid name");
            std::string text(static_cast<size_t>(size), '\0');
            if (size > 0)
                _stream->ReadBuffer(text.data(), text.size());
            return ZString(text.data(), text.size());
        }
        return _stream->CString(1024);
    }

    auto Name(int32_t index) const -> std::string
    {
        return index >= 0 && static_cast<size_t>(index) < _names.size() ? _names[static_cast<size_t>(index)] : std::string("<noname>");
    }

    // The object of a reference: positive = export, negative = import, 0 = none
    auto ClassName(int32_t reference) const -> std::string
    {
        if (reference > 0 && static_cast<size_t>(reference) <= _exports.size())
            return Name(_exports[static_cast<size_t>(reference) - 1].Object);
        if (reference < 0 && static_cast<size_t>(-static_cast<int64_t>(reference)) <= _imports.size())
            return Name(_imports[static_cast<size_t>(-static_cast<int64_t>(reference)) - 1].Object);
        return "Class";
    }

    auto GroupPath(int32_t group) const -> std::string
    {
        std::string path;
        for (int depth = 0; depth < 16 && group > 0 && static_cast<size_t>(group) <= _exports.size(); ++depth)
        {
            const Export &g = _exports[static_cast<size_t>(group) - 1];
            path = Name(g.Object) + "\\" + path;
            group = g.Group;
        }
        return path;
    }

    // The size of the sound or the music that the entry gives (the data of the object): from the start of the object only
    auto SoundSize(const Export &e, int64_t kind, int64_t fallback) -> int64_t
    {
        if (_version >= 100 || _licensee != 0 || e.Size <= 0 || e.Offset < 0)
            return fallback;
        try
        {
            std::vector<uint8_t> head(static_cast<size_t>(std::min<int64_t>(e.Size, 1024)));
            _stream->Seek(e.Offset);
            _stream->ReadBuffer(head.data(), head.size());
            Cursor c(head.data(), head.size());
            if (kind == KMusic)
            {
                c.U16();
                c.U32();
            }
            else
            {
                ReadProperties(c);
                c.Idx();
                if (_version >= 83 && _version >= 79)
                    return fallback; // (Undying: the extra values)
                if (_version >= 63)
                    c.U32();
            }
            const int32_t size = c.Idx();
            return size >= 0 && c.Pos() + size <= e.Size ? size : fallback;
        }
        catch (const Error &)
        {
        }
        catch (const ReadError &)
        {
        }
        return fallback;
    }

    auto ReadObject(const Export &e) -> std::vector<uint8_t>
    {
        if (e.Size < 0 || e.Size > MaxObject || e.Offset < 0 || static_cast<int64_t>(e.Offset) + e.Size > _stream->Size())
            throw Error(DUDI7_ERR_IO, "Object outside of the package");
        std::vector<uint8_t> data(static_cast<size_t>(e.Size));
        _stream->Seek(e.Offset);
        if (!data.empty())
            _stream->ReadBuffer(data.data(), data.size());
        return data;
    }

    // The list of properties at the start of an object (up to the name "None")
    auto ReadProperties(Cursor &c) const -> Properties
    {
        Properties props;
        for (int count = 0; count < 4096; ++count)
        {
            const std::string name = Name(c.Idx());
            if (name == "None")
                return props;
            const uint8_t info = c.U8();
            const int type = info & 0x0F;
            const int sizeCode = (info >> 4) & 7;
            const bool array = (info & 0x80) != 0;
            if (type == 10) // struct: the name of the struct
                c.Idx();
            uint32_t size = 0;
            switch (sizeCode)
            {
            case 0:
                size = 1;
                break;
            case 1:
                size = 2;
                break;
            case 2:
                size = 4;
                break;
            case 3:
                size = 12;
                break;
            case 4:
                size = 16;
                break;
            case 5:
                size = c.U8();
                break;
            case 6:
                size = c.U16();
                break;
            default:
                size = c.U32();
            }
            bool first = true;
            if (array && type != 3)
            {
                const uint8_t b = c.U8();
                if ((b & 0x80) == 0)
                    first = b == 0;
                else if ((b & 0xC0) == 0x80)
                {
                    const uint8_t b2 = c.U8();
                    first = (((b & 0x7F) << 8) | b2) == 0;
                }
                else
                {
                    c.U8();
                    c.U8();
                    c.U8();
                    first = false;
                }
            }
            std::string key = name;
            for (char &ch : key)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (type == 3) // booleans have no data: the flag of the array bit
            {
                if (first)
                    props.Values[key] = array ? 1 : 0;
                continue;
            }
            if (size > c.Left())
                throw Error(DUDI7_ERR_IO, "Invalid property");
            std::vector<uint8_t> value(size);
            if (size > 0)
                c.Bytes(value.data(), size);
            if (!first)
                continue;
            if (type == 1 && size >= 1) // byte
                props.Values[key] = value[0];
            else if (type == 2 && size >= 4)
            {
                int32_t v;
                std::memcpy(&v, value.data(), 4);
                props.Values[key] = v;
            }
            else if (type == 5 && size >= 1) // object: a compact index
            {
                Cursor v(value.data(), value.size());
                props.Values[key] = v.Idx();
            }
        }
        throw Error(DUDI7_ERR_IO, "Too many properties");
    }

    auto Convert(const Entry &entry, std::vector<uint8_t> &out) -> bool
    {
        const Export &e = _exports[static_cast<size_t>(entry.DataX)];
        const std::vector<uint8_t> object = ReadObject(e);
        Cursor c(object.data(), object.size());
        if (entry.DataY == KMusic)
        {
            // no properties: a 16 bits value, the offset of what follows and the module as an array of bytes
            c.U16();
            c.U32();
            return Array(c, out);
        }
        const Properties props = ReadProperties(c);
        if (entry.DataY == KSound)
        {
            c.Idx(); // the format
            if (_licensee == 0 && _version >= 79 && _version >= 83)
            {
                // Undying: unknown values before the data
                for (int i = 0; i < 5; ++i)
                    c.U32();
                if (_version > 79)
                    c.U32();
                if (_version > 81)
                    c.U32();
            }
            if (_version >= 63)
                c.U32(); // where the next object is
            return Array(c, out);
        }
        return Texture(e, props, c, out);
    }

    static auto Array(Cursor &c, std::vector<uint8_t> &out) -> bool
    {
        const int32_t size = c.Idx();
        if (size < 0 || static_cast<size_t>(size) > c.Left())
            return false;
        out.assign(c.Data(), c.Data() + size);
        return true;
    }

    // The colors of a palette object, as 0xAABBGGRR
    auto Palette(int32_t reference) -> std::vector<uint32_t>
    {
        std::vector<uint32_t> colors;
        if (reference <= 0 || static_cast<size_t>(reference) > _exports.size())
            return colors;
        const std::vector<uint8_t> object = ReadObject(_exports[static_cast<size_t>(reference) - 1]);
        Cursor c(object.data(), object.size());
        ReadProperties(c);
        const int32_t count = c.Idx();
        if (count < 0 || count > 4096 || static_cast<size_t>(count) * 4 > c.Left())
            return colors;
        for (int32_t i = 0; i < count; ++i)
            colors.push_back(c.U32());
        return colors;
    }

    // The first mip map as a BMP file
    auto Texture(const Export &e, const Properties &props, Cursor &c, std::vector<uint8_t> &out) -> bool
    {
        auto value = [&](const char *name, int64_t fallback) {
            const auto found = props.Values.find(name);
            return found == props.Values.end() ? fallback : found->second;
        };
        if (_licensee != 0 || _version >= 100)
            return false;
        auto readMip = [&](int format) -> bool {
            int64_t after;
            int32_t blockSize;
            int64_t dataAt;
            if (_version >= 63)
            {
                after = static_cast<int64_t>(c.U32()) - e.Offset;
                blockSize = c.Idx();
                dataAt = c.Pos();
            }
            else
            {
                blockSize = c.Idx();
                dataAt = c.Pos();
                after = dataAt + blockSize;
            }
            if (blockSize <= 0)
                return false;
            c.Seek(after);
            const int width = static_cast<int>(c.U32() & 0x2FFF);
            const int height = static_cast<int>(c.U32() & 0x2FFF);
            c.Seek(dataAt);
            if (width < 1 || height < 1 || width > 8192 || height > 8192)
                return false;
            const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
            std::vector<uint8_t> data;
            int bpp = 24;
            std::vector<uint32_t> palette;
            switch (format)
            {
            case TEXF_P8:
                if (c.Left() < pixels)
                    return false;
                data.assign(c.Data(), c.Data() + pixels);
                palette = Palette(static_cast<int32_t>(value("palette", 0)));
                bpp = 8;
                break;
            case TEXF_DXT1:
            case TEXF_DXT3:
            case TEXF_DXT5:
                data.assign(pixels * 4, 0);
                DecodeDxt(format, c, width, height, data);
                bpp = 32;
                break;
            case TEXF_RGBA8:
                if (c.Left() < pixels * 4)
                    return false;
                data.assign(c.Data(), c.Data() + pixels * 4);
                bpp = 32;
                break;
            case TEXF_RGBA7:
                if (c.Left() < pixels * 4)
                    return false;
                data.assign(c.Data(), c.Data() + pixels * 4);
                for (uint8_t &b : data)
                    b = static_cast<uint8_t>(b << 1);
                bpp = 32;
                break;
            case TEXF_RGB8:
                if (c.Left() < pixels * 3)
                    return false;
                data.assign(c.Data(), c.Data() + pixels * 3);
                break;
            case TEXF_RGB16:
                if (c.Left() < pixels * 6)
                    return false;
                for (size_t i = 0; i < pixels * 3; ++i)
                    data.push_back(static_cast<uint8_t>(c.U16() / 256));
                break;
            case TEXF_L8:
                if (c.Left() < pixels)
                    return false;
                for (size_t i = 0; i < pixels; ++i)
                    data.insert(data.end(), 3, c.U8());
                break;
            case TEXF_G16:
                if (c.Left() < pixels * 2)
                    return false;
                for (size_t i = 0; i < pixels; ++i)
                    data.insert(data.end(), 3, static_cast<uint8_t>(c.U16() / 256));
                break;
            default:
                return false;
            }
            out = MakeBmp(width, height, bpp, data, palette);
            return true;
        };
        const int mips = c.U8();
        if (mips > 0)
            return readMip(static_cast<int>(value("format", TEXF_P8)));
        if (value("bhascomp", 0) != 0)
        {
            if (c.U8() > 0)
                return readMip(static_cast<int>(value("compformat", TEXF_DXT1)));
        }
        return false;
    }

    uint32_t _version = 0, _licensee = 0;
    std::vector<std::string> _names;
    std::vector<Export> _exports;
    std::vector<Import> _imports;
};
} // namespace

void RegisterUtPackage()
{
    Register<UtPackageArchive>("*.UAX;*.UMX;*.UMS;*.UTX;*.UKX;*.USX;*.U;*.UMOD",
                               "Unreal, Unreal Tournament, Rune, Deus Ex, Harry Potter... packages (*.UAX;*.UMX;*.UMS;*.UTX;*.UKX;*.USX;*.U)");
}
} // namespace Dud
