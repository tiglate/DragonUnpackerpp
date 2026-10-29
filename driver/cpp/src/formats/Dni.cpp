// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// realMyst 3D .DNI support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (28 bytes): "Dirt", version 65536, offset of the root directory, offset of the file table, offset of
// the names, offset of the data, another table offset. A directory is an offset to its name (null terminated)
// and the number of objects, followed by that many 32 bits offsets: an offset before the file table is a
// sub directory, any other is a file record (offset of the name, offset of the next name, size, offset, packed
// size). Files with a packed size are zlib streams.
// Deviation from v5: the files of a sub directory get the whole path of their directory (v5 left out the name
// of the directory itself, which looks like a slip); not checked on a real file.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int MaxDepth = 64;
constexpr size_t MaxEntries = 1000000;

class DniArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[4];
        int32_t h[6];
        if (stream.Size() < 28 || stream.Read(id, 4) != 4 || std::string_view(id, 4) != "Dirt" ||
            stream.Read(h, 24) != 24 || h[0] != 65536)
            return false;
        return h[1] > 0 && h[1] < stream.Size() && h[2] > 0 && h[2] < stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(4);
        int32_t h[6];
        _stream->ReadBuffer(h, 24);
        if (h[0] != 65536)
            Fail(DUDI7_ERR_NOTFORMAT, "Unknown version");
        _formatId = "DNI";
        _dirSeparator = "\\";
        _extractInternal = true;
        _fileTable = h[2];
        Directory("", h[1], 0);
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX > 0)
            return ExtractZlib(entry.Offset, entry.DataX, entry.Size, sink);
        return Archive::Extract(index, sink);
    }

  private:
    auto CString(int64_t offset) -> std::string
    {
        if (offset < 0 || offset >= _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "A name is outside of the file");
        _stream->Seek(offset);
        return _stream->CString(1024);
    }

    void Directory(const std::string &parent, int64_t offset, int depth)
    {
        if (depth > MaxDepth)
            Fail(DUDI7_ERR_NOTFORMAT, "Folders nested too deep");
        if (offset < 0 || offset + 8 > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "A directory is outside of the file");
        _stream->Seek(offset);
        const int32_t nameOffset = _stream->I32();
        const int32_t count = _stream->I32();
        const std::string path = parent + CString(nameOffset) + "\\";
        const std::string prefix = path == "\\" ? "" : path;
        if (count < 0 || offset + 8 + static_cast<int64_t>(count) * 4 > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of objects");
        for (int32_t x = 0; x < count; ++x)
        {
            _stream->Seek(offset + 8 + static_cast<int64_t>(x) * 4);
            const int32_t objOffset = _stream->I32();
            if (objOffset < _fileTable)
                Directory(prefix, objOffset, depth + 1);
            else
            {
                if (objOffset + 20 > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file record is outside of the file");
                _stream->Seek(objOffset);
                int32_t record[5];
                _stream->ReadBuffer(record, 20);
                const std::string name = CString(record[0]);
                if (_entries.size() >= MaxEntries)
                    Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
                const int64_t stored = record[4] > 0 ? record[4] : record[2];
                if (record[2] < 0 || record[3] < 0 || static_cast<int64_t>(record[3]) + stored > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                Add(prefix + name, record[3], record[2], record[4]);
            }
        }
    }

    int64_t _fileTable = 0;
};
} // namespace

void RegisterDni()
{
    Register<DniArchive>("*.DNI", "realMyst 3D Archive (*.DNI)");
}
} // namespace Dud
