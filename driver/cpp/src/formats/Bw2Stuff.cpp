// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Black & White 2 .STUFF support, ported from drv_default.dpr of Dragon UnPACKer 5.
// No signature: the last 32 bits of the file are the offset of the directory, which is made
// of entries of 268 bytes (name[256], offset, size, date) up to that last integer. Only
// tried on .STUFF files; the directory is validated.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t EntrySize = 268;

// Where the directory starts, -1 if the end of the file does not look like it
auto DirStart(Reader &stream) -> int64_t
{
    if (stream.Size() < 4 + EntrySize)
        return -1;
    stream.Seek(stream.Size() - 4);
    uint32_t offset = 0;
    if (stream.Read(&offset, 4) != 4)
        return -1;
    if (offset == 0 || offset >= stream.Size() - 4 || (stream.Size() - 4 - offset) % EntrySize != 0)
        return -1;
    return offset;
}

class Bw2StuffArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        const int64_t start = DirStart(stream);
        if (start < 0)
            return false;
        // first name: printable, null terminated
        stream.Seek(start);
        uint8_t ent[EntrySize];
        if (stream.Read(ent, EntrySize) != EntrySize)
            return false;
        bool result = ent[0] >= 32;
        for (int i = 0; i < 256; ++i)
        {
            if (ent[i] == 0)
                break;
            if (ent[i] < 32)
                return false;
        }
        return result && ent[255] == 0;
    }

    void Parse() override
    {
        const int64_t start = DirStart(*_stream);
        if (start < 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory offset");
        const int64_t count = (_stream->Size() - 4 - start) / EntrySize;

        _formatId = "BW2STUFF";
        _dirSeparator = "\\";
        _extractInternal = false;
        _stream->Seek(start);
        for (int64_t i = 1; i <= count; ++i)
        {
            uint8_t ent[EntrySize];
            _stream->ReadBuffer(ent, EntrySize);
            uint32_t offset = 0, size = 0, date = 0;
            std::memcpy(&offset, ent + 256, 4);
            std::memcpy(&size, ent + 260, 4);
            std::memcpy(&date, ent + 264, 4);
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(reinterpret_cast<const char *>(ent), 256), offset, size, date);
        }
    }
};
} // namespace

void RegisterBw2Stuff()
{
    Register<Bw2StuffArchive>("*.STUFF", "Black & White 2 Archive (*.STUFF)");
}
} // namespace Dud
