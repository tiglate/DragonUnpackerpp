// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Dark Forces .GOB support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header: "GOB\n", directory offset. Directory: number of entries, then
// entries of 21 bytes (offset, size, name[13]).

#include "Registry.h"

namespace Dud
{
namespace
{
class GobArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "GOB\n");
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t dirOffset = _stream->U32();
        CheckTable(dirOffset, 1, 4);
        _stream->Seek(dirOffset);
        const uint32_t count = _stream->U32();
        CheckTable(static_cast<int64_t>(dirOffset) + 4, count, 21);

        _formatId = "DGOB";
        _dirSeparator = "";
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            char name[13];
            _stream->ReadBuffer(name);
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterGob()
{
    Register<GobArchive>("*.GOB", "Dark Forces (*.GOB)|Indiana Jones 3D (*.GOB)|Jedi Knight: Dark Forces 2 (*.GOB)");
}
} // namespace Dud
