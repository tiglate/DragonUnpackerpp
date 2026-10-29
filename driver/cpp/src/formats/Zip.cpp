// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ZIP archives (and what is a ZIP with another name: Quake III/4 PK3/PK4, Nightdive KPF, JAR, APK, LOVE...).
// Not in Dragon UnPACKer 5 (it used Abbrevia). The end of central directory record (the last 22 bytes, followed
// by a comment of up to 64 KB) is found from the end of the file: its signature, the directory that it points
// to and the number of entries must agree, so a file of another format is not taken for a ZIP. The entries
// are those of the central directory (so a file that does not start with "PK" - self extractors, data
// in front - is read too: the offsets are corrected by the size of what precedes the first entry). ZIP64
// (sizes and offsets beyond 4 GB) is read. Methods: stored, deflate, LZMA; the others (Deflate64, BZip2,
// Zstd, XZ) and the encrypted entries are listed but extraction says "unsupported".
// The names are UTF-8 when the flag says so, otherwise CP437/CP1252 (as the base class guesses).

#include <array>
#include <cstring>

#include "Registry.h"
#include "codecs/Lzma.h"

namespace Dud
{
namespace
{
constexpr int64_t MaxDir = 512 * 1024 * 1024;
constexpr int64_t MaxLzma = 1024 * 1024 * 1024;
constexpr uint32_t SigEocd64 = 0x06064B50;
constexpr uint32_t SigLoc64 = 0x07064B50;
constexpr uint32_t SigCdir = 0x02014B50;
constexpr uint32_t SigLocal = 0x04034B50;

// 64 bits arithmetic wraps like in the Pascal version
auto Add64(int64_t a, int64_t b) -> int64_t
{
    return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

auto Le16(const uint8_t *b, int64_t p) -> int
{
    return b[p] | (b[p + 1] << 8);
}

auto Le32(const uint8_t *b, int64_t p) -> uint32_t
{
    return static_cast<uint32_t>(b[p]) | (static_cast<uint32_t>(b[p + 1]) << 8) |
           (static_cast<uint32_t>(b[p + 2]) << 16) | (static_cast<uint32_t>(b[p + 3]) << 24);
}

auto Le64(const uint8_t *b, int64_t p) -> int64_t
{
    return static_cast<int64_t>(static_cast<uint64_t>(Le32(b, p)) | (static_cast<uint64_t>(Le32(b, p + 4)) << 32));
}

// Position of the end of central directory record, -1 if there is none
auto FindEocd(Reader &s) -> int64_t
{
    if (s.Size() < 22)
        return -1;
    int64_t n = s.Size();
    if (n > 65536 + 22)
        n = 65536 + 22;
    const int64_t base = s.Size() - n;
    std::vector<uint8_t> buf(static_cast<size_t>(n));
    s.Seek(base);
    if (s.Read(buf.data(), buf.size()) != buf.size())
        return -1;
    // the comment must end exactly at the end of the file...
    for (int64_t i = n - 22; i >= 0; --i)
        if (buf[static_cast<size_t>(i)] == 0x50 && buf[static_cast<size_t>(i) + 1] == 0x4B &&
            buf[static_cast<size_t>(i) + 2] == 5 && buf[static_cast<size_t>(i) + 3] == 6)
            if (i + 22 + Le16(buf.data(), i + 20) == n)
                return base + i;
    // ...or, for the files with a few bytes of junk after the record (Hitman: Contracts), the last record whose
    // comment fits; the callers check the directory that it points to
    for (int64_t i = n - 22; i >= 0; --i)
        if (buf[static_cast<size_t>(i)] == 0x50 && buf[static_cast<size_t>(i) + 1] == 0x4B &&
            buf[static_cast<size_t>(i) + 2] == 5 && buf[static_cast<size_t>(i) + 3] == 6)
            if (i + 22 + Le16(buf.data(), i + 20) <= n)
                return base + i;
    return -1;
}

// Where the central directory is: (offset in the file, size, number of entries), false if it does not fit
auto Directory(Reader &s, int64_t eocd, int64_t &off, int64_t &size, int64_t &numE, int64_t &delta) -> bool
{
    uint8_t b[56];
    s.Seek(eocd);
    if (s.Read(b, 22) != 22)
        return false;
    numE = Le16(b, 10);
    size = Le32(b, 12);
    off = Le32(b, 16);
    delta = 0;
    if (numE == 0xFFFF || size == 0xFFFFFFFF || off == 0xFFFFFFFF)
    {
        // ZIP64: the locator is just before the record, it gives the position of the 64 bits record
        if (eocd < 20)
            return false;
        uint8_t loc[20];
        s.Seek(eocd - 20);
        if (s.Read(loc, 20) != 20 || Le32(loc, 0) != SigLoc64)
            return false;
        const int64_t z = Le64(loc, 8);
        if (z < 0 || Add64(z, 56) > s.Size())
            return false;
        s.Seek(z);
        if (s.Read(b, 56) != 56 || Le32(b, 0) != SigEocd64)
            return false;
        numE = Le64(b, 32);
        size = Le64(b, 40);
        off = Le64(b, 48);
        // data in front of a ZIP64 archive: the directory ends where the 64 bits record starts
        delta = Add64(z, -Add64(off, size));
    }
    else
        delta = Add64(eocd, -Add64(off, size));
    if (delta < 0 || size < 0 || numE < 0 || off < 0)
        return false;
    off = Add64(off, delta);
    return Add64(off, size) <= s.Size() &&
           static_cast<int64_t>(static_cast<uint64_t>(numE) * 46) <= size;
}

class ZipArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        const int64_t e = FindEocd(stream);
        if (e < 0)
            return false;
        int64_t off, size, numE, delta;
        if (!Directory(stream, e, off, size, numE, delta))
            return false;
        if (numE == 0)
            return true;
        stream.Seek(off);
        uint8_t sig[4];
        return stream.Read(sig, 4) == 4 && Le32(sig, 0) == SigCdir;
    }

