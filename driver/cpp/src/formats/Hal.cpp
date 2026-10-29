// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Mortyr .HAL support (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header (32 bytes): "APUK", number of entries, 24 unknown bytes.
// Entries (32 bytes): size, offset, 2 unknown values, name[16].

#include "Registry.h"

namespace Dud
{
namespace
{
class HalArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "APUK");
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t count = _stream->U32();
        CheckTable(32, count, 32);

        _formatId = "HAL";
        _dirSeparator = "";
        _stream->Seek(32);
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t size = _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t unknown1 = _stream->U32();
            const uint32_t unknown2 = _stream->U32();
            char name[16];
            _stream->ReadBuffer(name);
            Add(ZString(name), offset, size, unknown1, unknown2);
        }
    }
};
} // namespace

void RegisterHal()
{
    Register<HalArchive>("*.HAL", "Mortyr (*.HAL)");
}
} // namespace Dud
