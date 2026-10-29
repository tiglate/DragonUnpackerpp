// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// South Park: The Stick of Truth .OAF support, ported from drv_default.dpr of
// Dragon UnPACKer 5.
// "OAF!", 2 unknown integers, offset of the names block (64 bits), number of entries,
// then entries (20 bytes: hash, offset (64 bits), size (64 bits)). The names block
// holds the null terminated names, in the entries order, up to the end of the file.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t HeaderSize = 24;
constexpr int64_t EntrySize = 20;

class OafArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, "OAF!");
    }

    void Parse() override
    {
        _stream->Seek(12);
        int64_t nameBlock = 0;
        _stream->ReadBuffer(&nameBlock, 8);
        const uint32_t count = _stream->U32();
        if (nameBlock < 0 || nameBlock > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid names block offset");
        CheckTable(HeaderSize, count, EntrySize);
        std::vector<int64_t> offsets(count);
        std::vector<int64_t> sizes(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            uint8_t buffer[EntrySize];
            _stream->ReadBuffer(buffer, EntrySize);
            std::memcpy(&offsets[i], buffer + 4, 8);
            std::memcpy(&sizes[i], buffer + 12, 8);
        }

        _formatId = "OAF";
        _dirSeparator = "\\";
        _extractInternal = false;
        _stream->Seek(nameBlock);
        for (uint32_t i = 0; i < count; ++i)
        {
            const std::string name = _stream->CString(1024);
            if (offsets[i] < 0 || sizes[i] < 0 || offsets[i] + sizes[i] > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offsets[i], sizes[i]);
        }
    }
};
} // namespace

void RegisterOaf()
{
    Register<OafArchive>("*.OAF", "South Park: The Stick of Truth Archive (*.OAF)");
}
} // namespace Dud
