// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Abuse .SPE / .SPEC (Crack dot Com, 1995) and Drake Paint .DPA support. The layouts come from
// the sources of Abuse (public domain) and of Abuso, the modern iteration (MIT): src/IO/Specs.h,
// SpecDirectory.cpp, DpaDirectory.cpp.
//
// .SPE: "SPEC1.0\0", uint16 count, then per entry: uint8 type, uint8 name length (with the null),
// name, uint8 flags, and for a link (flag 1) uint8 length + file name, otherwise uint32 size and
// uint32 offset. All little endian. Entries are shown as "<type>/<name>": the same name can be
// used by several types (an image and its palette). Images and tiles (types 4, 5, 6: uint16 width,
// uint16 height, width*height palette indices, then for the tiles a few bytes of their own) get
// the extension .img, .ftile or .btile (characters, types 7 and 21, start the same way, then their own
// data: .chr; a particle frame, type 22, is uint32 count then count of int16 x, int16 y, uint8 color: .part), and the type is kept as data_x for the interface, which
// draws them with the palette entry of the file (uint16 count, count RGB triples).
//
// .DPA ("DRAKEARC", written by Drake Paint): uint32 version (1), uint32 count, then per entry:
// uint8 type, uint8 flags (1 = link), uint32 name length, name, uint32 target length, target,
// uint64 offset, uint64 size. The types are those of .SPE plus 64, a modern item: uint8 raster
// kind, uint8 item kind, uint32 PNG length, the PNG (truecolour or indexed) and editor data; it is
// listed as the PNG itself ("Modern item/<name>.png"). A link has no data of its own: it shows
// the data of the entry that it names.

#include <cctype>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr std::string_view SpeSignature("SPEC1.0\0", 8);
constexpr std::string_view DpaSignature("DRAKEARC", 8);
constexpr uint32_t DpaVersion = 1;
constexpr uint8_t DpaModernItem = 64;
constexpr size_t ModernHeader = 1 + 1 + 4;

const char *const SpecTypes[] = {"Invalid type", "Color table", "Palette",     "Invalid type", "Image",
                                 "Fore tile",    "Back tile",   "Character",   "8 Morph",      "16 Morph",
                                 "Grue objs",    "Extern WAV",  "DMX MUS",     "Patched morph", "Normal file",
                                 "Compress1 file", "Vector image", "Light list", "Grue fgmap",  "Grue bgmap",
                                 "Data array",   "Character2",  "Particle",    "Extern lcache"};

auto TypeFolder(uint8_t type) -> std::string
{
    if (type == DpaModernItem)
        return "Modern item";
    if (type < sizeof SpecTypes / sizeof *SpecTypes)
        return SpecTypes[type];
    return "Type " + std::to_string(type);
}

// The entries that the interface can draw get an extension so that it can tell them from other data: .img (image),
// .ftile (fore tile), .btile (back tile), .chr (character: a sprite, index 0 is transparent), .part (particle frame)
auto EntryName(uint8_t type, const std::string &name) -> std::string
{
    const char *extension = type == 4 ? ".img" : type == 5 ? ".ftile" : type == 6 ? ".btile" : (type == 7 || type == 21) ? ".chr" : type == 22 ? ".part" : "";
    std::string shown = name;
    // The artists' files were PCX pictures, and the entries keep that name ("acff0001.pcx"): the new extension replaces it
    if (*extension != '\0' && shown.size() > 4)
    {
        std::string tail = shown.substr(shown.size() - 4);
        for (auto &c : tail)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (tail == ".pcx")
            shown.resize(shown.size() - 4);
    }
    return TypeFolder(type) + "/" + shown + extension;
}

class SpecBase : public Archive
{
  protected:
    static auto ReadName(Reader &stream, uint32_t length) -> std::string
    {
        if (length > stream.Size())
            Fail(DUDI7_ERR_NOTFORMAT, "A name is longer than the file");
        std::string name(length, '\0');
        if (length > 0)
            stream.ReadBuffer(name.data(), length);
        const auto end = name.find('\0');
        return end == std::string::npos ? name : name.substr(0, end);
    }
};

