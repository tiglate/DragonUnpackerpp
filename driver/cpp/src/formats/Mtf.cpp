// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Darkstone .MTF support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Number of entries (32 bits) then for each: name (32 bits length + chars), offset, size
// (the unpacked size). A file starts either with raw data or with a 12 bytes header
// (ID1 = 174 or 175, ID2 = 190, 2 reserved bytes, number of blocks, flags) followed by
// LZ data: a control byte (8 items, least significant bit first) then, for each item,
// a literal byte (bit set) or a 16 bits word: copy count - 3 on the 6 upper bits, distance
// on the 10 lower bits. No signature: only tried on .MTF files.

#include <array>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t MaxUnpacked = 512 * 1024 * 1024;

class MtfArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        const uint32_t count = stream.U32();
        if (!(count >= 1 && count < 65536 && stream.Size() > 4 + static_cast<int64_t>(count) * 12))
            return false;
        const uint32_t length = stream.U32();
        return length > 0 && length < 256;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint32_t count = _stream->U32();
        _formatId = "MTF";
        _dirSeparator = "\\";
        _extractInternal = true;
        for (uint32_t i = 1; i <= count; ++i)
        {
            const std::string name = _stream->LString32(255);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            if (name.empty() || offset > _stream->Size() || size > MaxUnpacked)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid entry");
            Add(name, offset, size);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        const int64_t final = entry.Size;
        if (entry.Offset + 12 > _stream->Size())
            return Archive::Extract(index, sink);
        _stream->Seek(entry.Offset);
        uint8_t header[12];
        _stream->ReadBuffer(header, 12);
        if (!(header[1] == 190 && (header[0] == 174 || header[0] == 175)))
            return Archive::Extract(index, sink);

        std::vector<uint8_t> res(static_cast<size_t>(final) + 8);
        std::array<uint8_t, 16384> buffer;
        size_t bufferLength = 0;
        size_t bufferPos = 0;
        bool failed = false;
        const auto nextByte = [&]() -> int {
            if (bufferPos >= bufferLength)
            {
                bufferLength = _stream->Read(buffer.data(), buffer.size());
                bufferPos = 0;
                if (bufferLength == 0)
                {
                    failed = true;
                    return 0;
                }
            }
            return buffer[bufferPos++];
        };

        int64_t j = 0;
        while (j < final && !failed)
        {
            const int control = nextByte();
            for (int x = 0; x < 8; ++x)
            {
                if (j >= final || failed)
                    break;
                if (((control >> x) & 1) == 1)
                {
                    res[static_cast<size_t>(j)] = static_cast<uint8_t>(nextByte());
                    ++j;
                }
                else
                {
                    int count = nextByte();
                    count += 256 * nextByte();
                    const int distance = count % 1024;
                    count = count / 1024 + 3;
                    if (distance == 0 || distance > j)
                    {
                        failed = true;
                        break;
                    }
                    while (count > 0 && j < final)
                    {
                        res[static_cast<size_t>(j)] = res[static_cast<size_t>(j - distance)];
                        ++j;
                        --count;
                    }
                }
            }
        }
        if (failed)
        {
            LastError = "Invalid or truncated compressed data";
            return DUDI7_ERR_IO;
        }
        return ExtractBytes(res.data(), final, sink);
    }
};
} // namespace

void RegisterMtf()
{
    Register<MtfArchive>("*.MTF", "Darkstone Archive (*.MTF)");
}
} // namespace Dud
