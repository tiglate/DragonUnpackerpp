// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Breakneck / Excessive Speed / N.I.C.E.2 .SYN support (ported from
// drv_default.dpr of Dragon UnPACKer 5).
// Header (16 bytes): "FNYS", number of entries, 8 reserved bytes.
// Entries (32 bytes): name[24], offset, size.

#include "Registry.h"

namespace Dud
{
namespace
{
class SynArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "FNYS");
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t count = _stream->U32();
        CheckTable(16, count, 32);

        _formatId = "SYN";
        _dirSeparator = "";
        _stream->Seek(16);
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[24];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterSyn()
{
    Register<SynArchive>("*.SYN", "Breakneck (*.SYN)|Excessive Speed (*.SYN)|N.I.C.E.2 (*.SYN)");
}
} // namespace Dud
