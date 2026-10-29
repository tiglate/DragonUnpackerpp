// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Westwood .MIX and .PAK support (Command & Conquer, Red Alert, Tiberian Sun...), ported from
// drv_default.dpr of Dragon UnPACKer 5.
// MIX: number of files (16 bits), size of the data (32 bits), then entries of 12 bytes (hash of the name,
// offset in the data, size) and the data. Only the names of the list of v5 (codecs/MixNames) can be given
// back: the others are shown as the hash in hexadecimal, with an extension guessed from the content when
// there is none (VQA, VXL, FNT, CPS, AUD, TMP, SHP, PAL). The encrypted MIX files of Red Alert and later
// are not supported (as in v5). v5 only looked for the hashes of Command & Conquer; the hashes of Red Alert
// and Tiberian Sun are tried too here.
// PAK: no signature, a chain of the offsets of the files, each one followed by its name (null terminated).

#include <cstdio>
#include <cstring>

#include "codecs/MixNames.h"
#include "Registry.h"

namespace Dud
{
namespace
{
// Reads from a position of the file, false at the end of the file
class Probe
{
  public:
    Probe(Reader &stream, int64_t base) : _stream(stream), _base(base)
    {
    }
    auto Bytes(int64_t at, void *buffer, size_t count) -> bool
    {
        _stream.Seek(_base + at);
        return _stream.Read(buffer, count) == count;
    }
    auto U16(int64_t at, uint32_t &value) -> bool
    {
        uint8_t b[2];
        if (!Bytes(at, b, 2))
            return false;
        value = static_cast<uint32_t>(b[0] | (b[1] << 8));
        return true;
    }
    auto U32(int64_t at, uint32_t &value) -> bool
    {
        uint8_t b[4];
        if (!Bytes(at, b, 4))
            return false;
        value = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) |
                (static_cast<uint32_t>(b[3]) << 24);
        return true;
    }

  private:
    Reader &_stream;
    int64_t _base;
};

auto GuessExtension(Reader &stream, int64_t offset, int64_t size) -> std::string
{
    Probe p(stream, offset);
    char c4[4];
    char c8[8];
    char c12[12];
    if (!p.Bytes(0, c4, 4))
        return "";
    if (std::memcmp(c4, "FORM", 4) == 0 && p.Bytes(8, c8, 8) && std::memcmp(c8, "WVQAVQHD", 8) == 0)
        return ".vqa";
    if (std::memcmp(c4, "Voxe", 4) == 0 && p.Bytes(4, c12, 12) && std::memcmp(c12, "l Animation", 11) == 0)
        return ".vxl";
    uint32_t num2 = 0, num4 = 0, num4b = 0;
    if (p.U16(0, num2))
    {
        if (static_cast<int64_t>(num2) == size)
        {
            if (p.U16(2, num2) && num2 == 0x0500 && p.U16(4, num2) && num2 == 0x000E && p.U16(6, num2) &&
                num2 == 0x0014)
                return ".fnt";
        }
        else if (static_cast<int64_t>(num2) == size - 2)
        {
            if (p.U16(2, num2) && num2 == 0x0004 && p.U16(4, num2) && num2 == 0xFA00)
                return ".cps";
        }
        if (p.U32(2, num4) && num2 <= 44100 && static_cast<int64_t>(num4) + 12 == size)
        {
            uint8_t num1;
            if (p.U32(6, num4b) && num4b >= num4 && p.Bytes(11, &num1, 1) && (num1 == 0 || num1 == 1 || num1 == 99))
                return ".aud";
        }
        if (p.U32(8, num4) && num4 == 48 && p.U32(12, num4) && num4 == 24)
            return ".tmp";
    }
    // SHP: header of 14 bytes, then 8 bytes per image
    uint32_t images = 0;
    if (p.U16(0, images) && images > 0)
    {
        bool isShp = true;
        for (uint32_t x = 0; x < images && isShp; ++x)
        {
            uint32_t off, ref;
            if (!p.U32(14 + static_cast<int64_t>(x) * 8, off) || !p.U32(14 + static_cast<int64_t>(x) * 8 + 4, ref))
            {
                isShp = false;
                break;
            }
            const uint32_t kind = off & 0xFF000000u;
            isShp = (off & 0x00FFFFFFu) < size &&
                    ((kind == 0x80000000u && ref == 0) || (kind == 0x40000000u && (ref & 0x00FFFFFFu) < size) ||
                     (kind == 0x20000000u && (ref & 0xFF000000u) == 0x48000000u) || kind == 0);
        }
        if (isShp)
            return ".shp";
    }
    if (size == 768)
        return ".pal";
    return "";
}

class MixArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 6)
            return false;
        const uint16_t count = stream.U16();
        const uint32_t size = stream.U32();
        if (count == 0 || static_cast<int64_t>(size) + count * 12 + 6 != stream.Size())
            return false;
        const int64_t body = count * 12 + 6;
        for (uint16_t x = 0; x < count; ++x)
        {
            stream.U32();
            const uint32_t offset = stream.U32();
            const uint32_t length = stream.U32();
            if (static_cast<int64_t>(offset) + length > size)
                return false;
        }
        (void)body;
        return true;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint16_t count = _stream->U16();
        _stream->U32();
        const int64_t body = count * 12 + 6;
        _formatId = "MIX";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (uint16_t x = 0; x < count; ++x)
        {
            _stream->Seek(6 + static_cast<int64_t>(x) * 12);
            const uint32_t id = _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            if (body + offset + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            std::string name = MixNameCommandAndConquer(id);
            if (name.empty())
                name = MixNameRedAlert(id);
            if (name.empty())
            {
                char hex[16];
                std::snprintf(hex, sizeof hex, "%08X", id);
                name = hex;
            }
            if (name.find('.') == std::string::npos)
                name += GuessExtension(*_stream, body + offset, size);
            Add(name, body + offset, size);
        }
    }
};

class PakArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        std::vector<Item> items;
        return Walk(stream, items) && !items.empty();
    }

    void Parse() override
    {
        std::vector<Item> items;
        if (!Walk(*_stream, items) || items.empty())
            Fail(DUDI7_ERR_NOTFORMAT, "Not a Westwood PAK");
        _formatId = "WWPAK";
        _dirSeparator = "";
        _extractInternal = false;
        for (const Item &i : items)
            Add(i.Name, i.Offset, i.Size);
    }

  private:
    struct Item
    {
        std::string Name;
        int64_t Offset;
        int64_t Size;
    };

    static auto Walk(Reader &stream, std::vector<Item> &items) -> bool
    {
        const int64_t total = stream.Size();
        if (total < 5)
            return false;
        int64_t oldOffset = 0;
        std::string disp;
        stream.Seek(0);
        try
        {
            for (;;)
            {
                const int64_t offset = stream.U32();
                if (offset != 0 && (offset < oldOffset || offset > total))
                    return false;
                if (oldOffset > 0)
                {
                    const int64_t size = offset == 0 ? total - oldOffset : offset - oldOffset;
                    if (disp.empty())
                        return false;
                    items.push_back({disp, oldOffset, size});
                    if (items.size() > 100000)
                        return false;
                }
                if (offset > 0 && total > offset)
                {
                    disp = stream.CString(256);
                    for (char c : disp)
                        if (static_cast<unsigned char>(c) < ' ')
                            return false;
                }
                oldOffset = offset;
                if (offset == 0 || offset == total)
                    break;
            }
        }
        catch (const Error &)
        {
            return false;
        }
        catch (const ReadError &)
        {
            return false;
        }
        // the table comes before the data of the first file
        return items.empty() || stream.Pos() <= items.front().Offset;
    }
};
} // namespace

void RegisterWestWood()
{
    Register<MixArchive>("*.MIX", "Command & Conquer, Red Alert, Tiberian Sun (*.MIX)");
    Register<PakArchive>("*.PAK;*.TLK", "Westwood (*.PAK;*.TLK)");
}
} // namespace Dud
