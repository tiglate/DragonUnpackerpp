// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Ghostbusters: Sanctum of Slime .PAK ("tongas_pack_v30000") support, ported from
// drv_default.dpr of Dragon UnPACKer 5.
// Header: 18 bytes id, number of entries, size of the directory (which follows). Entries:
// offset, size (packed size for the compressed ones), compressed flag, name (32 bits length +
// chars). Compressed files are LZMA streams ("LZMA alone": properties, unpacked size,
// data). Unlike Dragon UnPACKer 5 the unpacked size is read from the stream header
// to list the real size.

#include "Registry.h"
#include "codecs/Lzma.h"

namespace Dud
{
namespace
{
constexpr std::string_view Id = "tongas_pack_v30000";
constexpr int64_t Header = 26;
constexpr int64_t MaxUnpacked = 1024 * 1024 * 1024;

class TongaArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        return StartsWith(stream, Id);
    }

    void Parse() override
    {
        _stream->Seek(18);
        const uint32_t count = _stream->U32();
        const uint32_t dirSize = _stream->U32();
        CheckTable(Header, 1, dirSize);
        if (count > dirSize / 13)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        std::vector<uint8_t> data(dirSize);
        if (dirSize > 0)
            _stream->ReadBuffer(data.data(), dirSize);
        Reader dir(std::move(data));

        _formatId = "tonga";
        _dirSeparator = "\\";
        _extractInternal = true;
        for (uint32_t i = 1; i <= count; ++i)
        {
            const uint32_t offset = dir.U32();
            const uint32_t size = dir.U32();
            const uint32_t flag = dir.U32();
            const std::string name = dir.LString32(1024);
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            if (flag == 1 && size >= 13)
            {
                uint8_t header[13];
                _stream->Seek(offset);
                _stream->ReadBuffer(header, 13);
                const int64_t unpacked = LzmaAloneSize(header, 13);
                if (unpacked < 0 || unpacked > MaxUnpacked)
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid LZMA header");
                Add(name, offset, unpacked, 1, size);
            }
            else
                Add(name, offset, size, 0, size);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == 0)
            return Archive::Extract(index, sink);
        std::vector<uint8_t> src(static_cast<size_t>(entry.DataY) + 1);
        std::vector<uint8_t> dst(static_cast<size_t>(entry.Size) + 1);
        _stream->Seek(entry.Offset);
        _stream->ReadBuffer(src.data(), static_cast<size_t>(entry.DataY));
        const int r = LzmaAloneDecode(src.data(), static_cast<int>(entry.DataY), dst.data(), entry.Size);
        if (r != LzmaOk)
        {
            LastError = "Cannot unpack the file (LZMA error " + std::to_string(r) + ")";
            return DUDI7_ERR_IO;
        }
        return ExtractBytes(dst.data(), entry.Size, sink);
    }
};
} // namespace

void RegisterTonga()
{
    Register<TongaArchive>("*.PAK", "Ghostbusters: Sanctum of Slime Archive (*.PAK)");
}
} // namespace Dud
