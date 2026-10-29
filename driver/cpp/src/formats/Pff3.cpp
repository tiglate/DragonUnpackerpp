// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Novalogic .PFF support ("PFF3"): Comanche 4, Delta Force, ..
// (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (20 bytes): data offset, "PFF3", number of entries, size of an entry
// in the directory (32 or more), directory offset.
// Entries: unknown, offset, size, unknown, name[16].

#include "Registry.h"

namespace Dud
{
namespace
{
class Pff3Archive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(4);
        char id[4];
        return stream.Read(id, 4) == 4 && Equals(id, "PFF3");
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t count = _stream->U32();
        const uint32_t entrySize = _stream->U32();
        const uint32_t dirOffset = _stream->U32();
        if (entrySize < 32 || entrySize > 4096)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid PFF directory entry size");
        CheckTable(dirOffset, count, entrySize);

        _formatId = "PFF3";
        _dirSeparator = "";
        for (uint32_t i = 0; i < count; ++i)
        {
            _stream->Seek(static_cast<int64_t>(dirOffset) + static_cast<int64_t>(i) * entrySize + 4);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            _stream->U32();
            char name[16];
            _stream->ReadBuffer(name);
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterPff3()
{
    Register<Pff3Archive>(
        "*.PFF", "Comanche 4 (*.PFF)|Delta Force (*.PFF)|Delta Force 2 (*.PFF)|Delta Force: Land Warrior (*.PFF)");
}
} // namespace Dud
