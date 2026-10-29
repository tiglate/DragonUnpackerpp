// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Doom .WAD support (IWAD and PWAD): Doom, Doom 2, Heretic, Hexen, ..
// Header: "IWAD" or "PWAD", number of lumps, directory offset.
// Lumps are 16 bytes: offset, size, name[8]. Lump names are not unique.

#include "Registry.h"

namespace Dud
{
namespace
{
class DoomWadArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[4];
        return stream.Read(id, 4) == 4 && (Equals(id, "IWAD") || Equals(id, "PWAD"));
    }

    void Parse() override
    {
        _stream->Seek(0);
        char id[4];
        _stream->ReadBuffer(id);
        const uint32_t count = _stream->U32();
        const uint32_t dirOffset = _stream->U32();
        CheckTable(dirOffset, count, 16);

        _formatId = ZString(id);
        _dirSeparator = "";
        _stream->Seek(dirOffset);
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            char name[8];
            _stream->ReadBuffer(name);
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterDoomWad()
{
    Register<DoomWadArchive>("*.WAD", "Doom (*.WAD)|Doom 2 (*.WAD)|Heretic (*.WAD)|Hexen (*.WAD)");
}
} // namespace Dud
