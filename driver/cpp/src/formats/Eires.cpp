// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Evil Islands .RES support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header: id $019CE23C, number of entries, directory offset, size of the names table.
// The names table is at the end of the file. Entries (22 bytes): unknown, size, offset,
// unknown, name length (16 bits), name offset in the names table.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t Id = 0x019CE23C;
constexpr uint32_t EntrySize = 22;

class EiResArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t id = 0;
        return stream.Read(&id, 4) == 4 && id == Id;
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t count = _stream->U32();
        const uint32_t dirOffset = _stream->U32();
        const uint32_t namesSize = _stream->U32();
        if (namesSize > _stream->Size() || dirOffset > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");
        CheckTable(dirOffset, count, EntrySize);
        std::vector<char> names(namesSize);
        if (namesSize > 0)
        {
            _stream->Seek(_stream->Size() - namesSize);
            _stream->ReadBuffer(names.data(), names.size());
        }

        _formatId = "EIRES";
        _dirSeparator = "\\";
        _extractInternal = false;
        _stream->Seek(dirOffset);
        for (uint32_t i = 0; i < count; ++i)
        {
            _stream->U32();
            const uint32_t size = _stream->U32();
            const uint32_t offset = _stream->U32();
            _stream->U32();
            const uint16_t nameLength = _stream->U16();
            const uint32_t nameOffset = _stream->U32();
            if (static_cast<int64_t>(nameOffset) + nameLength > namesSize)
                Fail(DUDI7_ERR_NOTFORMAT, "A name is outside of the names table");
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(nameLength > 0 ? ZString(names.data() + nameOffset, nameLength) : "", offset, size);
        }
    }
};
} // namespace

void RegisterEires()
{
    Register<EiResArchive>("*.RES", "Evil Islands Resource (*.RES)");
}
} // namespace Dud
