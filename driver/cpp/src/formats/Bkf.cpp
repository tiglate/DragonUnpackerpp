// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Moto Racer .BKF support (ported from drv_default.dpr of Dragon UnPACKer 5).
// No signature: number of entries (32 bits) then entries of 44 bytes
// (name[36], offset, size). The whole table is validated to recognize it.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 200000;

class BkfArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 4)
            return false;
        const uint32_t count = stream.U32();
        const int64_t tableEnd = 4 + static_cast<int64_t>(count) * 44;
        if (count == 0 || count > MaxEntries || tableEnd > stream.Size())
            return false;
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[36];
            stream.ReadBuffer(name);
            const uint32_t offset = stream.U32();
            const uint32_t size = stream.U32();
            if (offset < tableEnd || static_cast<int64_t>(offset) + size > stream.Size() || name[0] == '\0')
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint32_t count = _stream->U32();
        CheckTable(4, count, 44);

        _formatId = "BKF";
        _dirSeparator = "\\";
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[36];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterBkf()
{
    Register<BkfArchive>("*.BKF", "Moto Racer (*.BKF)");
}
} // namespace Dud
