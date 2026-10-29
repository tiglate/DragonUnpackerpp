// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Dungeon Keeper 2 .WAD/.SDT support ("DWFB"), ported from drv_default.dpr of
// Dragon UnPACKer 5. Header (88 bytes): "DWFB", version, 64 filler bytes, number
// of entries, name table offset, name table size, unknown.
// Entries (40 bytes): unknown, name offset, name size, offset, size (compressed),
// compression method (0 = none), uncompressed size, 12 filler bytes. Names are
// stored apart. The data of compressed files is given as it is stored (the
// method is DataX, the uncompressed size DataY).

#include "Registry.h"

namespace Dud
{
namespace
{
class DwfbArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "DWFB");
    }

    void Parse() override
    {
        _stream->Seek(72);
        const uint32_t count = _stream->U32();
        CheckTable(88, count, 40);

        _formatId = "DWFB";
        _dirSeparator = "\\";
        for (uint32_t i = 0; i < count; ++i)
        {
            _stream->Seek(88 + static_cast<int64_t>(i) * 40 + 4);
            const uint32_t nameOffset = _stream->U32();
            const uint32_t nameSize = _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            const uint32_t method = _stream->U32();
            const uint32_t uncompressed = _stream->U32();
            if (nameSize > 1024 || static_cast<int64_t>(nameOffset) + nameSize > _stream->Size() ||
                static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "An entry is outside of the file");
            _stream->Seek(nameOffset);
            std::string name(nameSize, '\0');
            if (nameSize > 0)
                _stream->ReadBuffer(name.data(), nameSize);
            Add(ZString(name.data(), nameSize), offset, size, method, uncompressed);
        }
    }
};
} // namespace

void RegisterDwfb()
{
    Register<DwfbArchive>("*.WAD;*.SDT;*.DWFB", "Dungeon Keeper 2 (*.WAD;*.SDT;*.DWFB)");
}
} // namespace Dud
