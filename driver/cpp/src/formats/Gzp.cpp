// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Giants: Citizen Kabuto .GZP support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (24 bytes): id $6608F101, offset of the directory, 4 unknown. At the directory
// offset + 4: number of entries, then for each: size, unpacked size, date, start of the
// block (the data is 16 bytes further), flag (1 = compressed, 2 = stored), name length,
// name. Compressed data is LZSS (4096 bytes window, initial position $FEE).

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t Id = 0x6608F101;
constexpr int64_t MaxUnpacked = 512 * 1024 * 1024;

class GzpArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t id = 0;
        return stream.Read(&id, 4) == 4 && id == Id;
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t dirOffset = _stream->U32();
        if (static_cast<int64_t>(dirOffset) + 8 > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory offset");
        _stream->Seek(static_cast<int64_t>(dirOffset) + 4);
        const uint32_t count = _stream->U32();
        if (count > static_cast<uint64_t>(_stream->Size()) / 19)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");

        _formatId = "GZP";
        _dirSeparator = "";
        _extractInternal = true;
        for (uint32_t i = 1; i <= count; ++i)
        {
            const uint32_t size = _stream->U32();
            const uint32_t unpacked = _stream->U32();
            _stream->U32();
            const uint32_t start = _stream->U32();
            uint8_t flag = 0;
            uint8_t nameLength = 0;
            _stream->ReadBuffer(&flag, 1);
            _stream->ReadBuffer(&nameLength, 1);
            std::string name(nameLength, '\0');
            if (nameLength > 0)
                _stream->ReadBuffer(name.data(), nameLength);
            if (static_cast<int64_t>(start) + 16 + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            if (flag == 1)
                Add(ZString(name.data(), nameLength), static_cast<int64_t>(start) + 16, unpacked, size, 1);
            else
                Add(ZString(name.data(), nameLength), static_cast<int64_t>(start) + 16, size, size, 0);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataY == 0)
            return Archive::Extract(index, sink);
        const int64_t packedLength = entry.DataX;
        const int64_t final = entry.Size;
        if (final > MaxUnpacked || entry.Offset + packedLength > _stream->Size())
        {
            LastError = "Invalid compressed entry";
            return DUDI7_ERR_IO;
        }
        std::vector<uint8_t> src(static_cast<size_t>(packedLength) + 2);
        std::vector<uint8_t> res(static_cast<size_t>(final) + 1);
        _stream->Seek(entry.Offset);
        if (packedLength > 0)
            _stream->ReadBuffer(src.data(), static_cast<size_t>(packedLength));
        int64_t i = 0;
        int64_t j = 0;
        int64_t bits = 8;
        uint8_t control = 0;
        while (j < final)
        {
            if (bits == 8)
            {
                if (i >= packedLength)
                    break;
                control = src[static_cast<size_t>(i++)];
                bits = 0;
            }
            if (((control >> bits) & 1) == 0)
            {
                if (i + 1 >= packedLength)
                    break;
                const int64_t b0 = src[static_cast<size_t>(i)];
                const int64_t b1 = src[static_cast<size_t>(i) + 1];
                int64_t position = ((b0 + ((b1 & 0xF0) << 4) - 0xFEE - j) & 0xFFF) - 0x1000 + j;
                int64_t length = (b1 & 0xF) + 3;
                i += 2;
                while (length > 0 && j < final)
                {
                    res[static_cast<size_t>(j)] = position >= 0 ? res[static_cast<size_t>(position)] : 32;
                    ++j;
                    ++position;
                    --length;
                }
            }
            else
            {
                if (i >= packedLength)
                    break;
                res[static_cast<size_t>(j++)] = src[static_cast<size_t>(i++)];
            }
            ++bits;
        }
        if (j < final)
        {
            LastError = "The compressed data is truncated";
            return DUDI7_ERR_IO;
        }
        return ExtractBytes(res.data(), final, sink);
    }
};
} // namespace

void RegisterGzp()
{
    Register<GzpArchive>("*.GZP", "Giants: Citizen Kabuto Archive (*.GZP)");
}
} // namespace Dud
