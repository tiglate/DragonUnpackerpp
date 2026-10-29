// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Pixel Painters .XRS support (Dig It!), ported from drv_default.dpr of Dragon UnPACKer 5.
// No signature: number of entries (16 bits) then entries of 32 bytes: name length (8 bits),
// name[12], offset, length, 11 bytes (DOS date and time and unknown). The table is
// validated, so the extension is needed.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t EntrySize = 32;

class PpXrsArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint16_t count = 0;
        return stream.Read(&count, 2) == 2 && count > 0 && 2 + static_cast<int64_t>(count) * EntrySize <= stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(0);
        const int count = _stream->U16();
        CheckTable(2, count, EntrySize);
        if (count == 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Empty archive");

        _formatId = "PPXRS";
        _dirSeparator = "";
        _extractInternal = false;
        for (int i = 1; i <= count; ++i)
        {
            uint8_t ent[EntrySize];
            _stream->ReadBuffer(ent, EntrySize);
            const int nameLength = ent[0];
            if (nameLength == 0 || nameLength > 12)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid name length");
            const std::string name(reinterpret_cast<const char *>(ent) + 1, static_cast<size_t>(nameLength));
            for (const char c : name)
            {
                if (static_cast<unsigned char>(c) < ' ')
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid name");
            }
            uint32_t offset = 0, length = 0;
            std::memcpy(&offset, ent + 13, 4);
            std::memcpy(&length, ent + 17, 4);
            if (static_cast<int64_t>(offset) + length > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, length);
        }
    }
};
} // namespace

void RegisterPpXrs()
{
    Register<PpXrsArchive>("*.XRS", "Pixel Painters XRS (*.XRS)|Dig It! (*.XRS)");
}
} // namespace Dud
