// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Archive.h"

#include <algorithm>
#include <array>

#include <zlib.h>

namespace Dud
{
void Archive::Add(const std::string &name, int64_t offset, int64_t size, int64_t dataX, int64_t dataY)
{
    _entries.push_back({EnsureUtf8(name), offset, size, dataX, dataY});
}

void Archive::Fail(int32_t code, const std::string &message)
{
    throw Error(code, message);
}

void Archive::CheckTable(int64_t offset, int64_t count, int64_t entrySize) const
{
    if (offset < 0 || count < 0 || offset + count * entrySize > _stream->Size())
        Fail(DUDI7_ERR_NOTFORMAT, "The directory is outside of the file");
}

void Archive::DecodeChunk(int64_t, uint8_t *, int64_t, int64_t)
{
}

auto Archive::ExtractBytes(const void *data, int64_t length, const Sink &sink) -> int32_t
{
    const auto *p = static_cast<const uint8_t *>(data);
    while (length > 0)
    {
        const int64_t chunk = std::min<int64_t>(length, 65536);
        if (!sink.Write(p, chunk))
        {
            LastError = "Extraction stopped by the host";
            return DUDI7_ERR_CANCELLED;
        }
        p += chunk;
        length -= chunk;
    }
    return DUDI7_OK;
}

auto Archive::Inflate(int64_t offset, int64_t packedSize, int64_t unpackedSize, bool raw, const Sink &sink) -> int32_t
{
    if (offset < 0 || packedSize < 0 || unpackedSize < 0 || offset + packedSize > _stream->Size())
    {
        LastError = "Invalid compressed entry";
        return DUDI7_ERR_IO;
    }
    std::vector<uint8_t> packed(static_cast<size_t>(packedSize));
    _stream->Seek(offset);
    if (packedSize > 0)
        _stream->ReadBuffer(packed.data(), packed.size());

    z_stream zs{};
    if (inflateInit2(&zs, raw ? -15 : 15) != Z_OK)
    {
        LastError = "Cannot unpack the file: zlib initialisation failed";
        return DUDI7_ERR_IO;
    }
    struct Guard
    {
        z_stream *Z;
        ~Guard()
        {
            inflateEnd(Z);
        }
    } guard{&zs};

    zs.next_in = packed.data();
    zs.avail_in = static_cast<uInt>(packed.size());
    std::array<uint8_t, 65536> buffer;
    int64_t left = unpackedSize;
    while (left > 0)
    {
        const auto want = static_cast<uInt>(std::min<int64_t>(left, buffer.size()));
        zs.next_out = buffer.data();
        zs.avail_out = want;
        const int rc = inflate(&zs, Z_NO_FLUSH);
        const uInt got = want - zs.avail_out;
        if (rc != Z_OK && rc != Z_STREAM_END && !(rc == Z_BUF_ERROR && got > 0))
        {
            if (rc == Z_BUF_ERROR)
                LastError = "Unexpected end of the compressed data";
            else
                LastError = std::string("Cannot unpack the file: ") + (zs.msg ? zs.msg : "data error");
            return DUDI7_ERR_IO;
        }
        if (got == 0)
        {
            LastError = "Unexpected end of the compressed data";
            return DUDI7_ERR_IO;
        }
        if (!sink.Write(buffer.data(), got))
        {
            LastError = "Extraction stopped by the host";
            return DUDI7_ERR_CANCELLED;
        }
        left -= got;
    }
    return DUDI7_OK;
}

auto Archive::Unpack(Reader &source, int64_t offset, int64_t unpackedSize, bool raw) -> std::vector<uint8_t>
{
    std::vector<uint8_t> result(static_cast<size_t>(unpackedSize));
    z_stream zs{};
    if (inflateInit2(&zs, raw ? -15 : 15) != Z_OK)
        throw Error(DUDI7_ERR_NOTFORMAT, "Cannot unpack: zlib initialisation failed");
    struct Guard
    {
        z_stream *Z;
        ~Guard()
        {
            inflateEnd(Z);
        }
    } guard{&zs};

    source.Seek(offset);
    std::array<uint8_t, 65536> input;
    zs.next_out = result.data();
    zs.avail_out = static_cast<uInt>(result.size());
    while (zs.avail_out > 0)
    {
        if (zs.avail_in == 0)
        {
            zs.next_in = input.data();
            zs.avail_in = static_cast<uInt>(source.Read(input.data(), input.size()));
            if (zs.avail_in == 0)
                throw Error(DUDI7_ERR_NOTFORMAT, "Cannot unpack: unexpected end of the compressed data");
        }
        const int rc = inflate(&zs, Z_NO_FLUSH);
        if (rc == Z_STREAM_END)
            break;
        if (rc != Z_OK)
            throw Error(DUDI7_ERR_NOTFORMAT, std::string("Cannot unpack: ") + (zs.msg ? zs.msg : "data error"));
    }
    if (zs.avail_out > 0)
        throw Error(DUDI7_ERR_NOTFORMAT, "Cannot unpack: unexpected end of the compressed data");
    return result;
}

auto Archive::ExtractZlib(int64_t offset, int64_t packedSize, int64_t unpackedSize, const Sink &sink) -> int32_t
{
    return Inflate(offset, packedSize, unpackedSize, false, sink);
}

auto Archive::ExtractDeflate(int64_t offset, int64_t packedSize, int64_t unpackedSize, const Sink &sink) -> int32_t
{
    return Inflate(offset, packedSize, unpackedSize, true, sink);
}

auto Archive::Extract(int64_t index, const Sink &sink) -> int32_t
{
    const Entry &entry = _entries[static_cast<size_t>(index)];
    int64_t left = entry.Size;
    if (left < 0 || entry.Offset < 0 || entry.Offset + left > _stream->Size())
    {
        LastError = "Entry is outside of the file";
        return DUDI7_ERR_IO;
    }
    _stream->Seek(entry.Offset);
    std::array<uint8_t, 65536> buffer;
    int64_t done = 0;
    while (left > 0)
    {
        const int64_t chunk = std::min<int64_t>(left, buffer.size());
        if (_stream->Read(buffer.data(), static_cast<size_t>(chunk)) != static_cast<size_t>(chunk))
        {
            LastError = "Unexpected end of file";
            return DUDI7_ERR_IO;
        }
        DecodeChunk(index, buffer.data(), chunk, done);
        if (!sink.Write(buffer.data(), chunk))
        {
            LastError = "Extraction stopped by the host";
            return DUDI7_ERR_CANCELLED;
        }
        done += chunk;
        left -= chunk;
    }
    return DUDI7_OK;
}
} // namespace Dud
