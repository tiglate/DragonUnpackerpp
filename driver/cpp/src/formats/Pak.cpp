// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Quake .PAK support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header: "PACK", directory offset, directory size (all 32 bits little-endian).
// Entries are 64 bytes (name[56], offset, size), or 72 bytes in some derived games.

#include "Registry.h"

namespace Dud
{
namespace
{
class PakArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "PACK");
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t dirOffset = _stream->U32();
        const uint32_t dirSize = _stream->U32();
        uint32_t entrySize = 0;
        if (dirSize % 64 == 0)
            entrySize = 64;
        else if (dirSize % 72 == 0)
            entrySize = 72;
        else
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid PAK directory size");
        const uint32_t count = dirSize / entrySize;
        CheckTable(dirOffset, count, entrySize);

        _formatId = "PACK";
        _dirSeparator = "/";
        _stream->Seek(dirOffset);
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[56];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            if (entrySize > 64)
                _stream->Skip(entrySize - 64);
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterPak()
{
    Register<PakArchive>("*.PAK", "Quake (*.PAK)|Quake 2 (*.PAK)|Half-Life (*.PAK)|Trickstyle (*.PAK)");
}
} // namespace Dud