    void Parse() override
    {
        _formatId = "ZIP";
        _dirSeparator = "/";
        const int64_t e = FindEocd(*_stream);
        int64_t off, size, numE;
        if (e < 0 || !Directory(*_stream, e, off, size, numE, _delta))
            Fail(DUDI7_ERR_NOTFORMAT, "No central directory");
        if (size > MaxDir)
            Fail(DUDI7_ERR_NOTFORMAT, "Central directory too big");
        std::vector<uint8_t> dir(static_cast<size_t>(size) + 8, 0);
        _stream->Seek(off);
        if (size > 0)
            _stream->ReadBuffer(dir.data(), static_cast<size_t>(size));
        int64_t p = 0;
        for (int64_t i = 1; i <= numE; ++i)
        {
            if (p + 46 > size || Le32(dir.data(), p) != SigCdir)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid central directory");
            const int flags = Le16(dir.data(), p + 8);
            const int method = Le16(dir.data(), p + 10);
            int64_t csize = Le32(dir.data(), p + 20);
            int64_t usize = Le32(dir.data(), p + 24);
            const int nameLen = Le16(dir.data(), p + 28);
            const int extraLen = Le16(dir.data(), p + 30);
            const int commLen = Le16(dir.data(), p + 32);
            int64_t locOff = Le32(dir.data(), p + 42);
            if (p + 46 + nameLen + extraLen + commLen > size)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid central directory");
            // ZIP64 extra field: the values that are 0xFFFFFFFF, in this order
            int64_t xp = p + 46 + nameLen;
            const int64_t extraEnd = p + 46 + nameLen + extraLen;
            while (xp + 4 <= extraEnd)
            {
                const int xId = Le16(dir.data(), xp);
                const int xSz = Le16(dir.data(), xp + 2);
                if (xId == 1 && xp + 4 + xSz <= extraEnd)
                {
                    xp += 4;
                    if (usize == 0xFFFFFFFF && xp + 8 <= extraEnd)
                    {
                        usize = Le64(dir.data(), xp);
                        xp += 8;
                    }
                    if (csize == 0xFFFFFFFF && xp + 8 <= extraEnd)
                    {
                        csize = Le64(dir.data(), xp);
                        xp += 8;
                    }
                    if (locOff == 0xFFFFFFFF && xp + 8 <= extraEnd)
                        locOff = Le64(dir.data(), xp);
                    break;
                }
                xp += 4 + xSz;
            }
            const std::string name(reinterpret_cast<const char *>(&dir[static_cast<size_t>(p) + 46]),
                                   static_cast<size_t>(nameLen));
            p += 46 + nameLen + extraLen + commLen;
            if (name.empty() || name.back() == '/' || name.back() == '\\')
                continue; // a folder
            if (usize < 0 || csize < 0 || locOff < 0 || Add64(Add64(locOff, _delta), 30) > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid entry");
            // DataX: method + 1 << 16 for an encrypted entry; DataY: the packed size
            Add(name, Add64(locOff, _delta), usize, method | ((flags & 1) << 16), csize);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        const int method = static_cast<int>(entry.DataX & 0xFFFF);
        if ((entry.DataX >> 16) != 0)
        {
            LastError = "The entry is encrypted";
            return DUDI7_ERR_UNSUPPORTED;
        }
        _stream->Seek(entry.Offset);
        uint8_t h[30];
        if (_stream->Read(h, 30) != 30 || Le32(h, 0) != SigLocal)
        {
            LastError = "Invalid local header";
            return DUDI7_ERR_IO;
        }
        const int64_t dataPos = entry.Offset + 30 + Le16(h, 26) + Le16(h, 28);
        if (Add64(dataPos, entry.DataY) > _stream->Size())
        {
            LastError = "The data goes past the end of the file";
            return DUDI7_ERR_IO;
        }
        switch (method)
        {
        case 0: {
            // stored: a plain copy
            int64_t left = entry.Size;
            if (left > entry.DataY)
            {
                LastError = "Invalid stored entry";
                return DUDI7_ERR_IO;
            }
            std::vector<uint8_t> buf(65536);
            _stream->Seek(dataPos);
            while (left > 0)
            {
                const int64_t chunk = left < static_cast<int64_t>(buf.size()) ? left : static_cast<int64_t>(buf.size());
                if (_stream->Read(buf.data(), static_cast<size_t>(chunk)) != static_cast<size_t>(chunk))
                {
                    LastError = "Cannot read the data";
                    return DUDI7_ERR_IO;
                }
                const int32_t result = ExtractBytes(buf.data(), chunk, sink);
                if (result != DUDI7_OK)
                    return result;
                left -= chunk;
            }
            return DUDI7_OK;
        }
        case 8:
            return ExtractDeflate(dataPos, entry.DataY, entry.Size, sink);
        case 14: {
            // LZMA: version (2 bytes), size of the properties (2), properties (5), the stream without size
            if (entry.DataY < 9 || entry.Size > MaxLzma)
            {
                LastError = "Invalid LZMA entry";
                return DUDI7_ERR_IO;
            }
            std::vector<uint8_t> src(static_cast<size_t>(entry.DataY) + 1);
            _stream->Seek(dataPos);
            _stream->ReadBuffer(src.data(), static_cast<size_t>(entry.DataY));
            const int propsSize = src[2] | (src[3] << 8);
            if (propsSize != 5)
            {
                LastError = "Invalid LZMA properties";
                return DUDI7_ERR_IO;
            }
            // the decoder wants the header of "LZMA alone": 5 bytes of properties, 8 of size
            std::vector<uint8_t> buf(static_cast<size_t>(entry.DataY) - 4 + 8 + 1, 0);
            std::memcpy(buf.data(), &src[4], 5);
            std::memset(&buf[5], 0xFF, 8);
            std::memcpy(&buf[13], &src[9], static_cast<size_t>(entry.DataY) - 9);
            std::vector<uint8_t> dst(static_cast<size_t>(entry.Size) + 1);
            if (LzmaAloneDecode(buf.data(), static_cast<int>(13 + entry.DataY - 9), dst.data(), entry.Size) != LzmaOk)
            {
                LastError = "Cannot unpack the LZMA data";
                return DUDI7_ERR_IO;
            }
            return ExtractBytes(dst.data(), entry.Size, sink);
        }
        default:
            LastError = "Compression method " + std::to_string(method) + " is not supported";
            return DUDI7_ERR_UNSUPPORTED;
        }
    }

  private:
    int64_t _delta = 0; // size of the data in front of the archive
};
} // namespace

void RegisterZip()
{
    Register<ZipArchive>("*.ZIP;*.PK3;*.PK4;*.PK7;*.KPF;*.JAR;*.APK;*.LOVE;*.XPI",
                         "ZIP archive (*.ZIP;*.PK3;*.PK4;*.KPF;*.JAR)");
}
} // namespace Dud
