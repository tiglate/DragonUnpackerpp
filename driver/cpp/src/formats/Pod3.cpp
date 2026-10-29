// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// BloodRayne .POD (POD3 and POD5) support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (288 bytes): id, unknown, 80 bytes of text, number of entries, 3 unknown, 160
// empty bytes, offset of the directory ($108), unknown, size of the names table, 3 unknown.
// Directory: entries of 20 bytes (POD3) or 28 bytes (POD5): name offset, size, offset, 2
// unknown (+ 8 bytes for POD5), followed by the names table.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
class Pod3Archive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[4];
        return stream.Read(id, 4) == 4 && Equals(id, "POD") && (id[3] == '3' || id[3] == '5');
    }

    void Parse() override
    {
        _stream->Seek(0);
        char id[4];
        _stream->ReadBuffer(id);
        _stream->Seek(88);
        const uint32_t count = _stream->U32();
        _stream->Seek(264);
        const uint32_t dirOffset = _stream->U32();
        _stream->U32();
        const uint32_t namesSize = _stream->U32();
        const uint32_t entrySize = id[3] == '3' ? 20 : 28;
        CheckTable(dirOffset, count, entrySize);
        CheckTable(static_cast<int64_t>(dirOffset) + static_cast<int64_t>(count) * entrySize, 1, namesSize);
        std::vector<uint8_t> dir(static_cast<size_t>(count) * entrySize + 1);
        std::vector<char> names(static_cast<size_t>(namesSize) + 1);
        _stream->Seek(dirOffset);
        if (count > 0)
            _stream->ReadBuffer(dir.data(), static_cast<size_t>(count) * entrySize);
        if (namesSize > 0)
            _stream->ReadBuffer(names.data(), namesSize);

        _formatId = std::string("POD") + id[3];
        _dirSeparator = "\\";
        _extractInternal = false;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint8_t *e = dir.data() + static_cast<size_t>(i) * entrySize;
            uint32_t nameOffset = 0, size = 0, offset = 0;
            std::memcpy(&nameOffset, e, 4);
            std::memcpy(&size, e + 4, 4);
            std::memcpy(&offset, e + 8, 4);
            if (nameOffset >= namesSize)
                Fail(DUDI7_ERR_NOTFORMAT, "A name is outside of the names table");
            uint32_t end = nameOffset;
            while (end < namesSize && names[end] != '\0')
                ++end;
            const std::string name(names.data() + nameOffset, end - nameOffset);
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, size);
        }
    }
};
} // namespace

void RegisterPod3()
{
    Register<Pod3Archive>("*.POD", "BloodRayne Archive (*.POD)");
}
} // namespace Dud
