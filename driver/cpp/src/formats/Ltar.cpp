// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// F.E.A.R. .ARCH00 (LTAR) support, ported from drv_default.dpr of Dragon UnPACKer 5.
// 48 bytes header: "LTAR", version, size of the names chunk, number of folders, number
// of entries, 7 unknown integers. Then the names chunk, the entries (32 bytes: name
// position, offset (64 bits), size (64 bits), size2 (64 bits), unknown) and the
// folders (16 bytes: name position, 2 unknown, number of entries). Entries are listed
// folder after folder.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t HeaderSize = 48;
constexpr int64_t EntrySize = 32;
constexpr int64_t DirSize = 16;

class LtarArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "LTAR");
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t nameSize = _stream->U32();
        const uint32_t dirCount = _stream->U32();
        const uint32_t count = _stream->U32();
        CheckTable(HeaderSize, 1, nameSize);
        CheckTable(HeaderSize + static_cast<int64_t>(nameSize), count, EntrySize);
        CheckTable(HeaderSize + static_cast<int64_t>(nameSize) + static_cast<int64_t>(count) * EntrySize, dirCount,
                   DirSize);
        std::vector<char> names(static_cast<size_t>(nameSize) + 1, '\0');
        _stream->Seek(HeaderSize);
        if (nameSize > 0)
            _stream->ReadBuffer(names.data(), nameSize);
        std::vector<uint32_t> dirPos(dirCount);
        std::vector<uint32_t> dirCounts(dirCount);
        _stream->Seek(HeaderSize + static_cast<int64_t>(nameSize) + static_cast<int64_t>(count) * EntrySize);
        for (uint32_t i = 0; i < dirCount; ++i)
        {
            dirPos[i] = _stream->U32();
            _stream->U32();
            _stream->U32();
            dirCounts[i] = _stream->U32();
        }

        const auto nameAt = [&](uint32_t pos) -> std::string {
            if (pos >= nameSize)
                Fail(DUDI7_ERR_NOTFORMAT, "A name is outside of the names chunk");
            uint32_t end = pos;
            while (end < nameSize && names[end] != '\0')
                ++end;
            return std::string(names.data() + pos, end - pos);
        };

        _formatId = "LTAR";
        _dirSeparator = "\\";
        _extractInternal = false;
        _stream->Seek(HeaderSize + static_cast<int64_t>(nameSize));
        int64_t current = -1;
        uint32_t left = 0;
        std::string dirName;
        for (uint32_t i = 1; i <= count; ++i)
        {
            // Some folders announce no entries: skip them
            while (left == 0)
            {
                ++current;
                if (current >= static_cast<int64_t>(dirCount))
                    Fail(DUDI7_ERR_NOTFORMAT, "More entries than the folders announce");
                left = dirCounts[static_cast<size_t>(current)];
                dirName = nameAt(dirPos[static_cast<size_t>(current)]);
                if (!dirName.empty())
                    dirName += "\\";
            }
            uint8_t ent[EntrySize];
            _stream->ReadBuffer(ent, EntrySize);
            uint32_t namePos = 0;
            int64_t offset = 0;
            int64_t size = 0;
            std::memcpy(&namePos, ent, 4);
            std::memcpy(&offset, ent + 4, 8);
            std::memcpy(&size, ent + 12, 8);
            if (offset < 0 || size < 0 || offset + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(dirName + nameAt(namePos), offset, size);
            --left;
        }
    }
};
} // namespace

void RegisterLtar()
{
    Register<LtarArchive>("*.ARCH00", "F.E.A.R. Archive (*.ARCH00)");
}
} // namespace Dud
