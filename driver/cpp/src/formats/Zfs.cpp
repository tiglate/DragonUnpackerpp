// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Interstate 76 / Interstate 82 .ZFS support ("ZFSF" or "ZFS3"), ported from
// drv_default.dpr of Dragon UnPACKer 5.
// Header (32 bytes): signature, version, unknown, entries per directory block,
// total number of entries, 2 unknown values, offset of the next directory block.
// Entries are 36 bytes (name[16], offset, unknown, size, 2 unknown values). After
// each block of entries comes a jump to the next block, which starts with the
// offset of the one after it.

#include "Registry.h"

namespace Dud
{
namespace
{
class ZfsArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[4];
        return stream.Read(id, 4) == 4 && (Equals(id, "ZFSF") || Equals(id, "ZFS3")) && stream.Size() >= 32;
    }

    void Parse() override
    {
        _stream->Seek(12);
        const uint32_t blockSize = _stream->U32();
        const uint32_t count = _stream->U32();
        _stream->Seek(28);
        uint32_t next = _stream->U32();
        if (blockSize == 0 || count > 10000000)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid ZFS header");
        CheckTable(32, 1, 36);

        _formatId = "ZFS";
        _dirSeparator = "";
        uint32_t inBlock = 0;
        for (uint32_t i = 1; i <= count; ++i)
        {
            char name[16];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            _stream->U32();
            const uint32_t size = _stream->U32();
            _stream->U32();
            _stream->U32();
            ++inBlock;
            if (inBlock == blockSize)
            {
                // The directory goes on at another place (not needed after the last entry)
                inBlock = 0;
                if (i < count)
                {
                    if (static_cast<int64_t>(next) + 4 > _stream->Size())
                        Fail(DUDI7_ERR_NOTFORMAT, "The next directory block is outside of the file");
                    _stream->Seek(next);
                    next = _stream->U32();
                }
            }
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterZfs()
{
    Register<ZfsArchive>("*.ZFS", "Interstate '76 (*.ZFS)|Interstate '82 (*.ZFS)");
}
} // namespace Dud
