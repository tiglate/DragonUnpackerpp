// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Spellforce .PAK support ("MASSIVE PAKFILE V 4.0"), ported from drv_default.dpr
// of Dragon UnPACKer 5.
// Header (92 bytes): version (4), the text above + CR LF (24 bytes), 44 unknown
// bytes, unknown, number of entries (at 76), unknown, data offset (at 84), file
// size. Then the entries (16 bytes: size, offset relative to the data, name
// offset, directory name offset; the upper byte of the two offsets is a flag)
// and the names: null terminated and written backwards. Names start 2 bytes
// after their offset, directories names right at theirs; offsets are relative to
// the end of the entries.

#include <algorithm>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr std::string_view MassiveId = "MASSIVE PAKFILE V 4.0\r\n";
constexpr int64_t HeaderSize = 92;

class SpellforceArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t version = 0;
        if (stream.Read(&version, 4) != 4 || version != 4)
            return false;
        char id[23];
        return stream.Read(id, 23) == 23 && ZString(id) == MassiveId;
    }

    void Parse() override
    {
        _stream->Seek(76);
        const uint32_t count = _stream->U32();
        _stream->U32();
        const uint32_t dataOffset = _stream->U32();
        CheckTable(HeaderSize, count, 16);
        std::vector<uint32_t> fat(static_cast<size_t>(count) * 4);
        _stream->Seek(HeaderSize);
        for (auto &value : fat)
            value = _stream->U32();
        const int64_t namesBase = HeaderSize + static_cast<int64_t>(count) * 16;

        _formatId = "MPF4";
        _dirSeparator = "\\";
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t size = fat[i * 4];
            const uint32_t relOffset = fat[i * 4 + 1];
            const uint32_t nameOffset = fat[i * 4 + 2] & 0xFFFFFF;
            const uint32_t dirOffset = fat[i * 4 + 3] & 0xFFFFFF;
            if (namesBase + nameOffset + 2 >= _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A name is outside of the file");
            _stream->Seek(namesBase + nameOffset + 2);
            std::string name = _stream->CString();
            std::ranges::reverse(name);
            if (namesBase + dirOffset >= _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A directory name is outside of the file");
            _stream->Seek(namesBase + dirOffset);
            std::string dir = _stream->CString();
            std::ranges::reverse(dir);
            if (static_cast<int64_t>(dataOffset) + relOffset + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(dir + "\\" + name, static_cast<int64_t>(dataOffset) + relOffset, size);
        }
    }
};
} // namespace

void RegisterSpellforce()
{
    Register<SpellforceArchive>("*.PAK", "Spellforce (*.PAK)");
}
} // namespace Dud
