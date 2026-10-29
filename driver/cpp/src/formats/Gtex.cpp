// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// IO Interactive / Glacier texture packs (.TEX: Hitman 2, Hitman: Contracts, Freedom Fighters...), ported from
// drv_default.dpr of Dragon UnPACKer 5.
// Header (16 bytes): offset of the index, offset of a second table, 3 and 4 (they can be 0 in an empty file).
// The index is a table of 32 bits offsets (8 or 16 KiB, so 2048 or 4096 slots, the empty ones are 0). Each offset
// points to an entry: size of the data, 4 bytes of type (written backwards), 4 bytes of another type, 2 integers,
// 2 words, 4 integers (36 bytes in all) then the name (null terminated, '/' for folders). The file of the entry
// is exposed from its header on, named "<name>.<type>". Only files with the extension .TEX are tried.

#include <algorithm>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t EntrySize = 36;

class GtexArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t h[4];
        if (stream.Read(h, 16) != 16)
            return false;
        if ((h[2] != 3 && h[2] != 0) || (h[3] != 4 && h[3] != 0))
            return false;
        const int64_t total = stream.Size();
        if (h[0] < 16 || h[0] >= total || h[1] >= total)
            return false;
        const int64_t span = static_cast<int64_t>(h[1]) - h[0];
        return span == 0x2000 || span == 0x4000;
    }

    void Parse() override
    {
        _stream->Seek(0);
        uint32_t h[4];
        _stream->ReadBuffer(h, 16);
        const int64_t span = static_cast<int64_t>(h[1]) - h[0];
        if (span != 0x2000 && span != 0x4000)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid index");
        std::vector<uint32_t> slots(static_cast<size_t>(span / 4));
        _stream->Seek(h[0]);
        _stream->ReadBuffer(slots.data(), slots.size() * 4);
        _formatId = "GTEX";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (const uint32_t offset : slots)
        {
            if (offset == 0)
                continue;
            if (static_cast<int64_t>(offset) + EntrySize > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "An entry is outside of the file");
            _stream->Seek(offset);
            uint32_t size;
            char type[4];
            _stream->ReadBuffer(&size, 4);
            _stream->ReadBuffer(type);
            _stream->Skip(EntrySize - 8);
            std::string name;
            for (;;)
            {
                char c;
                _stream->ReadBuffer(&c, 1);
                if (c == '\0')
                    break;
                name += c == '/' ? '\\' : c;
                if (name.size() > 1024)
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid name");
            }
            std::string ext = ZString(type);
            std::reverse(ext.begin(), ext.end());
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name + "." + ext, offset, size);
        }
    }
};
} // namespace

void RegisterGtex()
{
    Register<GtexArchive>("*.TEX", "Glacier Texture Pack (*.TEX)|Hitman 2, Hitman: Contracts, Freedom Fighters (*.TEX)");
}
} // namespace Dud
