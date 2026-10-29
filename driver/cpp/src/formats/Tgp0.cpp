// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Astebreed / Ether Vapor Remaster / Fairy Bloom Freesia .TGP (TGP0) support, ported from
// drv_default.dpr of Dragon UnPACKer 5.
// Header: "TGP0", version (16 bits, 1 or 2). Version 1: unused (16 bits), flags (32 bits;
// 1 = data xored with $46), number of entries; entries of 112 bytes: name[96], offset
// (from the start of the file), unknown, size, unknown. Version 2: flags (16 bits; 1 =
// xored with $46, 2 = the whole data block is a gzip stream), number of entries,
// unpacked size of the data block; entries of 112 bytes: name[96], offset (from the data
// block, which follows the entries), size, 2 unknown.

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t HeaderSize = 16;
constexpr int64_t EntrySize = 112;
constexpr int64_t MaxBlock = 768 * 1024 * 1024;

class Tgp0Archive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        if (!StartsWith(stream, "TGP0"))
            return false;
        uint16_t version = 0;
        return stream.Read(&version, 2) == 2 && (version == 1 || version == 2);
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint16_t version = _stream->U16();
        uint32_t count;
        int64_t base;
        if (version == 1)
        {
            _stream->U16();
            _flags = _stream->U32();
            count = _stream->U32();
            base = 0;
        }
        else
        {
            _flags = _stream->U16();
            count = _stream->U32();
            _unpackedSize = _stream->U32();
            base = HeaderSize + static_cast<int64_t>(count) * EntrySize;
        }
        CheckTable(HeaderSize, count, EntrySize);

        _formatId = "TGP0";
        _dirSeparator = "\\";
        _extractInternal = (_flags & 3) != 0;
        for (uint32_t i = 1; i <= count; ++i)
        {
            uint8_t ent[EntrySize];
            _stream->ReadBuffer(ent, EntrySize);
            uint32_t offset = 0, size = 0;
            std::memcpy(&offset, ent + 96, 4);
            std::memcpy(&size, ent + (version == 1 ? 104 : 100), 4);
            if (version == 1 || (_flags & 2) == 0)
            {
                if (base + offset + size > _stream->Size())
                    Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            }
            else if (static_cast<int64_t>(offset) + size > _unpackedSize)
            {
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the data block");
            }
            const std::string name = ZString(reinterpret_cast<const char *>(ent), 96);
            if (version == 2 && (_flags & 2) == 0)
                Add(name, base + offset, size, _flags);
            else
                Add(name, offset, size, _flags);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if ((_flags & 2) != 0)
        {
            if (!LoadBlock())
            {
                LastError = "Cannot unpack the data block";
                return DUDI7_ERR_IO;
            }
            if (entry.Offset + entry.Size > static_cast<int64_t>(_block.size()))
            {
                LastError = "Entry is outside of the data block";
                return DUDI7_ERR_IO;
            }
            if (entry.Size == 0)
                return DUDI7_OK;
            return ExtractBytes(_block.data() + entry.Offset, entry.Size, sink);
        }
        if ((_flags & 1) == 0)
            return Archive::Extract(index, sink);
        if (entry.Offset + entry.Size > _stream->Size())
        {
            LastError = "Entry is outside of the file";
            return DUDI7_ERR_IO;
        }
        std::vector<uint8_t> buffer(static_cast<size_t>(entry.Size) + 1);
        _stream->Seek(entry.Offset);
        if (entry.Size > 0)
            _stream->ReadBuffer(buffer.data(), static_cast<size_t>(entry.Size));
        for (int64_t i = 0; i < entry.Size; ++i)
            buffer[static_cast<size_t>(i)] ^= 0x46;
        return ExtractBytes(buffer.data(), entry.Size, sink);
    }

  private:
    // Unpacks the gzip stream that makes the data block (version 2, flag 2)
    auto LoadBlock() -> bool
    {
        if (_blockLoaded)
            return !_block.empty();
        _blockLoaded = true;
        if (_unpackedSize <= 0 || _unpackedSize > MaxBlock)
            return false;
        _stream->Seek(8);
        const uint32_t count = _stream->U32();
        const int64_t pos = HeaderSize + static_cast<int64_t>(static_cast<uint32_t>(count * EntrySize));
        if (pos + 18 > _stream->Size())
            return false;
        _stream->Seek(pos);
        uint8_t header[10];
        _stream->ReadBuffer(header, 10);
        if (header[0] != 0x1F || header[1] != 0x8B || header[2] != 8)
            return false;
        const uint32_t flags = header[3];
        if ((flags & 4) != 0)
        {
            const uint32_t extra = _stream->U16();
            _stream->Skip(extra);
        }
        for (const uint32_t terminated : {8u, 16u})
        {
            if ((flags & terminated) == 0)
                continue;
            uint8_t c = 0;
            do
            {
                if (_stream->Read(&c, 1) != 1)
                    return false;
            } while (c != 0);
        }
        if ((flags & 2) != 0)
            _stream->Skip(2);
        if (_stream->Pos() >= _stream->Size())
            return false;
        try
        {
            _block = Unpack(*_stream, _stream->Pos(), _unpackedSize, true);
        }
        catch (const Error &)
        {
            _block.clear();
            return false;
        }
        return true;
    }

    uint32_t _flags = 0;
    int64_t _unpackedSize = 0;
    std::vector<uint8_t> _block;
    bool _blockLoaded = false;
};
} // namespace

void RegisterTgp0()
{
    Register<Tgp0Archive>("*.TGP", "Astebreed Archive (*.TGP)|Ether Vapor Remaster (*.TGP)|Fairy Bloom Freesia (*.TGP)");
}
} // namespace Dud
