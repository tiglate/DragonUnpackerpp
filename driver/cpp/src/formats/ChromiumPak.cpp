// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Chromium / Chrome Embedded Framework / NW.js / Electron .PAK (data pack) support. Not in
// Dragon UnPACKer 5: written from the layout of ui/base/resource/data_pack.cc and checked on
// real files (CrossCode, data pack version 5).
// Version 5: version (32 bits), encoding (8 bits, 1 = UTF-8, 2 = UTF-16, 0 = binary) + 3
// padding bytes, number of resources (16 bits), number of aliases (16 bits). Version 4: version,
// number of resources (32 bits), encoding (8 bits). Then (number of resources + 1) records of
// 6 bytes: resource id (16 bits), offset (32 bits); the last one only marks the end of the
// data. A resource ends where the next one (by offset) starts. Version 5 then holds the aliases
// (id, index of a resource); they are listed as the resource they point to.
// The resources have no name: they are called after their id, with an extension guessed from
// their first bytes.

#include <cstring>

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
struct Header
{
    uint32_t Version = 0;
    uint32_t Num = 0;
    uint32_t Aliases = 0;
    uint32_t Size = 0;
};

// Reads the header: version, number of resources, number of aliases, size of the header
auto ReadHeader(Reader &s, Header &header) -> bool
{
    uint8_t b[4];
    s.Seek(0);
    if (s.Read(&header.Version, 4) != 4)
        return false;
    header.Aliases = 0;
    if (header.Version == 5)
    {
        if (s.Read(b, 4) != 4)
            return false;
        if (b[0] > 2 || b[1] != 0 || b[2] != 0 || b[3] != 0)
            return false;
        if (s.Read(b, 4) != 4)
            return false;
        header.Num = b[0] | (b[1] << 8);
        header.Aliases = b[2] | (b[3] << 8);
        header.Size = 12;
    }
    else if (header.Version == 4)
    {
        if (s.Read(&header.Num, 4) != 4)
            return false;
        if (s.Read(b, 1) != 1)
            return false;
        if (b[0] > 2)
            return false;
        header.Size = 9;
    }
    else
        return false;
    return header.Num > 0 &&
           static_cast<int64_t>(header.Size) + (static_cast<int64_t>(header.Num) + 1) * 6 +
                   static_cast<int64_t>(header.Aliases) * 4 <=
               s.Size();
}

auto GuessExt(Reader &s, int64_t offset, int64_t size) -> std::string
{
    if (size < 2)
        return "";
    uint8_t b[16] = {};
    const size_t n = size < 16 ? static_cast<size_t>(size) : 16;
    s.Seek(offset);
    if (s.Read(b, n) != n)
        return "";
    if (n >= 8 && b[0] == 0x89 && b[1] == 0x50 && b[2] == 0x4E && b[3] == 0x47)
        return ".png";
    if (b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF)
        return ".jpg";
    if (n >= 6 && b[0] == 'G' && b[1] == 'I' && b[2] == 'F' && b[3] == '8')
        return ".gif";
    if (n >= 12 && std::memcmp(b, "RIFF", 4) == 0 && std::memcmp(b + 8, "WEBP", 4) == 0)
        return ".webp";
    if (b[0] == 0x1F && b[1] == 0x8B)
        return ".gz";
    if (n >= 4 && std::memcmp(b, "OggS", 4) == 0)
        return ".ogg";
    if (n >= 4 && std::memcmp(b, "wOFF", 4) == 0)
        return ".woff";
    if (n >= 4 && std::memcmp(b, "wOF2", 4) == 0)
        return ".woff2";
    if (n >= 4 && b[0] == 0 && b[1] == 1 && b[2] == 0 && b[3] == 0)
        return ".ttf";
    if (b[0] == '<')
    {
        const std::string t = ToLower(std::string(reinterpret_cast<const char *>(b), n));
        if (t.starts_with("<!doctype") || t.starts_with("<html"))
            return ".html";
        if (t.starts_with("<?xml"))
            return ".xml";
        if (t.starts_with("<svg"))
            return ".svg";
    }
    return "";
}

class ChromiumPakArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        Header header;
        if (!ReadHeader(stream, header))
            return false;
        // The offsets must grow and stay in the file
        uint32_t prev = 0;
        for (int64_t i = 0; i <= header.Num; ++i)
        {
            uint16_t id;
            uint32_t off;
            if (stream.Read(&id, 2) != 2 || stream.Read(&off, 4) != 4)
                return false;
            if (off > stream.Size() ||
                off < header.Size + (static_cast<int64_t>(header.Num) + 1) * 6 || (i > 0 && off < prev))
                return false;
            prev = off;
        }
        return prev <= stream.Size();
    }

    void Parse() override
    {
        Header header;
        if (!ReadHeader(*_stream, header))
            Fail(DUDI7_ERR_NOTFORMAT, "Not a data pack");
        const uint32_t num = header.Num;
        std::vector<uint16_t> ids(static_cast<size_t>(num) + 1);
        std::vector<uint32_t> offs(static_cast<size_t>(num) + 1);
        for (int64_t i = 0; i <= num; ++i)
        {
            ids[static_cast<size_t>(i)] = _stream->U16();
            offs[static_cast<size_t>(i)] = _stream->U32();
            if (offs[static_cast<size_t>(i)] > _stream->Size() ||
                (i > 0 && offs[static_cast<size_t>(i)] < offs[static_cast<size_t>(i) - 1]))
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid offset");
        }
        _formatId = "CRPAK";
        _dirSeparator = "";
        _extractInternal = false;
        for (size_t i = 0; i < num; ++i)
        {
            const int64_t size = static_cast<int64_t>(offs[i + 1]) - offs[i];
            Add(std::to_string(ids[i]) + GuessExt(*_stream, offs[i], size), offs[i], size, ids[i]);
        }
    }
};
} // namespace

void RegisterChromiumPak()
{
    Register<ChromiumPakArchive>("*.PAK", "Chromium Data Pack (*.PAK)|CrossCode, NW.js, Electron, CEF (*.PAK)");
}
} // namespace Dud
