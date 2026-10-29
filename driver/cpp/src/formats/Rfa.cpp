// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Battlefield 1942 .RFA support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Two flavors: the beta ("Refractor2 FlatArchive 1.1  " then the directory offset) and the
// retail one (directory offset in the first 32 bits, 1 in the second one). Directory:
// number of entries then, for each: name (32 bits length + chars), size, unpacked size,
// offset, 3 unknown. Retail files can be packed: segments with an LZO1X compressed block
// each: number of segments, then per segment (size, unpacked size, offset in the data),
// then the data.

#include "Registry.h"
#include "codecs/Lzo.h"

namespace Dud
{
namespace
{
constexpr std::string_view Id = "Refractor2 FlatArchive 1.1  ";
constexpr uint32_t MaxSegments = 100000;
constexpr uint32_t MaxUnpacked = 256 * 1024 * 1024;

auto IsBetaStream(Reader &stream) -> bool
{
    return StartsWith(stream, Id);
}

class RfaArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        if (IsBetaStream(stream))
            return true;
        stream.Seek(0);
        const uint32_t dirOffset = stream.U32();
        const uint32_t one = stream.U32();
        return one == 1 && dirOffset >= 8 && static_cast<int64_t>(dirOffset) + 4 <= stream.Size();
    }

    void Parse() override
    {
        const bool beta = IsBetaStream(*_stream);
        _stream->Seek(beta ? 28 : 0);
        const uint32_t dirOffset = _stream->U32();
        if (static_cast<int64_t>(dirOffset) + 4 > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory offset");
        _stream->Seek(dirOffset);
        const uint32_t count = _stream->U32();
        if (count > (_stream->Size() - dirOffset) / 28)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");

        _formatId = "RFA";
        _dirSeparator = "/";
        _extractInternal = !beta;
        for (uint32_t i = 1; i <= count; ++i)
        {
            const std::string name = _stream->LString32(255);
            uint32_t ent[6];
            _stream->ReadBuffer(ent, 24);
            if (static_cast<int64_t>(ent[2]) + ent[0] > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            if (beta)
                Add(name, ent[2], ent[0]);
            else if (ent[1] != ent[0])
                Add(name, ent[2], ent[1], 1, ent[0]);
            else
                Add(name, ent[2], ent[1], 0, ent[0]);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == 0)
            return Archive::Extract(index, sink);
        const int64_t base = entry.Offset;
        if (base + 4 > _stream->Size())
        {
            LastError = "Entry is outside of the file";
            return DUDI7_ERR_IO;
        }
        _stream->Seek(base);
        const uint32_t segments = _stream->U32();
        if (segments > MaxSegments || base + 4 + static_cast<int64_t>(segments) * 12 > _stream->Size())
        {
            LastError = "Invalid segment table";
            return DUDI7_ERR_IO;
        }
        std::vector<uint32_t> header(static_cast<size_t>(segments) * 3);
        if (segments > 0)
            _stream->ReadBuffer(header.data(), static_cast<size_t>(segments) * 12);
        for (size_t i = 0; i < segments; ++i)
        {
            const uint32_t packedSize = header[i * 3];
            const uint32_t unpackedSize = header[i * 3 + 1];
            const int64_t dataPos = base + static_cast<int64_t>(segments) * 12 + header[i * 3 + 2] + 4;
            if (unpackedSize > MaxUnpacked || packedSize > MaxUnpacked || dataPos + packedSize > _stream->Size())
            {
                LastError = "Invalid segment";
                return DUDI7_ERR_IO;
            }
            std::vector<uint8_t> src(static_cast<size_t>(packedSize) + 1);
            std::vector<uint8_t> dst(static_cast<size_t>(unpackedSize) + 1);
            _stream->Seek(dataPos);
            if (packedSize > 0)
                _stream->ReadBuffer(src.data(), packedSize);
            const int r = Lzo1xDecompress(src.data(), static_cast<int>(packedSize), dst.data(),
                                          static_cast<int>(unpackedSize));
            if (r != LzoOk)
            {
                LastError = "Cannot unpack the segment (error " + std::to_string(r) + ")";
                return DUDI7_ERR_IO;
            }
            const int32_t status = ExtractBytes(dst.data(), unpackedSize, sink);
            if (status != DUDI7_OK)
                return status;
        }
        return DUDI7_OK;
    }
};
} // namespace

void RegisterRfa()
{
    Register<RfaArchive>("*.RFA", "Refractor 2 Flat Archive (*.RFA)|Battlefield 1942 (*.RFA)");
}
} // namespace Dud
