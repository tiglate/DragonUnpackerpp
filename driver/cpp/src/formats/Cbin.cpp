// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// CyberBykes / Shadow Racer VR .BIN support (ported from drv_default.dpr of
// Dragon UnPACKer 5). No signature: number of entries (32 bits) then entries of
// 24 bytes (name[16], offset, size). Each file starts with a 32 bits value
// (DataX) before its data. The table is validated, so the extension is needed.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 100000;

class CBinArchive : public Archive
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
        const int64_t tableEnd = 4 + static_cast<int64_t>(count) * 24;
        if (count == 0 || count > MaxEntries || tableEnd > stream.Size())
            return false;
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[16];
            stream.ReadBuffer(name);
            const uint32_t offset = stream.U32();
            const uint32_t size = stream.U32();
            if (name[0] == '\0' || offset < tableEnd || size < 4 || static_cast<int64_t>(offset) + size > stream.Size())
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint32_t count = _stream->U32();
        CheckTable(4, count, 24);

        _formatId = "CBIN";
        _dirSeparator = "";
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[16];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            if (size < 4 || static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            const int64_t position = _stream->Pos();
            _stream->Seek(offset);
            const uint32_t pre = _stream->U32();
            _stream->Seek(position);
            Add(ZString(name), static_cast<int64_t>(offset) + 4, static_cast<int64_t>(size) - 4, pre);
        }
    }
};
} // namespace

void RegisterCbin()
{
    Register<CBinArchive>("*.BIN", "CyberBykes: Shadow Racer VR (*.BIN)");
}
} // namespace Dud
