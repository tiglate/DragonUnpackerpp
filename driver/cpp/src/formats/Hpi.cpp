// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Total Annihilation .HPI support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (20 bytes): "HAPI", save marker (not "BANK"), size of the directory, header key,
// start of the directory. When the key is not null everything after the header is
// encrypted: byte = (position xor key) xor (not byte), key = not((k * 4) or (k shr 6)).
// The directory is a tree: a table is a count and the offset of its entries (9 bytes: name
// offset, data offset, flag; flag 1 = folder). A file record is: offset, size, compression
// (0 none, 1 or 2). A compressed file is split in chunks of 64 KiB of unpacked data: a table
// of the chunks sizes, then for each chunk a 19 bytes header ("SQSH", unknown, method (1 =
// LZ77, else zlib), encryption flag, packed size, unpacked size, checksum) and the data
// (optionally scrambled again: byte = (byte - index) xor index).

#include <cstring>

#include <zlib.h>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t Header = 20;
constexpr int MaxDepth = 32;
constexpr size_t MaxEntries = 1000000;
constexpr int64_t Chunk = 65536;
constexpr int64_t MaxDir = 256 * 1024 * 1024;

auto Lz77(const uint8_t *src, int srcLength, uint8_t *dst, int dstLength) -> bool
{
    uint8_t window[4096] = {};
    int ip = 0, op = 0, wp = 1, mask = 1;
    if (srcLength < 1)
        return false;
    int tag = src[ip++];
    while (op < dstLength)
    {
        if ((mask & tag) == 0)
        {
            if (ip >= srcLength)
                return false;
            dst[op] = src[ip];
            window[wp] = src[ip];
            ++op;
            ++ip;
            wp = (wp + 1) & 0xFFF;
        }
        else
        {
            if (ip + 1 >= srcLength)
                return false;
            int cnt = src[ip] + src[ip + 1] * 0x100;
            ip += 2;
            int pos = (cnt & 0xFFF0) >> 4;
            if (pos == 0)
                break;
            cnt = (cnt & 0xF) + 2;
            for (int k = 1; k <= cnt; ++k)
            {
                if (op >= dstLength)
                    break;
                dst[op] = window[pos];
                window[wp] = window[pos];
                ++op;
                wp = (wp + 1) & 0xFFF;
                pos = (pos + 1) & 0xFFF;
            }
        }
        mask *= 2;
        if ((mask & 0x100) == 0x100)
        {
            mask = 1;
            if (ip >= srcLength)
                return op >= dstLength;
            tag = src[ip++];
        }
    }
    return op >= dstLength;
}

// Does the zlib stream give at least dstLength bytes?
auto InflateAtLeast(const uint8_t *src, size_t srcLength, uint8_t *dst, size_t dstLength) -> bool
{
    if (dstLength == 0)
        return true;
    z_stream zs{};
    if (inflateInit(&zs) != Z_OK)
        return false;
    zs.next_in = const_cast<uint8_t *>(src);
    zs.avail_in = static_cast<uInt>(srcLength);
    zs.next_out = dst;
    zs.avail_out = static_cast<uInt>(dstLength);
    // a data error (also a wrong checksum met when the output is full) is an error, as the exception
    // of the Pascal TDecompressionStream (whose zlib port accepts some references too far back and
    // gives garbage: the known difference of the corrupted zlib streams)
    bool ok = true;
    while (zs.avail_out > 0)
    {
        const int rc = inflate(&zs, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END)
        {
            ok = rc == Z_BUF_ERROR;
            break;
        }
        if (rc == Z_STREAM_END || zs.avail_in == 0)
            break;
    }
    ok = ok && zs.avail_out == 0;
    inflateEnd(&zs);
    return ok;
}

class HpiArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char h[8];
        return stream.Read(h, 8) == 8 && std::memcmp(h, "HAPI", 4) == 0 && std::memcmp(h + 4, "BANK", 4) != 0;
    }

    void Parse() override
    {
        _stream->Seek(0);
        uint32_t h[5];
        _stream->ReadBuffer(h, Header);
        const int64_t dirSize = h[2];
        const uint32_t hk = h[3];
        if (dirSize < Header || dirSize > MaxDir || static_cast<int64_t>(h[4]) + dirSize - Header > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");
        if (hk != 0)
            _key = static_cast<uint8_t>(~((hk * 4) | (hk >> 6)));
        else
            _key = 0;
        _dir.assign(static_cast<size_t>(dirSize) + 16, 0);
        _stream->Seek(h[4]);
        _stream->ReadBuffer(&_dir[Header], static_cast<size_t>(dirSize - Header));
        Decrypt(&_dir[Header], dirSize - Header, h[4]);
        _formatId = "HPI";
        _dirSeparator = "\\";
        _extractInternal = true;
        ParseTable("", h[4], 0);
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == 0)
        {
            // Everything after the header is encrypted when there is a key
            if (_key == 0)
                return Archive::Extract(index, sink);
            if (entry.Offset + entry.Size > _stream->Size())
            {
                LastError = "Entry is outside of the file";
                return DUDI7_ERR_IO;
            }
            std::vector<uint8_t> src(static_cast<size_t>(entry.Size) + 1);
            _stream->Seek(entry.Offset);
            if (entry.Size > 0)
                _stream->ReadBuffer(src.data(), static_cast<size_t>(entry.Size));
            Decrypt(src.data(), entry.Size, entry.Offset);
            return ExtractBytes(src.data(), entry.Size, sink);
        }
        int64_t left = entry.Size;
        const int chunks = static_cast<int>((left + Chunk - 1) / Chunk);
        int64_t pos = entry.Offset;
        if (pos + static_cast<int64_t>(chunks) * 4 > _stream->Size())
        {
            LastError = "Invalid chunk table";
            return DUDI7_ERR_IO;
        }
        std::vector<uint32_t> sizes(static_cast<size_t>(chunks) + 1);
        _stream->Seek(pos);
        if (chunks > 0)
            _stream->ReadBuffer(sizes.data(), static_cast<size_t>(chunks) * 4);
        Decrypt(sizes.data(), static_cast<int64_t>(chunks) * 4, pos);
        pos += static_cast<int64_t>(chunks) * 4;
        for (int i = 0; i < chunks; ++i)
        {
            if (pos + 19 > _stream->Size())
            {
                LastError = "Truncated chunk";
                return DUDI7_ERR_IO;
            }
            _stream->Seek(pos);
            uint8_t hdr[19];
            _stream->ReadBuffer(hdr, 19);
            Decrypt(hdr, 19, pos);
            uint32_t csize, dsize;
            std::memcpy(&csize, hdr + 7, 4);
            std::memcpy(&dsize, hdr + 11, 4);
            if (dsize > Chunk * 2 || pos + 19 + static_cast<int64_t>(csize) > _stream->Size())
            {
                LastError = "Invalid chunk";
                return DUDI7_ERR_IO;
            }
            std::vector<uint8_t> src(static_cast<size_t>(csize) + 1);
            std::vector<uint8_t> dst(static_cast<size_t>(dsize) + 1);
            if (csize > 0)
                _stream->ReadBuffer(src.data(), csize);
            Decrypt(src.data(), csize, pos + 19);
            if (hdr[6] == 1)
                for (int j = 0; j < static_cast<int>(csize); ++j)
                    src[static_cast<size_t>(j)] = static_cast<uint8_t>(static_cast<uint8_t>(src[static_cast<size_t>(j)] - j) ^ static_cast<uint8_t>(j));
            if (hdr[5] == 1)
            {
                if (!Lz77(src.data(), static_cast<int>(csize), dst.data(), static_cast<int>(dsize)))
                {
                    LastError = "Invalid LZ77 data";
                    return DUDI7_ERR_IO;
                }
            }
            else if (!InflateAtLeast(src.data(), csize, dst.data(), dsize))
            {
                LastError = "Cannot unpack the chunk: invalid zlib data";
                return DUDI7_ERR_IO;
            }
            int64_t out = dsize;
            if (out > left)
                out = left;
            const int32_t result = ExtractBytes(dst.data(), out, sink);
            if (result != DUDI7_OK)
                return result;
            left -= out;
            pos += sizes[static_cast<size_t>(i)];
        }
        return DUDI7_OK;
    }

  private:
    void Decrypt(void *buffer, int64_t length, int64_t filePos) const
    {
        if (_key == 0)
            return;
        auto *p = static_cast<uint8_t *>(buffer);
        for (int64_t i = 0; i < length; ++i)
            p[i] = static_cast<uint8_t>(static_cast<uint8_t>((filePos + i) ^ _key) ^ static_cast<uint8_t>(~p[i]));
    }

    auto DirU32(int64_t offset) -> uint32_t
    {
        if (offset < 0 || offset + 4 > static_cast<int64_t>(_dir.size()))
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory offset");
        uint32_t value;
        std::memcpy(&value, &_dir[static_cast<size_t>(offset)], 4);
        return value;
    }

    void ParseTable(const std::string &parent, int64_t offset, int depth)
    {
        if (depth > MaxDepth)
            Fail(DUDI7_ERR_NOTFORMAT, "Folders nested too deep");
        const uint32_t entries = DirU32(offset);
        const uint32_t entryOffset = DirU32(offset + 4);
        if (entries > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
        for (int64_t i = 0; i < entries; ++i)
        {
            const int64_t e = static_cast<int64_t>(entryOffset) + i * 9;
            const uint32_t nameOffset = DirU32(e);
            const uint32_t dataOffset = DirU32(e + 4);
            if (e + 9 > static_cast<int64_t>(_dir.size()))
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory");
            const uint8_t flag = _dir[static_cast<size_t>(e + 8)];
            if (nameOffset >= _dir.size())
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid name offset");
            size_t start = nameOffset;
            while (start < _dir.size() && _dir[start] != 0)
                ++start;
            std::string name(reinterpret_cast<const char *>(&_dir[nameOffset]), start - nameOffset);
            if (!parent.empty())
                name = parent + "\\" + name;
            if (flag == 1)
                ParseTable(name, dataOffset, depth + 1);
            else
            {
                if (_entries.size() >= MaxEntries)
                    Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
                if (static_cast<int64_t>(DirU32(dataOffset)) > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
                const size_t flagAt = static_cast<size_t>(dataOffset) + 8;
                Add(name, DirU32(dataOffset), DirU32(static_cast<int64_t>(dataOffset) + 4),
                    flagAt < _dir.size() ? _dir[flagAt] : 0);
            }
        }
    }

    uint8_t _key = 0;
    std::vector<uint8_t> _dir;
};
} // namespace

void RegisterHpi()
{
    Register<HpiArchive>("*.HPI;*.UFO;*.CCX",
                         "Total Annihilation Archive (*.HPI;*.UFO;*.CCX)|Total Annihilation (*.HPI)");
}
} // namespace Dud
