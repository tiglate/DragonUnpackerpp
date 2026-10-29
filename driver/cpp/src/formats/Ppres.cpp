// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Pixel Painters .RES support: Electranoid, Fuzzy's World of Miniature Space
// Golf, Laser Light, Xatax (ported from drv_default.dpr of Dragon UnPACKer 5).
// No signature: number of entries (16 bits) then entries of 22 bytes: name
// length (8 bits), name[12], offset, length, type (1 or 2). The table is
// validated, so the extension is needed.

#include "Registry.h"

namespace Dud
{
namespace
{
class PpResArchive : public Archive
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
        const int64_t tableEnd = 2 + static_cast<int64_t>(count) * 22;
        if (count == 0 || tableEnd > stream.Size())
            return false;
        for (uint16_t i = 0; i < count; ++i)
        {
            const uint8_t nameLength = stream.U8();
            char name[12];
            stream.ReadBuffer(name);
            const uint32_t offset = stream.U32();
            const uint32_t length = stream.U32();
            const uint8_t type = stream.U8();
            if (nameLength == 0 || nameLength > 12 || (type != 1 && type != 2) || offset < tableEnd ||
                static_cast<int64_t>(offset) + length > stream.Size())
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint16_t count = _stream->U16();
        CheckTable(2, count, 22);

        _formatId = "PPRES";
        _dirSeparator = "";
        for (uint16_t i = 0; i < count; ++i)
        {
            uint8_t nameLength = _stream->U8();
            char name[12];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            const uint32_t length = _stream->U32();
            const uint8_t type = _stream->U8();
            if (nameLength > 12)
                nameLength = 12;
            Add(ZString(name).substr(0, nameLength), offset, length, type);
        }
    }
};
} // namespace

void RegisterPpres()
{
    Register<PpResArchive>("*.RES", "Electranoid (*.RES)|Fuzzy's World of Miniature Space Golf (*.RES)|Laser Light "
                                    "(*.RES)|Xatax (*.RES)");
}
} // namespace Dud
