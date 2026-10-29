// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Age of Empires 2 .DRS support (ported from drv_default.dpr of Dragon
// UnPACKer 5, which only read the first table).
// Header (64 bytes): the copyright text "Copyright (c) 1997 Ensemble Studios."
// (36 bytes) + 4 bytes, version, file type, number of tables (at 56) and
// offset of the first file. Then 12 bytes per table (type, offset, number of
// files); each table lists files as (id, offset, size).
// Files have no name: the id is used, the extension is guessed from the data.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr std::string_view DrsId = "Copyright (c) 1997 Ensemble Studios.";

class DrsArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, DrsId);
    }

    void Parse() override
    {
        _stream->Seek(56);
        const uint32_t tableCount = _stream->U32();
        _stream->U32();
        CheckTable(64, tableCount, 12);
        std::vector<uint32_t> tableOffsets(tableCount);
        std::vector<uint32_t> tableCounts(tableCount);
        for (uint32_t t = 0; t < tableCount; ++t)
        {
            _stream->U32();
            tableOffsets[t] = _stream->U32();
            tableCounts[t] = _stream->U32();
            CheckTable(tableOffsets[t], tableCounts[t], 12);
        }

        _formatId = "DRS";
        _dirSeparator = "";
        for (uint32_t t = 0; t < tableCount; ++t)
        {
            for (int64_t i = 0; i < tableCounts[t]; ++i)
            {
                _stream->Seek(static_cast<int64_t>(tableOffsets[t]) + i * 12);
                const uint32_t fileId = _stream->U32();
                const uint32_t offset = _stream->U32();
                const uint32_t size = _stream->U32();
                if (static_cast<int64_t>(offset) + size > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "DRS entry is outside of the file");
                std::string ext = ".txt";
                char magic[4] = {};
                _stream->Seek(offset);
                if (_stream->Read(magic, 4) == 4)
                {
                    if (Equals(magic, "2.0N"))
                    {
                        ext = ".slp";
                    }
                    else if (Equals(magic, "RIFF"))
                    {
                        ext = ".raw";
                        _stream->Seek(static_cast<int64_t>(offset) + 8);
                        if (_stream->Read(magic, 4) == 4 && Equals(magic, "WAVE"))
                            ext = ".wav";
                    }
                }
                std::string id = std::to_string(fileId);
                id.insert(0, 10 - id.size(), '0');
                Add(id + ext, offset, size);
            }
        }
    }
};
} // namespace

void RegisterDrs()
{
    Register<DrsArchive>("*.DRS", "Age of Empires 2: Age of Kings (*.DRS)");
}
} // namespace Dud
