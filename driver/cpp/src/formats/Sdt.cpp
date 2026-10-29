// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Dungeon Keeper 2 .SDT (sound) support, ported from drv_default.dpr of Dragon UnPACKer 5.
// The file starts with the number of entries, then a table of that many offsets (32 bits). At each offset:
// 40 bytes (size of this header, size of the data, name (16 bytes), 4 unknown integers) and the data
// follows the header. The sounds are MP2 audio, so ".mp2" replaces the extension of the name. There is no
// signature: only files with the extension .SDT are tried, and every record must be inside the file.

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t RecordSize = 40;

class SdtArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t count;
        if (stream.Size() < 8 || stream.Read(&count, 4) != 4 || count == 0 || count > 1000000)
            return false;
        if (4 + static_cast<int64_t>(count) * 4 > stream.Size())
            return false;
        // the first record decides: its header size must keep the data inside of the file
        uint32_t offset;
        if (stream.Read(&offset, 4) != 4 || static_cast<int64_t>(offset) + RecordSize > stream.Size())
            return false;
        stream.Seek(offset);
        int32_t headerSize, size;
        return stream.Read(&headerSize, 4) == 4 && stream.Read(&size, 4) == 4 && headerSize >= RecordSize &&
               size >= 0 && static_cast<int64_t>(offset) + headerSize + size <= stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(0);
        const int32_t count = _stream->I32();
        if (count <= 0 || 4 + static_cast<int64_t>(count) * 4 > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        _formatId = "SDT";
        _dirSeparator = "";
        _extractInternal = false;
        for (int32_t x = 1; x <= count; ++x)
        {
            _stream->Seek(static_cast<int64_t>(x) * 4);
            const int32_t offset = _stream->I32();
            if (offset < 0 || static_cast<int64_t>(offset) + RecordSize > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A record is outside of the archive");
            _stream->Seek(offset);
            const int32_t headerSize = _stream->I32();
            const int32_t size = _stream->I32();
            char name[16];
            _stream->ReadBuffer(name);
            const int64_t start = static_cast<int64_t>(offset) + headerSize;
            if (headerSize < 0 || size < 0 || start + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ChangeFileExt(ZString(name), ".mp2"), start, size);
        }
    }
};
} // namespace

void RegisterSdt()
{
    Register<SdtArchive>("*.SDT", "Dungeon Keeper 2 Sound (*.SDT)");
}
} // namespace Dud
