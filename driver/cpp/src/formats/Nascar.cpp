// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Nascar Racing .DAT support (ported from drv_default.dpr of Dragon UnPACKer 5).
// No signature: number of entries (16 bits) then entries of 27 bytes (unknown
// 16 bits, size, size again, name[13], offset). The table is validated to
// recognize the format, so the extension is needed.

#include "Registry.h"

namespace Dud
{
namespace
{
class NascarArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 2)
            return false;
        const uint16_t count = stream.U16();
        const int64_t tableEnd = 2 + static_cast<int64_t>(count) * 27;
        if (count == 0 || tableEnd > stream.Size())
            return false;
        for (uint16_t i = 0; i < count; ++i)
        {
            stream.U16();
            const uint32_t size = stream.U32();
            stream.U32();
            char name[13];
            stream.ReadBuffer(name);
            const uint32_t offset = stream.U32();
            if (name[0] == '\0' || offset < tableEnd || static_cast<int64_t>(offset) + size > stream.Size())
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint16_t count = _stream->U16();
        CheckTable(2, count, 27);

        _formatId = "DAT";
        _dirSeparator = "";
        for (uint16_t i = 0; i < count; ++i)
        {
            _stream->U16();
            const uint32_t size = _stream->U32();
            _stream->U32();
            char name[13];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterNascar()
{
    Register<NascarArchive>("*.DAT", "Nascar Racing (*.DAT)");
}
} // namespace Dud