class SpeArchive : public SpecBase
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, SpeSignature);
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t count = _stream->U16();
        _formatId = "SPEC";
        _dirSeparator = "/";
        const int64_t size = _stream->Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint8_t type = _stream->U8();
            const std::string name = ReadName(*_stream, _stream->U8());
            const uint8_t flags = _stream->U8();
            if ((flags & 1) != 0)
            {
                ReadName(*_stream, _stream->U8()); // the file that the link names: no data in this file
                Add(EntryName(type, name), 0, 0, type);
                continue;
            }
            const uint32_t length = _stream->U32();
            const uint32_t offset = _stream->U32();
            if (offset > size || length > size - offset)
                Fail(DUDI7_ERR_NOTFORMAT, "An entry is outside of the file");
            Add(EntryName(type, name), offset, length, type);
        }
    }
};

class DpaArchive : public SpecBase
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (!StartsWith(stream, DpaSignature))
            return false;
        stream.Seek(8);
        return stream.U32() == DpaVersion;
    }

    void Parse() override
    {
        _stream->Seek(8);
        if (_stream->U32() != DpaVersion)
            Fail(DUDI7_ERR_NOTFORMAT, "Unsupported DPA version");
        const uint32_t count = _stream->U32();
        // A record is at least 26 bytes: the count cannot be more than the file can hold
        if (count > (static_cast<uint64_t>(_stream->Size()) - 16) / 26)
            Fail(DUDI7_ERR_NOTFORMAT, "The directory is outside of the file");
        _formatId = "DPA";
        _dirSeparator = "/";

        struct Record
        {
            uint8_t Type;
            bool Link;
            std::string Name, Target;
            uint64_t Offset, Size;
        };
        std::vector<Record> records;
        records.reserve(count);
        const auto size = static_cast<uint64_t>(_stream->Size());
        for (uint32_t i = 0; i < count; ++i)
        {
            Record record{};
            record.Type = _stream->U8();
            record.Link = (_stream->U8() & 1) != 0;
            record.Name = ReadName(*_stream, _stream->U32());
            record.Target = ReadName(*_stream, _stream->U32());
            record.Offset = _stream->U64();
            record.Size = _stream->U64();
            if (!record.Link && (record.Offset > size || record.Size > size - record.Offset))
                Fail(DUDI7_ERR_NOTFORMAT, "An entry is outside of the file");
            records.push_back(std::move(record));
        }

        for (const auto &record : records)
        {
            uint64_t offset = record.Offset, length = record.Size;
            uint8_t type = record.Type;
            if (record.Link)
            {
                offset = length = 0;
                for (const auto &target : records)
                {
                    if (!target.Link && target.Name == record.Target)
                    {
                        offset = target.Offset;
                        length = target.Size;
                        type = target.Type;
                        break;
                    }
                }
            }
            std::string name = EntryName(record.Type, record.Name);
            if (type == DpaModernItem && length >= ModernHeader)
            {
                // The PNG itself: rasterKind, itemKind, uint32 length, then the PNG
                _stream->Seek(static_cast<int64_t>(offset) + 2);
                const uint32_t pngLength = _stream->U32();
                if (pngLength <= length - ModernHeader)
                {
                    Add(name + ".png", static_cast<int64_t>(offset + ModernHeader), pngLength, type);
                    continue;
                }
            }
            Add(name, static_cast<int64_t>(offset), static_cast<int64_t>(length), type);
        }
    }
};
} // namespace

void RegisterAbuseSpec()
{
    Register<SpeArchive>("*.SPE;*.SPEC;*.LVL", "Abuse (*.SPE)|Abuse (*.SPEC)|Abuse levels (*.LVL)");
}

void RegisterDrakePaint()
{
    Register<DpaArchive>("*.DPA", "Drake Paint / Abuso (*.DPA)");
}
} // namespace Dud
