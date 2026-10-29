// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Godot engine .PCK support. Not in Dragon UnPACKer 5: written from the layout of
// the engine (core/io/file_access_pack.cpp) and checked on a real Godot 3.4 file (pack
// format 1); formats 2 and 3 (Godot 4) are only tested on synthetic files.
// "GDPC", pack format, Godot version (major, minor, patch), then:
//   format 0/1: 16 reserved integers, number of files
//   format 2:   flags, base offset of the files (64 bits), 16 reserved integers, number of files
//   format 3:   flags, base offset, offset of the directory (64 bits): the number of files is there
// Files: path length (padded to 4 bytes), path, offset (64 bits, from the base offset in format
// 2/3), size (64 bits), MD5 (16 bytes) and, in format 2/3, flags (1 = encrypted, 2 = removed).
// An archive can also be appended to an executable: the file then ends with the size of the
// pack (64 bits) and "GDPC", and offsets are relative to the start of the pack.
// Encrypted packs and files are not supported.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t Magic = 0x43504447; // "GDPC"
constexpr uint32_t MaxEntries = 5000000;
constexpr uint32_t FlagEncrypted = 1;
constexpr uint32_t FlagRemoved = 2;
constexpr uint32_t PackEncryptedDir = 1;

// 64 bits sums wrap like in the Pascal version (a damaged header must give the same answer)
auto Sum(int64_t a, int64_t b) -> int64_t
{
    return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

// Start of the pack in the file: 0, or the position found through the trailer; -1 if none
auto PackStart(Reader &stream) -> int64_t
{
    uint32_t magic;
    stream.Seek(0);
    if (stream.Read(&magic, 4) == 4 && magic == Magic)
        return 0;
    if (stream.Size() < 16)
        return -1;
    stream.Seek(stream.Size() - 4);
    if (stream.Read(&magic, 4) == 4 && magic == Magic)
    {
        stream.Seek(stream.Size() - 12);
        int64_t size;
        if (stream.Read(&size, 8) == 8)
        {
            if (size > 0 && size + 12 <= stream.Size())
            {
                stream.Seek(stream.Size() - 12 - size);
                if (stream.Read(&magic, 4) == 4 && magic == Magic)
                    return stream.Size() - 12 - size;
            }
        }
    }
    return -1;
}

class GodotPckArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return PackStart(stream) >= 0;
    }

    void Parse() override
    {
        const int64_t base = PackStart(*_stream);
        if (base < 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Not a Godot pack");
        _stream->Seek(base + 4);
        const uint32_t format = _stream->U32();
        if (format > 3)
            Fail(DUDI7_ERR_UNSUPPORTED, "Unsupported pack format " + std::to_string(format));
        _stream->Skip(12); // Godot version
        int64_t fileBase = 0;
        if (format >= 2)
        {
            const uint32_t packFlags = _stream->U32();
            _stream->ReadBuffer(&fileBase, 8);
            if ((packFlags & PackEncryptedDir) != 0)
                Fail(DUDI7_ERR_UNSUPPORTED, "The directory of this pack is encrypted");
        }
        if (format >= 3)
        {
            int64_t dirOffset;
            _stream->ReadBuffer(&dirOffset, 8);
            if (dirOffset < 0 || Sum(base, dirOffset) >= _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory offset");
            _stream->Seek(base + dirOffset);
        }
        else
            _stream->Skip(64); // 16 reserved integers
        const uint32_t count = _stream->U32();
        if (count > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of files");
        _formatId = "GDPC";
        _dirSeparator = "/";
        _extractInternal = false;
        for (uint32_t i = 1; i <= count; ++i)
        {
            const uint32_t pathLength = _stream->U32();
            if (pathLength > 4096)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid path length");
            std::string path(pathLength, '\0');
            if (pathLength > 0)
                _stream->ReadBuffer(path.data(), pathLength);
            path = ZString(path.data(), pathLength);
            int64_t off, size;
            _stream->ReadBuffer(&off, 8);
            _stream->ReadBuffer(&size, 8);
            uint8_t md5[16];
            _stream->ReadBuffer(md5, 16);
            uint32_t flags = 0;
            if (format >= 2)
                flags = _stream->U32();
            if ((flags & FlagRemoved) != 0)
                continue;
            const int64_t start = Sum(Sum(base, fileBase), off);
            if (off < 0 || size < 0 || Sum(start, size) > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            if (path.compare(0, 6, "res://") == 0)
                path.erase(0, 6);
            else if (path.compare(0, 7, "user://") == 0)
                path.erase(0, 7);
            Add(path, start, size, flags & FlagEncrypted);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        if (_entries[static_cast<size_t>(index)].DataX != 0)
        {
            LastError = "Encrypted files are not supported";
            return DUDI7_ERR_UNSUPPORTED;
        }
        return Archive::Extract(index, sink);
    }
};
} // namespace

void RegisterGodotPck()
{
    Register<GodotPckArchive>("*.PCK", "Godot Engine Package (*.PCK)");
}
} // namespace Dud
