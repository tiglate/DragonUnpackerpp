// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Unreal Engine 4 .PAK support. Not in Dragon UnPACKer 5: written from the layout of the
// engine (Runtime/PakFile) and checked on a real file (Boltgun, pack version 11).
// The footer is at the end of the file: [encryption key GUID (16 bytes, version >= 7)]
// [index is encrypted (1 byte, version >= 4)] magic $5A6F12E1, version, offset of the index,
// size of the index, SHA1 of the index, [frozen index (1 byte, version 9)] [names of the
// compression methods (version >= 8: 4 or 5 names of 32 bytes)].
// Index up to version 9: mount point, number of entries, then for each: name and entry.
// Version 10 and 11: mount point, number of entries, path hash seed, location of the path
// hash index, location of the full directory index (names), the "encoded" entries (bit
// packed) and the entries that cannot be encoded; the names come from the full directory
// index: for each folder, its name and its files (name + location of the entry).
// Each entry has a copy of its description before the data of the file.
// Supported: stored files and zlib compressed files (blocks). Not supported: encryption
// and the other compressions (Oodle, LZ4, gzip): the files are listed, extracting them fails.

#include <cstring>

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t Magic = 0x5A6F12E1;
constexpr int64_t MaxIndex = 1024 * 1024 * 1024;
constexpr uint32_t MaxEntries = 10000000;
constexpr uint32_t MaxBlocks = 100000000;

