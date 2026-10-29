// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// UFO: Aftermath / Aftershock / Afterlight .VFS support, ported from drv_default.dpr of
// Dragon UnPACKer 5.
// Header (48 bytes): version 1.0 (float), block size, number of placement entries (8 bytes
// each), number of entries in the root directory, 0, maximum name length (64), compression
// window, MD5, size of the extra info. The root directory is at 48 + placement * 8 + extra
// + 4; the blocks start after it. Entries (88 bytes): name[64], unknown, type (1 = file,
// 2 = folder, 9 = compressed file), -1, relative block, size, unpacked size. Compressed files
// are a series of chunks (32 bits size + zlib data, each giving one compression window of
// data) and 8 bytes chunks (size = 8 and 2 integers) to skip.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t HeaderSize = 48;
constexpr int64_t EntrySize = 88;
constexpr int MaxDepth = 32;
constexpr int64_t MaxEntries = 1000000;

class VfsArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t h[8];
        return stream.Read(h, 32) == 32 && h[0] == 0x3F800000 && h[4] == 0 && h[5] == 64;
    }

    void Parse() override
    {
        _stream->Seek(0);
        uint32_t h[12];
        _stream->ReadBuffer(h, HeaderSize);
        _blockSize = h[1];
        _window = h[6];
        const int64_t root = static_cast<int64_t>(h[2]) * 8 + HeaderSize + h[11] + 4;
        _start = root + static_cast<int64_t>(h[3]) * EntrySize;
        if (_blockSize <= 0 || _window <= 0 || root > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");

        _formatId = "VFS";
        _dirSeparator = "\\";
        _extractInternal = true;
        ParseDir("", root, h[3], 0);
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == 0)
            return Archive::Extract(index, sink);
        int64_t pos = entry.Offset;
        int64_t packedLeft = entry.DataY;
        int64_t left = entry.Size;
        if (pos + packedLeft > _stream->Size())
        {
            LastError = "Invalid compressed entry";
            return DUDI7_ERR_IO;
        }
        while (packedLeft > 0 && left > 0)
        {
            _stream->Seek(pos);
            const uint32_t chunkSize = _stream->U32();
            int64_t n;
            if (chunkSize == 8)
            {
                n = 12;
            }
            else
            {
                if (static_cast<int64_t>(chunkSize) + 4 > packedLeft)
                {
                    LastError = "Invalid chunk";
                    return DUDI7_ERR_IO;
                }
                const int64_t chunk = left > _window ? _window : left;
                const int32_t status = ExtractZlib(pos + 4, chunkSize, chunk, sink);
                if (status != DUDI7_OK)
                    return status;
                left -= chunk;
                n = static_cast<int64_t>(chunkSize) + 4;
            }
            pos += n;
            packedLeft -= n;
        }
        if (left > 0)
        {
            LastError = "The compressed data ends too early";
            return DUDI7_ERR_IO;
        }
        return DUDI7_OK;
    }

  private:
    void ParseDir(const std::string &dir, int64_t offset, int64_t count, int depth)
    {
        if (depth > MaxDepth || offset < 0 || count < 0 || offset + count * EntrySize > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory");
        for (int64_t i = 0; i < count; ++i)
        {
            _stream->Seek(offset + i * EntrySize);
            uint8_t ent[EntrySize];
            _stream->ReadBuffer(ent, EntrySize);
            int32_t type = 0, rel = 0, size = 0, original = 0;
            std::memcpy(&type, ent + 68, 4);
            std::memcpy(&rel, ent + 76, 4);
            std::memcpy(&size, ent + 80, 4);
            std::memcpy(&original, ent + 84, 4);
            const std::string name = ZString(reinterpret_cast<const char *>(ent), 64);
            const int64_t where = _start + (static_cast<int64_t>(rel) - 1) * _blockSize;
            switch (type)
            {
            case 1:
            case 9:
                if (where < 0 || size < 0 || where + size > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                if (Count() >= MaxEntries)
                    Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
                if (type == 1)
                    Add(dir + name, where, size);
                else
                    Add(dir + name, where, original, 1, size);
                break;
            case 2:
                ParseDir(dir + name + "\\", where, size / EntrySize, depth + 1);
                break;
            default:
                return; // an unknown type ends the directory
            }
        }
    }

    int64_t _blockSize = 0;
    int64_t _start = 0;
    int64_t _window = 0;
};
} // namespace

void RegisterVfs()
{
    Register<VfsArchive>("*.VFS", "UFO Virtual File System (*.VFS)|UFO: Aftermath (*.VFS)|UFO: Aftershock "
                                  "(*.VFS)|UFO: Afterlight (*.VFS)");
}
} // namespace Dud
