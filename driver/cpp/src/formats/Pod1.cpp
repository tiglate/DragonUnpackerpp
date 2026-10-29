// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Terminal Velocity .POD support (ported from drv_default.dpr of Dragon
// UnPACKer 5). No signature: number of entries (32 bits), 80 bytes of text,
// then entries of 40 bytes (name[32], size, offset). The table is validated to
// recognize the format, so the extension is needed.
// The other POD flavors (BloodRayne, Nocturne, ..) are in their own files.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 100000;

class Pod1Archive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 84)
            return false;
        const uint32_t count = stream.U32();
        const int64_t tableEnd = 84 + static_cast<int64_t>(count) * 40;
        if (count == 0 || count > MaxEntries || tableEnd > stream.Size())
            return false;
        stream.Seek(84);
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[32];
            stream.ReadBuffer(name);
            const uint32_t size = stream.U32();
            const uint32_t offset = stream.U32();
            if (offset < tableEnd || static_cast<int64_t>(offset) + size > stream.Size() || name[0] == '\0')
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint32_t count = _stream->U32();
        CheckTable(84, count, 40);

        _formatId = "POD";
        _dirSeparator = "\\";
        _stream->Seek(84);
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[32];
            _stream->ReadBuffer(name);
            const uint32_t size = _stream->U32();
            const uint32_t offset = _stream->U32();
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterPod1()
{
    Register<Pod1Archive>("*.POD", "Terminal Velocity (*.POD)");
}
} // namespace Dud
