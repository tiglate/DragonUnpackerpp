// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Rage of Mages 2 .RES support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (24 bytes): "&YA1", index of the first root entry, number of root entries, unknown, offset of the
// table of entries, size of the table. Entries (32 bytes): unknown, offset, size, flags, name[16]. A folder
// (flags 1) has as offset the index of its first entry in the table and as size the number of its entries.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int MaxDepth = 64;
constexpr size_t MaxEntries = 1000000;
constexpr int64_t EntrySize = 32;

class Rom2ResArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "&YA1") && stream.Size() >= 24;
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t first = _stream->U32();
        const uint32_t count = _stream->U32();
        _stream->U32();
        _dirOffset = _stream->U32();
        _formatId = "RoM2RES";
        _dirSeparator = "\\";
        _extractInternal = true;
        ReadFolder("", first, count, 0);
    }

  private:
    void ReadFolder(const std::string &curDir, uint32_t first, uint32_t count, int depth)
    {
        if (depth > MaxDepth)
            Fail(DUDI7_ERR_NOTFORMAT, "Folders nested too deep");
        for (uint32_t x = 0; x < count; ++x)
        {
            const int64_t at = static_cast<int64_t>(_dirOffset) + (static_cast<int64_t>(first) + x) * EntrySize;
            if (at + EntrySize > _stream->Size() || _entries.size() >= MaxEntries)
                Fail(DUDI7_ERR_NOTFORMAT, "An entry is outside of the archive");
            _stream->Seek(at);
            const uint32_t unknown = _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            const uint32_t flags = _stream->U32();
            char raw[16];
            _stream->ReadBuffer(raw);
            const std::string name = ZString(raw);
            if (flags == 1)
                ReadFolder(curDir + name + "\\", offset, size, depth + 1);
            else
            {
                if (static_cast<int64_t>(offset) + size > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                Add(curDir + name, offset, size, unknown, flags);
            }
        }
    }

    int64_t _dirOffset = 0;
};
} // namespace

void RegisterRom2Res()
{
    Register<Rom2ResArchive>("*.RES", "Rage of Mages 2 (*.RES)");
}
} // namespace Dud
