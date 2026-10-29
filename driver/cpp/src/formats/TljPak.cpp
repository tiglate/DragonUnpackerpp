// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Dreamfall: The Longest Journey .PAK support ("tlj_pack0001"), ported from
// drv_default.dpr of Dragon UnPACKer 5.
// Header (24 bytes): the text above (12 bytes), number of entries, 2 unknown
// values. Then the entries (20 bytes: offset, size, 3 unknown values). Files have
// no name: they are numbered after the archive and the extension is guessed from
// the first 4 bytes of the data. Entries with a size of 0 are ignored.

#include <cstdio>

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
auto GuessExtension(uint32_t magic) -> std::string
{
    switch (magic)
    {
    case 0x20534444:
        return ".dds";
    case 0x73760A0D:
    case 0x312E7376:
    case 0x31157376:
    case 0x315F7376:
        return ".vs1";
    case 0x322E7376:
    case 0x6E69203B:
        return ".vs2";
    case 0x0A0D2F2F:
        return ".vsc";
    case 0x312E7370:
    case 0x696C203B:
    case 0x6576203B:
        return ".ps1";
    case 0x322E7370:
    case 0x6170203B:
        return ".ps2";
    case 0x615F7376:
    case 0x332E7370:
        return ".ps3";
    case 0x5367674F:
        return ".ogg";
    case 0x626A6C74:
        return ".tljbone2d";
    case 0x55465453:
        return ".stfu";
    case 0x72616873:
        return ".shark3d";
    case 0x46464952:
        return ".wav";
    default:
        break;
    }
    if ((magic & 0xE0FF) == 0xE0FF)
        return ".mp3";
    char hex[16];
    std::snprintf(hex, sizeof hex, "%08X", magic);
    return std::string(".unk") + hex;
}

class TljPakArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "tlj_pack0001");
    }

    void Parse() override
    {
        _stream->Seek(12);
        const uint32_t count = _stream->U32();
        CheckTable(24, count, 20);
        // Names are made from the file name of the archive: file_pak -> file_pak-001.dds
        std::string prefix = FileNameOf(_stream->Path());
        const std::string ext = ExtensionOf(prefix);
        prefix = ext.size() > 1 ? ChangeFileExt(prefix, "_" + ext.substr(1)) : ChangeFileExt(prefix, "");
        const size_t digits = std::to_string(count).size();

        _formatId = "TLJPAK";
        _dirSeparator = "/";
        for (uint32_t i = 1; i <= count; ++i)
        {
            _stream->Seek(24 + static_cast<int64_t>(i - 1) * 20);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            if (size == 0)
                continue;
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            _stream->Seek(offset);
            uint32_t magic = 0;
            _stream->Read(&magic, 4);
            std::string number = std::to_string(i);
            if (number.size() < digits)
                number.insert(0, digits - number.size(), '0');
            Add(prefix + "-" + number + GuessExtension(magic), offset, size);
        }
    }
};
} // namespace

void RegisterTljPak()
{
    Register<TljPakArchive>("*.PAK", "Dreamfall: The Longest Journey (*.PAK)");
}
} // namespace Dud
