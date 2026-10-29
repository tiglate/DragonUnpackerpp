// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Florensia .PAK support, ported from drv_default.dpr of Dragon UnPACKer 5. Number of entries (32 bits), then
// entries of 296 bytes (name 256, 0, offset, size, 28 unknown bytes) and the data: the files add up to exactly
// the size of the archive, which is how it is recognized (there is no signature). The Steam release of the
// game has no .PAK (loose files): v5 was written for another release.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t EntrySize = 296;
constexpr uint32_t MaxEntries = 1000000;

class FlorensiaPakArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 4 + EntrySize)
            return false;
        const int32_t count = stream.I32();
        int64_t total = static_cast<int64_t>(count) * EntrySize + 4;
        if (count <= 0 || static_cast<uint32_t>(count) > MaxEntries || total >= stream.Size())
            return false;
        for (int32_t x = 0; x < count; ++x)
        {
            stream.Seek(4 + static_cast<int64_t>(x) * EntrySize + 260);
            const int32_t offset = stream.I32();
            const int32_t size = stream.I32();
            total += size;
            if (offset < 0 || size < 0 || static_cast<int64_t>(offset) + size > stream.Size() || total > stream.Size())
                return false;
        }
        return total == stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(0);
        const int32_t count = _stream->I32();
        CheckTable(4, count, EntrySize);
        _formatId = "FLPAK";
        _dirSeparator = "";
        _extractInternal = false;
        for (int32_t x = 0; x < count; ++x)
        {
            _stream->Seek(4 + static_cast<int64_t>(x) * EntrySize);
            char name[256];
            _stream->ReadBuffer(name);
            _stream->U32();
            const int32_t offset = _stream->I32();
            const int32_t size = _stream->I32();
            if (offset < 0 || size < 0 || static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(name), offset, size);
        }
    }
};
} // namespace

void RegisterFlorensiaPak()
{
    Register<FlorensiaPakArchive>("*.PAK", "Florensia (*.PAK)");
}
} // namespace Dud
