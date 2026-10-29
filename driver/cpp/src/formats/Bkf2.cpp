// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Moto Racer 2 .BKF support. Not in Dragon UnPACKer 5: worked out from the files of the GOG release (every
// .BKF of the game: the first data offset is the end of the table and the entries cover the file up to its end).
// Header (12 bytes): -2, number of entries, size of a name (48). Entries (name 48 bytes, offset, size, 8 bytes of
// a checksum or a time stamp), folders written with '\'. The first Moto Racer used another layout (see Bkf).

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 200000;

class Bkf2Archive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 12)
            return false;
        const int32_t version = stream.I32();
        const uint32_t count = stream.U32();
        const uint32_t nameSize = stream.U32();
        if (version != -2 || count == 0 || count > MaxEntries || nameSize < 8 || nameSize > 255)
            return false;
        const int64_t tableEnd = 12 + static_cast<int64_t>(count) * (nameSize + 16);
        if (tableEnd > stream.Size())
            return false;
        // the last entry decides: it has to be inside of the file, after the table
        stream.Seek(tableEnd - 16);
        const uint32_t offset = stream.U32();
        const uint32_t size = stream.U32();
        return (size == 0 || offset >= tableEnd) && static_cast<int64_t>(offset) + size <= stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t count = _stream->U32();
        const uint32_t nameSize = _stream->U32();
        const int64_t tableEnd = 12 + static_cast<int64_t>(count) * (nameSize + 16);
        if (count == 0 || count > MaxEntries || nameSize < 8 || nameSize > 255 || tableEnd > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");
        _formatId = "BKF2";
        _dirSeparator = "\\";
        _extractInternal = false;
        std::vector<char> name(nameSize);
        for (uint32_t i = 0; i < count; ++i)
        {
            _stream->Seek(12 + static_cast<int64_t>(i) * (nameSize + 16));
            _stream->ReadBuffer(name.data(), name.size());
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(name.data(), name.size()), offset, size);
        }
    }
};
} // namespace

void RegisterBkf2()
{
    Register<Bkf2Archive>("*.BKF", "Moto Racer 2 (*.BKF)");
}
} // namespace Dud
