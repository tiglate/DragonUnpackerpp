// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Assassin's Creed .FORGE support (the first game: "scimitar"), ported from the preliminary reader of
// drv_default.dpr of Dragon UnPACKer 5 (disabled there, EXPFORMATS). Header (29 bytes): "scimitar" + null,
// unknown integer, offset of the directory (64 bits), two unknown integers. Directory (88 bytes): counts,
// offset of the table of the files (offset 64 bits, hash, size) and offset of the table of the names (entries of
// 188 bytes, name at 44, 128 bytes), as many entries as the second count.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 4000000;
constexpr int64_t HeaderSize = 29;
constexpr int64_t DirectorySize = 88;
constexpr int64_t FileEntrySize = 16;
constexpr int64_t NameEntrySize = 188;

class ForgeArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[9];
        return stream.Size() >= HeaderSize + DirectorySize && stream.Read(id, 9) == 9 &&
               std::memcmp(id, "scimitar", 9) == 0;
    }

    void Parse() override
    {
        _stream->Seek(13);
        const int64_t dirAt = _stream->I64();
        if (dirAt < HeaderSize || dirAt + DirectorySize > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory offset");
        _stream->Seek(dirAt + 40);
        const uint32_t count = _stream->U32();
        _stream->U32();
        const int64_t filesAt = _stream->I64();
        _stream->Seek(dirAt + 72);
        const int64_t namesAt = _stream->I64();
        if (count > MaxEntries || filesAt < 0 || namesAt < 0 || filesAt + count * FileEntrySize > _stream->Size() ||
            namesAt + count * NameEntrySize > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid tables");
        _formatId = "FORGE";
        _dirSeparator = "";
        _extractInternal = false;
        for (uint32_t x = 0; x < count; ++x)
        {
            _stream->Seek(filesAt + x * FileEntrySize);
            const int64_t offset = _stream->I64();
            const int32_t hash = _stream->I32();
            const int32_t size = _stream->I32();
            _stream->Seek(namesAt + x * NameEntrySize + 44);
            char name[128];
            _stream->ReadBuffer(name);
            if (offset < 0 || size < 0 || offset + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(name), offset, size, hash);
        }
    }
};
} // namespace

void RegisterForge()
{
    Register<ForgeArchive>("*.FORGE", "Assassin's Creed (*.FORGE)");
}
} // namespace Dud
