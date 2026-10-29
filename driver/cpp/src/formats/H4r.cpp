// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Heroes of Might and Magic 4 .H4R support. v5 had a reader disabled (EXPFORMATS) that did not get the whole
// directory right; this one was worked out from heroes4.h4r of the GOG release (17,523 entries).
// Header: "H4R" 5, offset of the directory. Directory: number of entries, then per entry offset, packed size,
// size, a hash (32 bits each), three strings (16 bit length: name, folder of the author's machine, alias) and a
// 32 bits method: 3 = gzip (the data starts with 1F 8B), 1 = stored. An entry without packed size has offset 0
// and no data here (heroes4.h4r: layers.new_game.h4d): it is listed as empty.

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 1000000;

class H4rArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 12)
            return false;
        uint8_t id[4];
        stream.ReadBuffer(id, 4);
        const uint32_t dir = stream.U32();
        return id[0] == 'H' && id[1] == '4' && id[2] == 'R' && id[3] == 5 && dir >= 8 && dir + 4 <= stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(4);
        const uint32_t dir = _stream->U32();
        _stream->Seek(dir);
        const uint32_t count = _stream->U32();
        if (count > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
        _formatId = "H4R";
        _dirSeparator = "";
        _extractInternal = true;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t offset = _stream->U32();
            const uint32_t packed = _stream->U32();
            const uint32_t size = _stream->U32();
            _stream->U32();
            const std::string name = String16();
            String16();
            String16();
            const uint32_t method = _stream->U32();
            if (size != 0 && (static_cast<int64_t>(offset) + packed > _stream->Size()))
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            // (no data in this file for an entry without packed size: the update files hold it)
            Add(name, offset, packed == 0 ? 0 : size, method, packed);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.Size == 0)
            return ExtractBytes(nullptr, 0, sink);
        if (entry.DataX != 3)
        {
            Entry stored = entry;
            stored.Size = entry.DataY;
            return ExtractStored(stored, sink);
        }
        // gzip: skip the header and give the deflate stream to zlib
        int64_t pos = entry.Offset;
        uint8_t h[10];
        _stream->Seek(pos);
        if (entry.DataY < 18 || _stream->Read(h, 10) != 10 || h[0] != 0x1F || h[1] != 0x8B || h[2] != 8)
        {
            LastError = "Cannot unpack the file: not a gzip stream";
            return DUDI7_ERR_IO;
        }
        pos += 10;
        const uint8_t flags = h[3];
        if (flags & 4)
            pos += 2 + _stream->U16();
        auto skipString = [&]() {
            while (_stream->Pos() < _stream->Size() && _stream->U8() != 0)
            {
            }
        };
        _stream->Seek(pos);
        if (flags & 8)
            skipString();
        if (flags & 16)
            skipString();
        if (flags & 2)
            _stream->Skip(2);
        pos = _stream->Pos();
        const int64_t used = pos - entry.Offset;
        if (used + 8 > entry.DataY)
        {
            LastError = "Cannot unpack the file: not a gzip stream";
            return DUDI7_ERR_IO;
        }
        return ExtractDeflate(pos, entry.DataY - used - 8, entry.Size, sink);
    }

  private:
    auto String16() -> std::string
    {
        const uint16_t length = _stream->U16();
        std::string s(length, '\0');
        if (length > 0)
            _stream->ReadBuffer(s.data(), length);
        return s;
    }

    auto ExtractStored(const Entry &entry, const Sink &sink) -> int32_t
    {
        std::vector<uint8_t> data(static_cast<size_t>(entry.Size) + 1);
        _stream->Seek(entry.Offset);
        _stream->ReadBuffer(data.data(), static_cast<size_t>(entry.Size));
        return ExtractBytes(data.data(), entry.Size, sink);
    }
};
} // namespace

void RegisterH4r()
{
    Register<H4rArchive>("*.H4R", "Heroes of Might and Magic 4 (*.H4R)");
}
} // namespace Dud
