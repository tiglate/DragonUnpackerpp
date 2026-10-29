// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Aliens vs. Predator (2010) .ASR support ("Asura" and the zlib compressed "AsuraZlb"), ported from
// drv_default.dpr of Dragon UnPACKer 5. "Asura   " then blocks (id 4 bytes, size including the 16 bytes header,
// 2 unknown integers) up to an empty block. The compressed variant is "AsuraZlb", 0, compressed size,
// unpacked size, then a zlib stream with the same file; the offsets of the entries are positions in the
// unpacked data (which is kept in memory).
//   ASTS  streaming sounds: count, then name (null terminated), size, absolute offset
//   RSFL  list of resources: count, then name (null terminated, padded to 4 bytes), offset, size, unknown
//   RSCF  one resource: 8 unknown bytes, size, name (null terminated, padded to 4 bytes), data
// No real file was available: where v5's reading looked inconsistent with its own notes (the padding of
// the names of ASTS, the base of the offsets of RSFL) it is tried first and the other reading is the fallback
// if its entries would be outside of the file.

#include <cstring>
#include <functional>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 1000000;
constexpr int64_t MaxUnpacked = 1500000000;

class AsuraArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[8];
        if (stream.Size() < 24 || stream.Read(id, 8) != 8)
            return false;
        if (std::memcmp(id, "Asura   ", 8) == 0)
            return true;
        if (std::memcmp(id, "AsuraZlb", 8) != 0)
            return false;
        const uint32_t zero = stream.U32();
        const uint32_t packed = stream.U32();
        stream.U32();
        return zero == 0 && static_cast<int64_t>(packed) + 20 <= stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(0);
        char id[8];
        _stream->ReadBuffer(id);
        _formatId = "Asura";
        _dirSeparator = "\\";
        if (std::memcmp(id, "AsuraZlb", 8) == 0)
        {
            _stream->U32();
            _stream->U32();
            const uint32_t unpacked = _stream->U32();
            if (unpacked < 8 || unpacked > MaxUnpacked)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid unpacked size");
            _formatId = "AsuraZ";
            _extractInternal = true;
            _data = std::make_unique<Reader>(Unpack(*_stream, 20, unpacked));
            char magic[8];
            _data->Seek(0);
            _data->ReadBuffer(magic);
            if (std::memcmp(magic, "Asura   ", 8) != 0)
                Fail(DUDI7_ERR_NOTFORMAT, "Not an Asura file");
            ReadBlocks(*_data, true);
        }
        else
        {
            _extractInternal = false;
            _stream->Seek(8);
            ReadBlocks(*_stream, false);
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        if (!_data)
            return Archive::Extract(index, sink);
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.Offset < 0 || entry.Offset + entry.Size > _data->Size())
        {
            LastError = "Entry is outside of the file";
            return DUDI7_ERR_IO;
        }
        std::vector<uint8_t> bytes(static_cast<size_t>(entry.Size) + 1);
        _data->Seek(entry.Offset);
        if (entry.Size > 0)
            _data->ReadBuffer(bytes.data(), static_cast<size_t>(entry.Size));
        return ExtractBytes(bytes.data(), entry.Size, sink);
    }

  private:
    struct Pending
    {
        std::string Name;
        int64_t Offset = 0;
        int64_t Size = 0;
        int64_t X = 0;
    };

    // A null terminated name followed by the padding added by the format
    static auto Name(Reader &r, int extraPadding) -> std::string
    {
        const std::string name = r.CString(1024);
        int pad = 3 - static_cast<int>(name.size() % 4) + extraPadding;
        if (pad > 0)
            r.Skip(pad);
        return name;
    }

    void ReadBlocks(Reader &r, bool compressed)
    {
        const int64_t total = r.Size();
        int64_t cur;
        uint32_t blockSize;
        char blockId[4];
        do
        {
            cur = r.Pos();
            if (cur + 16 > total)
                break;
            r.ReadBuffer(blockId);
            blockSize = r.U32();
            r.U32();
            r.U32();
            if (std::memcmp(blockId, "ASTS", 4) == 0)
                ReadSounds(r, total, cur);
            else if (std::memcmp(blockId, "RSFL", 4) == 0)
                ReadList(r, total, cur, blockSize, compressed);
            else if (std::memcmp(blockId, "RSCF", 4) == 0)
            {
                r.Skip(8);
                const uint32_t size = r.U32();
                std::string name = Name(r, 0);
                const int64_t offset = r.Pos();
                if (!name.empty() && name[0] == '\\')
                    name.erase(0, 1);
                Push(name, offset, size, 0, compressed ? 1 : 0, total);
            }
            r.Seek(cur + blockSize);
        } while (std::memcmp(blockId, "\0\0\0\0", 4) != 0 && blockSize != 0 &&
                 static_cast<int64_t>(blockSize) + cur + 16 < total);
    }

    void Push(const std::string &name, int64_t offset, int64_t size, int64_t x, int64_t y, int64_t total)
    {
        if (offset < 0 || offset + size > total || _entries.size() >= MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
        Add(name, offset, size, x, y);
    }

    // Reads the entries of a block with a reading, tries the next one if an entry would be outside of the file
    void ReadSounds(Reader &r, int64_t total, int64_t)
    {
        const uint32_t count = r.U32();
        if (count > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
        const int64_t start = r.Pos();
        for (int extra = 1; extra >= 0; --extra)
        {
            r.Seek(start);
            std::vector<Pending> list;
            bool ok = true;
            for (uint32_t x = 0; x < count && ok; ++x)
            {
                Pending p;
                try
                {
                    p.Name = Name(r, extra);
                    p.Size = r.U32();
                    p.Offset = r.U32();
                }
                catch (const Error &)
                {
                    ok = false;
                    break;
                }
                catch (const ReadError &)
                {
                    ok = false;
                    break;
                }
                p.X = 4 - static_cast<int64_t>(p.Name.size() % 4);
                ok = p.Offset + p.Size <= total;
                list.push_back(p);
            }
            if (ok)
            {
                for (const Pending &p : list)
                    Push(p.Name, p.Offset, p.Size, p.X, 0, total);
                return;
            }
        }
        Fail(DUDI7_ERR_NOTFORMAT, "Invalid sound list");
    }

    void ReadList(Reader &r, int64_t total, int64_t blockStart, uint32_t blockSize, bool compressed)
    {
        const uint32_t count = r.U32();
        if (count > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Too many entries");
        const int64_t start = r.Pos();
        // the bases of the offsets: the size of the block (v5), the start of the block (notes of v5)
        const int64_t bases[2] = {static_cast<int64_t>(blockSize), blockStart};
        for (const int64_t base : bases)
        {
            r.Seek(start);
            std::vector<Pending> list;
            bool ok = true;
            for (uint32_t x = 0; x < count && ok; ++x)
            {
                Pending p;
                try
                {
                    p.Name = Name(r, 0);
                    p.Offset = static_cast<int64_t>(r.U32()) + base;
                    p.Size = r.U32();
                    p.X = r.I32();
                }
                catch (const Error &)
                {
                    ok = false;
                    break;
                }
                catch (const ReadError &)
                {
                    ok = false;
                    break;
                }
                ok = p.Offset + p.Size <= total;
                list.push_back(p);
            }
            if (ok)
            {
                for (const Pending &p : list)
                    Push(p.Name, p.Offset, p.Size, p.X, compressed ? 1 : 0, total);
                return;
            }
        }
        Fail(DUDI7_ERR_NOTFORMAT, "Invalid resource list");
    }

    std::unique_ptr<Reader> _data;
};
} // namespace

void RegisterAsura()
{
    Register<AsuraArchive>("*.ASR;*.STM", "Aliens vs. Predator 2010 (*.ASR;*.STM)");
}
} // namespace Dud
