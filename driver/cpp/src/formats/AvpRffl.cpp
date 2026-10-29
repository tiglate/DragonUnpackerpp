// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Aliens vs Predator .RFF support ("RFFL"), ported from drv_default.dpr of Dragon
// UnPACKer 5. Header (20 bytes): "RFFL", version, number of entries, directory
// size, data size. Entries: offset, size, null terminated name padded to 4 bytes.
// (The "CFFL" common.ffl variant is not supported.)

#include "Registry.h"

namespace Dud
{
namespace
{
class AvpRfflArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "RFFL") && stream.Size() >= 20;
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t count = _stream->U32();
        CheckTable(20, count, 9);

        _formatId = "RFFL";
        _dirSeparator = "\\";
        _stream->Seek(20);
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            const std::string name = _stream->CString();
            // The name (with its null) is padded to a multiple of 4 bytes
            _stream->Skip((4 - ((name.size() + 1) % 4)) % 4);
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, size);
        }
    }
};
} // namespace

void RegisterAvpRffl()
{
    Register<AvpRfflArchive>("*.RFF;*.FFL", "Aliens vs. Predator (*.RFF;*.FFL)");
}
} // namespace Dud
