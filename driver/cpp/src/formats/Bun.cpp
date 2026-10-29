// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Monkey Island 3 .BUN support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (16 bytes, big-endian values): "LB83", directory offset, number of
// entries, creation date. Entries (20 bytes): name[8], extension[4], offset, size
// (big-endian).

#include "Registry.h"

namespace Dud
{
namespace
{
class BunArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "LB83");
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t dirOffset = _stream->BE32();
        const uint32_t count = _stream->BE32();
        CheckTable(dirOffset, count, 20);

        _formatId = "BUN";
        _dirSeparator = "";
        _stream->Seek(dirOffset);
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[8];
            char ext[4];
            _stream->ReadBuffer(name);
            _stream->ReadBuffer(ext);
            const uint32_t offset = _stream->BE32();
            const uint32_t size = _stream->BE32();
            Add(ZString(name) + "." + ZString(ext), offset, size);
        }
    }
};
} // namespace

void RegisterBun()
{
    Register<BunArchive>("*.BUN", "Monkey Island 3 (*.BUN)");
}
} // namespace Dud
