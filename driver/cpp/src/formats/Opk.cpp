// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Sinking Island .OPK support, ported from drv_default.dpr of Dragon UnPACKer 5.
// "PAK ", number of entries, directory size, unknown; then for each entry: 24 bytes
// (offset, size, size of the record without name, size of the record, FILETIME), the
// null terminated name and padding up to the record size.

#include "Registry.h"

namespace Dud
{
namespace
{
class OpkArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "PAK ");
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t count = _stream->U32();
        if (count > static_cast<uint64_t>(_stream->Size()) / 25)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");

        _formatId = "OPK";
        _dirSeparator = "\\";
        _extractInternal = false;
        _stream->Seek(16);
        for (uint32_t i = 0; i < count; ++i)
        {
            const int64_t start = _stream->Pos();
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            _stream->U32();
            const uint32_t recordSize = _stream->U32();
            _stream->Seek(start + 24);
            const std::string name = _stream->CString(1024);
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, size);
            if (start + recordSize > _stream->Pos())
                _stream->Seek(start + recordSize);
        }
    }
};
} // namespace

void RegisterOpk()
{
    Register<OpkArchive>("*.OPK", "Sinking Island Archive (*.OPK)");
}
} // namespace Dud