// 64 bits sums wrap like in the Pascal version
auto Sum(int64_t a, int64_t b) -> int64_t
{
    return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

struct Info
{
    int32_t First = -1; // first block in the blocks table (-1: none)
    int32_t Count = 0;
    uint32_t BlockSize = 0;
    bool Encrypted = false;
};

struct Raw // an entry as read from the index
{
    int64_t Offset = 0, Size = 0, USize = 0;
    uint32_t Method = 0;
    uint32_t BlockSize = 0;
    bool Encrypted = false;
    std::vector<int64_t> Blocks; // start, end, start, end...
};

struct Footer
{
    int64_t MagicPos = 0;
    uint32_t Version = 0;
    int NumNames = 0;
    bool Frozen = false;
};

// Position of the magic in the footer, with the version and the number of names after it
auto FindFooter(Reader &s, Footer &footer) -> bool
{
    struct Candidate
    {
        int Back, Names;
    };
    static constexpr Candidate candidates[] = {{44, 0}, {172, 4}, {204, 5}, {205, 5}};
    for (const Candidate &candidate : candidates)
    {
        if (s.Size() < candidate.Back + 17)
            continue;
        footer.MagicPos = s.Size() - candidate.Back;
        s.Seek(footer.MagicPos);
        uint32_t m, v;
        if (s.Read(&m, 4) != 4 || m != Magic || s.Read(&v, 4) != 4)
            continue;
        footer.Version = v;
        footer.Frozen = candidate.Back == 205;
        footer.NumNames = candidate.Names;
        if ((footer.NumNames == 0 && footer.Version < 8) || (footer.NumNames > 0 && footer.Version >= 8))
            if (!footer.Frozen || footer.Version == 9)
                return true;
    }
    return false;
}

class UnrealPakArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        Footer footer;
        return FindFooter(stream, footer);
    }

    void Parse() override
    {
        Footer footer;
        if (!FindFooter(*_stream, footer))
            Fail(DUDI7_ERR_NOTFORMAT, "No footer");
        _version = footer.Version;
        _stream->Seek(footer.MagicPos + 8);
        int64_t indexOff, indexSize;
        _stream->ReadBuffer(&indexOff, 8);
        _stream->ReadBuffer(&indexSize, 8);
        _stream->Skip(20); // SHA1
        if (footer.Frozen)
            _stream->Skip(1);
        _methodNames.clear();
        for (int i = 0; i < footer.NumNames; ++i)
        {
            char buffer[32];
            _stream->ReadBuffer(buffer, 32);
            _methodNames.push_back(ZString(buffer, 32));
        }
        uint8_t encrypted = 0;
        if (_version >= 4)
        {
            _stream->Seek(footer.MagicPos - 1);
            _stream->ReadBuffer(&encrypted, 1);
        }
        if (encrypted != 0)
            Fail(DUDI7_ERR_UNSUPPORTED, "The index of this pack is encrypted");
        if (_version > 11)
            Fail(DUDI7_ERR_UNSUPPORTED, "Unsupported pack version " + std::to_string(_version));
        LoadBuffer(indexOff, indexSize);
        _formatId = "UEPAK" + std::to_string(_version);
        _dirSeparator = "/";
        _extractInternal = true;
        const std::string mount = GetFString();
        const uint32_t num = GetU32();
        if (num > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        if (_version >= 10)
            ParseIndex10(mount, num);
        else
            ParseIndex9(mount, num);
        _index.clear();
        _index.shrink_to_fit();
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        const Info info = _info[static_cast<size_t>(entry.DataY)];
        if (info.Encrypted)
        {
            LastError = "Encrypted files are not supported";
            return DUDI7_ERR_UNSUPPORTED;
        }
        if (entry.DataX == 0)
            return Archive::Extract(index, sink);
        const auto method = static_cast<uint32_t>(entry.DataX);
        if (ToLower(MethodName(method)) != "zlib")
        {
            LastError = "Unsupported compression: " + MethodName(method);
            return DUDI7_ERR_UNSUPPORTED;
        }
        if (info.Count == 0 || (info.BlockSize == 0 && info.Count > 1))
        {
            LastError = "Invalid compressed entry";
            return DUDI7_ERR_IO;
        }
        const int64_t baseOff = _version >= 5 ? entry.Offset : 0;
        int64_t left = entry.Size;
        for (int32_t b = 0; b < info.Count; ++b)
        {
            const size_t at = (static_cast<size_t>(info.First) + static_cast<size_t>(b)) * 2;
            const int64_t pos = Sum(baseOff, _blocks[at]);
            const int64_t size = Sum(_blocks[at + 1], -_blocks[at]);
            // every block gives BlockSize bytes but the last one, which gives what is left
            int64_t unpacked = b == info.Count - 1 ? left : static_cast<int64_t>(info.BlockSize);
            if (unpacked > left)
                unpacked = left;
            if (size < 0 || pos < 0 || Sum(pos, size) > _stream->Size())
            {
                LastError = "A block is outside of the file";
                return DUDI7_ERR_IO;
            }
            const int32_t result = ExtractZlib(pos, size, unpacked, sink);
            if (result != DUDI7_OK)
                return result;
            left -= unpacked;
        }
        if (left > 0)
        {
            LastError = "The blocks do not cover the whole file";
            return DUDI7_ERR_IO;
        }
        return DUDI7_OK;
    }

  private:
    auto MethodName(uint32_t m) const -> std::string
    {
        if (m == 0)
            return "";
        if (_version >= 8)
        {
            if (m <= _methodNames.size())
                return _methodNames[m - 1];
            return "#" + std::to_string(m);
        }
        if (m == 1)
            return "Zlib";
        return "#" + std::to_string(m);
    }

    void NeedBytes(int64_t n) const
    {
        if (n < 0 || _pos + n > static_cast<int64_t>(_buf->size()))
            Fail(DUDI7_ERR_NOTFORMAT, "The index is truncated");
    }

    auto GetU8() -> uint8_t
    {
        NeedBytes(1);
        return (*_buf)[static_cast<size_t>(_pos++)];
    }

    auto GetU32() -> uint32_t
    {
        NeedBytes(4);
        uint32_t value;
        std::memcpy(&value, &(*_buf)[static_cast<size_t>(_pos)], 4);
        _pos += 4;
        return value;
    }

    auto GetU64() -> int64_t
    {
        NeedBytes(8);
        int64_t value;
        std::memcpy(&value, &(*_buf)[static_cast<size_t>(_pos)], 8);
        _pos += 8;
        return value;
    }

    // FString: 32 bits length; > 0: that many ANSI chars (the null included), < 0: UTF-16
    auto GetFString() -> std::string
    {
        int32_t length = static_cast<int32_t>(GetU32());
        if (length == 0)
            return "";
        if (length > 65536 || length < -65536)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid string length");
        if (length > 0)
        {
            NeedBytes(length);
            std::string text = ZString(reinterpret_cast<const char *>(&(*_buf)[static_cast<size_t>(_pos)]),
                                       static_cast<size_t>(length));
            _pos += length;
            return text;
        }
        length = -length;
        NeedBytes(static_cast<int64_t>(length) * 2);
        std::u16string wide(static_cast<size_t>(length), u'\0');
        std::memcpy(wide.data(), &(*_buf)[static_cast<size_t>(_pos)], static_cast<size_t>(length) * 2);
        _pos += static_cast<int64_t>(length) * 2;
        if (!wide.empty() && wide.back() == u'\0')
            wide.pop_back();
        return Utf16ToUtf8(wide);
    }

    void LoadBuffer(int64_t offset, int64_t size)
    {
        if (offset < 0 || size < 0 || size > MaxIndex || Sum(offset, size) > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid index location");
        _index.assign(static_cast<size_t>(size), 0);
        _stream->Seek(offset);
        if (size > 0)
            _stream->ReadBuffer(_index.data(), static_cast<size_t>(size));
        _buf = &_index;
        _pos = 0;
    }

    // An entry as serialized in the index (and before the data of each file)
    void ReadEntry(Raw &e)
    {
        e.Offset = GetU64();
        e.Size = GetU64();
        e.USize = GetU64();
        e.Method = GetU32();
        if (_version <= 1)
            GetU64();
        NeedBytes(20);
        _pos += 20;
        e.Blocks.clear();
        e.BlockSize = 0;
        e.Encrypted = false;
        if (_version >= 3)
        {
            if (e.Method != 0)
            {
                const uint32_t n = GetU32();
                if (n > MaxBlocks)
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of blocks");
                NeedBytes(static_cast<int64_t>(n) * 16);
                e.Blocks.resize(static_cast<size_t>(n) * 2);
                for (size_t i = 0; i < n; ++i)
                {
                    e.Blocks[i * 2] = GetU64();
                    e.Blocks[i * 2 + 1] = GetU64();
                }
            }
            e.Encrypted = (GetU8() & 1) != 0;
            e.BlockSize = GetU32();
        }
    }

    // An entry bit packed in the "encoded" block of the version 10 and 11 indexes
    void DecodeEntry(int64_t pos, Raw &e)
    {
        _pos = pos;
        const uint32_t v = GetU32();
        e.Encrypted = (v & (1u << 22)) != 0;
        const uint32_t n = (v >> 6) & 0xFFFF;
        e.BlockSize = v & 0x3F;
        if (e.BlockSize == 0x3F)
            e.BlockSize = GetU32();
        else
            e.BlockSize <<= 11;
        e.Method = (v >> 23) & 0x3F;
        e.Offset = (v & (1u << 31)) != 0 ? static_cast<int64_t>(GetU32()) : GetU64();
        e.USize = (v & (1u << 30)) != 0 ? static_cast<int64_t>(GetU32()) : GetU64();
        if (e.Method != 0)
            e.Size = (v & (1u << 29)) != 0 ? static_cast<int64_t>(GetU32()) : GetU64();
        else
            e.Size = e.USize;
        int64_t header = 48 + 5;
        if (e.Method != 0)
            header += 4 + static_cast<int64_t>(n) * 16;
        e.Blocks.assign(static_cast<size_t>(n) * 2, 0);
        if (n > 0)
        {
            if (n == 1 && !e.Encrypted)
            {
                e.Blocks[0] = header;
                e.Blocks[1] = Sum(header, e.Size);
            }
            else
            {
                int64_t start = header;
                for (size_t i = 0; i < n; ++i)
                {
                    const uint32_t size = GetU32();
                    e.Blocks[i * 2] = start;
                    e.Blocks[i * 2 + 1] = start + size;
                    start += size;
                    if (e.Encrypted)
                        start = (start + 15) & ~static_cast<int64_t>(15);
                }
            }
        }
    }

    void AddEntry(const std::string &name, const Raw &e)
    {
        if (_entries.size() >= MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
        // Header of the copy of the entry that precedes the data
        int64_t header = 48;
        if (_version <= 1)
            header += 8;
        if (_version >= 3)
        {
            if (e.Method != 0)
                header += 4 + static_cast<int64_t>(e.Blocks.size() / 2) * 16;
            header += 5;
        }
        if (e.Offset < 0 || e.Size < 0 || e.USize < 0 || Sum(e.Offset, e.Size) > _stream->Size() * 2)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid entry");
        Info info;
        info.Encrypted = e.Encrypted;
        info.BlockSize = e.BlockSize;
        info.Count = static_cast<int32_t>(e.Blocks.size() / 2);
        if (info.Count > 0)
        {
            info.First = static_cast<int32_t>(_blocks.size() / 2);
            _blocks.insert(_blocks.end(), e.Blocks.begin(), e.Blocks.end());
        }
        const auto idx = static_cast<int64_t>(_info.size());
        _info.push_back(info);
        // Name without the "../../../" of the mount point
        std::string clean = name;
        for (;;)
        {
            if (clean.compare(0, 3, "../") == 0)
                clean.erase(0, 3);
            else if (clean.compare(0, 2, "./") == 0)
                clean.erase(0, 2);
            else
                break;
        }
        while (!clean.empty() && clean[0] == '/')
            clean.erase(0, 1);
        if (e.Method == 0)
            Add(clean, Sum(e.Offset, header), e.USize, 0, idx);
        else
            Add(clean, e.Offset, e.USize, e.Method, idx);
    }

    void ParseIndex9(const std::string &mount, uint32_t num)
    {
        Raw e;
        for (uint32_t i = 1; i <= num; ++i)
        {
            const std::string name = GetFString();
            ReadEntry(e);
            AddEntry(mount + name, e);
        }
    }

    void ParseIndex10(const std::string &mount, uint32_t num)
    {
        GetU64(); // path hash seed
        if (GetU32() != 0)
        {
            GetU64();
            GetU64();
            NeedBytes(20);
            _pos += 20;
        }
        if (GetU32() == 0)
            Fail(DUDI7_ERR_UNSUPPORTED, "This pack has no directory index (names are not stored)");
        const int64_t fullOff = GetU64();
        const int64_t fullSize = GetU64();
        NeedBytes(20);
        _pos += 20;
        const uint32_t encSize = GetU32();
        NeedBytes(encSize);
        const std::vector<uint8_t> encoded(_index.begin() + _pos, _index.begin() + _pos + encSize);
        _pos += encSize;
        const uint32_t numNon = GetU32();
        if (numNon > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        std::vector<Raw> nonEncoded(numNon);
        for (uint32_t i = 0; i < numNon; ++i)
            ReadEntry(nonEncoded[i]);
        // The directory index
        LoadBuffer(fullOff, fullSize);
        const uint32_t dirs = GetU32();
        if (dirs > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of folders");
        Raw e;
        for (uint32_t i = 1; i <= dirs; ++i)
        {
            const std::string dirName = GetFString();
            const uint32_t files = GetU32();
            if (files > MaxEntries)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of files");
            for (uint32_t j = 1; j <= files; ++j)
            {
                const std::string fileName = GetFString();
                int32_t loc = static_cast<int32_t>(GetU32());
                if (loc >= 0)
                {
                    // decode from the encoded block, keeping the position in the directory index
                    const int64_t savedPos = _pos;
                    _buf = &encoded;
                    if (static_cast<int64_t>(loc) + 4 > static_cast<int64_t>(encoded.size()))
                        Fail(DUDI7_ERR_NOTFORMAT, "Invalid entry location");
                    DecodeEntry(loc, e);
                    _buf = &_index;
                    _pos = savedPos;
                    AddEntry(mount + dirName + fileName, e);
                }
                else
                {
                    loc = -(loc + 1);
                    if (loc >= static_cast<int32_t>(numNon))
                        Fail(DUDI7_ERR_NOTFORMAT, "Invalid entry location");
                    AddEntry(mount + dirName + fileName, nonEncoded[static_cast<size_t>(loc)]);
                }
            }
        }
        if (num != static_cast<uint32_t>(_entries.size()))
            Fail(DUDI7_ERR_NOTFORMAT, "The directory index does not match the number of entries");
    }

    uint32_t _version = 0;
    std::vector<std::string> _methodNames;
    std::vector<Info> _info;
    std::vector<int64_t> _blocks; // start, end pairs
    std::vector<uint8_t> _index;
    const std::vector<uint8_t> *_buf = &_index;
    int64_t _pos = 0;
};
} // namespace

void RegisterUnrealPak()
{
    Register<UnrealPakArchive>("*.PAK", "Unreal Engine 4 Pak (*.PAK)");
}
} // namespace Dud
