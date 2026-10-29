// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Movies .PAK support, ported from drv_default.dpr of Dragon UnPACKer 5.
// 52 bytes header (id 5, data offset, unknown, number of entries, number of folder tables,
// number of folders, size of the folders chunk, 4 unknown, table offset, folders chunk
// offset), then entries (56 bytes: 2 unknown, offset, compressed size, size, folder position
// (15 significant bits, shifted by 1), name[32]). Each stored file starts with a 16 bytes
// header (total size, size, compressed size, "not compressed" flag), followed by zlib data
// or the raw bytes; a non compressed first header can be followed by "zcmp" and a second
// header of the same kind. No reliable signature: only tried on .PAK files.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t HeaderSize = 52;
constexpr int64_t EntrySize = 56;

class TmPakArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t h[13];
        if (stream.Read(h, HeaderSize) != HeaderSize)
            return false;
        const int64_t size = stream.Size();
        return h[0] == 5 && h[1] <= size && h[11] <= size && h[12] <= size &&
               static_cast<int64_t>(h[12]) + h[6] <= size && static_cast<int64_t>(h[3]) * EntrySize + HeaderSize <= size;
    }

    void Parse() override
    {
        _stream->Seek(12);
        const uint32_t count = _stream->U32();
        _stream->Seek(24);
        const uint32_t dirSize = _stream->U32();
        _stream->Seek(48);
        const uint32_t dirOffset = _stream->U32();
        CheckTable(dirOffset, 1, dirSize);
        CheckTable(HeaderSize, count, EntrySize);
        std::vector<char> dirs(static_cast<size_t>(dirSize) + 1, '\0');
        _stream->Seek(dirOffset);
        if (dirSize > 0)
            _stream->ReadBuffer(dirs.data(), dirSize);

        _formatId = "TMPAK";
        _dirSeparator = "\\";
        _extractInternal = true;
        _stream->Seek(HeaderSize);
        for (uint32_t i = 1; i <= count; ++i)
        {
            _stream->U32();
            _stream->U32();
            const uint32_t offset = _stream->U32();
            const uint32_t compressedSize = _stream->U32();
            const uint32_t size = _stream->U32();
            const uint32_t dirPos = _stream->U32();
            char name[32];
            _stream->ReadBuffer(name);
            const uint32_t p = (dirPos & 0x7FFF) >> 1;
            if (p >= dirSize)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid folder position");
            Add(ZString(dirs.data() + p, dirSize - p) + ZString(name), offset, size, compressedSize);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        const int64_t offset = entry.Offset;
        const int64_t size = entry.Size;
        if (offset < 0 || offset + 16 > _stream->Size())
            return Archive::Extract(index, sink);
        uint32_t h[4];
        _stream->Seek(offset);
        _stream->ReadBuffer(h, 16);
        if (static_cast<int64_t>(h[0]) != entry.DataX || static_cast<int64_t>(h[1]) != size)
            return Archive::Extract(index, sink);
        if (h[3] == 1)
        {
            char id[4];
            if (_stream->Read(id, 4) == 4 && Equals(id, "zcmp"))
            {
                _stream->ReadBuffer(h, 16);
                if (h[3] == 1)
                    return RawCopy(offset + 16, size, sink);
                return ExtractZlib(_stream->Pos(), h[2], size, sink);
            }
            return RawCopy(offset + 16, size, sink);
        }
        return ExtractZlib(offset + 16, h[2], size, sink);
    }

  private:
    auto RawCopy(int64_t from, int64_t length, const Sink &sink) -> int32_t
    {
        if (from < 0 || length < 0 || from + length > _stream->Size())
        {
            LastError = "Entry is outside of the file";
            return DUDI7_ERR_IO;
        }
        std::vector<uint8_t> buffer(static_cast<size_t>(length));
        _stream->Seek(from);
        if (length > 0)
            _stream->ReadBuffer(buffer.data(), buffer.size());
        return ExtractBytes(buffer.data(), length, sink);
    }
};
} // namespace

void RegisterTmPak()
{
    Register<TmPakArchive>("*.PAK", "The Movies Archive (*.PAK)");
}
} // namespace Dud
