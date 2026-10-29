// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Nocturne .POD (POD2) support, ported from drv_default.dpr of Dragon UnPACKer 5.
// "POD2", unknown, 80 bytes of text, number of entries, unknown, then (at $60) the
// entries (20 bytes: name offset, size, offset, 2 unknown) followed by the names table.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t Table = 0x60;
constexpr int64_t EntrySize = 20;

class Pod2Archive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "POD2");
    }

    void Parse() override
    {
        _stream->Seek(88);
        const int32_t count = _stream->I32();
        if (count < 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        CheckTable(Table, count, EntrySize);
        const int64_t namesBase = Table + static_cast<int64_t>(count) * EntrySize;

        _formatId = "POD2";
        _dirSeparator = "\\";
        _extractInternal = false;
        for (int32_t i = 0; i < count; ++i)
        {
            _stream->Seek(Table + static_cast<int64_t>(i) * EntrySize);
            const uint32_t nameOffset = _stream->U32();
            const uint32_t size = _stream->U32();
            const uint32_t offset = _stream->U32();
            if (namesBase + nameOffset >= _stream->Size() || static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid entry");
            _stream->Seek(namesBase + nameOffset);
            Add(_stream->CString(1024), offset, size);
        }
    }
};
} // namespace

void RegisterPod2()
{
    Register<Pod2Archive>("*.POD", "Nocturne Archive (*.POD)");
}
} // namespace Dud
