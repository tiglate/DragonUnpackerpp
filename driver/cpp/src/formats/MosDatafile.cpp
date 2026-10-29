// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// "MOS DATAFILE2.0" support: Enclave, The Chronicles of Riddick (Escape from Butcher Bay, Assault on Dark Athena)
// .XWC (sounds) and .XTC (textures), ported from drv_default.dpr of Dragon UnPACKer 5. No real file was
// available for this port: the reading follows v5 line by line.
// Chunks of 48 bytes (name 24 bytes, offset of the next chunk, 5 integers) chained from the start:
//   VERSION     integer 3 > 2 for the version 3 of the sounds
//   WAVEDATA    (version 3) the sounds: table at the offset [2] + [1], [3] entries: offset, size, name
//               (32 bits length, padded to 4 bytes); the Ogg data starts 36 (Enclave), 40 (Riddick) or 48 (Dark
//               Athena) bytes after the offset
//   STRINGS, WAVEDATA2 (earlier versions) names, and entries of 24 bytes (name offset, offset, size)
//   TEXTURES    the directory of textures, DXT1/3/5 given back as .dds files made from the mip maps
//   IMAGELIST   textures of Assault on Dark Athena

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxEntries = 1000000;
constexpr int MaxChunks = 10000;
constexpr size_t MaxName = 1024;

struct Chunk
{
    std::string Id;
    int32_t Next = 0;
    int32_t Data[5] = {};
};

auto ReadChunk(Reader &r) -> Chunk
{
    char id[24];
    r.ReadBuffer(id);
    Chunk c;
    c.Id = ZString(id);
    c.Next = r.I32();
    for (int32_t &d : c.Data)
        d = r.I32();
    return c;
}

// 32 bits length then the characters, cut at the first null as v5 did (the padding is based on what is left)
auto Get32(Reader &r) -> std::string
{
    const uint32_t length = r.U32();
    if (length > MaxName)
        throw Error(DUDI7_ERR_NOTFORMAT, "A name is too long");
    std::string raw(length, '\0');
    if (length > 0)
        r.ReadBuffer(raw.data(), length);
    return ZString(raw.data(), raw.size());
}

class MosArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[24];
        return stream.Size() >= 48 && stream.Read(id, 24) == 24 && ZString(id) == "MOS DATAFILE2.0";
    }

    void Parse() override
    {
        _stream->Seek(0);
        Chunk hdr = ReadChunk(*_stream);
        bool version3 = false;
        std::vector<uint8_t> strings, waveData2;
        uint32_t waveEntries = 0;
        int formatCheck = 0;
        for (int chunks = 0;; ++chunks)
        {
            if (chunks > MaxChunks)
                Fail(DUDI7_ERR_NOTFORMAT, "Too many chunks");
            _stream->Seek(hdr.Next);
            hdr = ReadChunk(*_stream);
            const int64_t afterHeader = _stream->Pos();
            if (hdr.Id == "VERSION")
                version3 = hdr.Data[3] >= 3;
            else if (hdr.Id == "WAVEDATA" && version3)
            {
                ReadSounds(hdr);
                _isWave = true;
            }
            else if (hdr.Id == "STRINGS" && !version3)
                strings = Block(hdr.Data[1], hdr.Data[2]);
            else if (hdr.Id == "WAVEDATA2" && !version3)
            {
                waveData2 = Block(hdr.Data[1], hdr.Data[2]);
                waveEntries = static_cast<uint32_t>(hdr.Data[3]);
            }
            else if (hdr.Id == "TEXTURES")
                formatCheck = ReadTextures(afterHeader);
            else if (hdr.Id == "IMAGELIST")
            {
                formatCheck = 512; // (not detected as Riddick)
                ReadImageList(afterHeader);
            }
            if (hdr.Next == 0)
                break;
        }
        if (!strings.empty() && !waveData2.empty() && waveEntries > 0 && !version3)
        {
            if (waveEntries > MaxEntries || static_cast<int64_t>(waveEntries) * 24 > static_cast<int64_t>(waveData2.size()))
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid sound table");
            for (uint32_t x = 0; x < waveEntries; ++x)
            {
                int32_t e[6];
                std::memcpy(e, waveData2.data() + static_cast<size_t>(x) * 24, 24);
                if (e[2] < 0 || static_cast<size_t>(e[2]) >= strings.size())
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid sound name");
                const std::string name = ZString(reinterpret_cast<const char *>(strings.data()) + e[2],
                                                 strings.size() - static_cast<size_t>(e[2]));
                CheckRange(e[3], e[4]);
                Add(name, e[3], e[4]);
            }
            _isWave = true;
        }
        if (_entries.empty())
            Fail(DUDI7_ERR_NOTFORMAT, "No sound and no texture");
        _riddick = !_isWave && formatCheck != 512;
        // the size of the .dds files is the one of the header and the mip maps
        for (Entry &e : _entries)
            if (e.DataX == 1 || e.DataX == 2)
            {
                std::vector<uint8_t> unused;
                int64_t size = 0;
                if (MakeDds(e, unused, &size))
                    e.Size = size;
            }
        _formatId = _isWave ? "MOSD" : (_riddick ? "MOSDr" : "MOSDe");
        _dirSeparator = "";
        _extractInternal = !_isWave;
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (_isWave || (entry.DataX != 1 && entry.DataX != 2))
            return Archive::Extract(index, sink);
        std::vector<uint8_t> dds;
        if (!MakeDds(entry, dds))
            return Archive::Extract(index, sink); // (odd entry: copied as is)
        return ExtractBytes(dds.data(), static_cast<int64_t>(dds.size()), sink);
    }

  private:
    void CheckRange(int64_t offset, int64_t size) const
    {
        if (offset < 0 || size < 0 || offset + size > _stream->Size() || _entries.size() >= MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
    }

    auto Block(int64_t offset, int64_t size) -> std::vector<uint8_t>
    {
        if (offset < 0 || size < 0 || offset + size > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "A block is outside of the archive");
        std::vector<uint8_t> data(static_cast<size_t>(size));
        _stream->Seek(offset);
        if (size > 0)
            _stream->ReadBuffer(data.data(), data.size());
        return data;
    }

    auto IsOgg(int64_t at) -> bool
    {
        char id[4];
        if (at < 0 || at + 4 > _stream->Size())
            return false;
        _stream->Seek(at);
        _stream->ReadBuffer(id);
        return ZString(id) == "OggS";
    }

    void ReadSounds(const Chunk &hdr)
    {
        int64_t next = static_cast<int64_t>(hdr.Data[2]) + hdr.Data[1];
        if (hdr.Data[3] < 0 || static_cast<uint32_t>(hdr.Data[3]) > MaxEntries)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid sound table");
        for (int32_t x = 0; x < hdr.Data[3]; ++x)
        {
            _stream->Seek(next);
            int32_t offset = _stream->I32();
            int32_t size = _stream->I32();
            std::string name = Get32(*_stream);
            next = _stream->Pos();
            if (name.size() % 4 != 0)
                next += 4 - static_cast<int64_t>(name.size() % 4);
            if (size >= 40)
            {
                if (IsOgg(static_cast<int64_t>(offset) + 36)) // Enclave
                {
                    name += ".ogg";
                    size -= 36;
                    offset += 36;
                }
                else if (size >= 44)
                {
                    if (IsOgg(static_cast<int64_t>(offset) + 40)) // Chronicles of Riddick
                    {
                        name += ".ogg";
                        size -= 40;
                        offset += 40;
                    }
                    else if (size >= 52 && IsOgg(static_cast<int64_t>(offset) + 48)) // Assault on Dark Athena
                    {
                        name += ".ogg";
                        size -= 48;
                        offset += 48;
                    }
                }
            }
            CheckRange(offset, size);
            Add(name, offset, size);
        }
    }

    // Header of the texture in the file: width, height, type, size of the data (and where the data starts)
    struct TextureHeader
    {
        int32_t Unk2 = 0, Width = 0, Height = 0, Type = 0, Size = 0;
    };

    auto ReadTextureHeader(int64_t at, bool riddick, TextureHeader &h) -> bool
    {
        if (at < 0 || at + 40 > _stream->Size())
            return false;
        _stream->Seek(at);
        _stream->I32();
        h.Unk2 = _stream->I32();
        _stream->I32();
        h.Width = _stream->I32();
        h.Height = _stream->I32();
        if (riddick)
            _stream->Skip(4);
        _stream->I32();
        h.Type = _stream->I32();
        h.Size = _stream->I32();
        return true;
    }

    static auto IsDxt(int32_t type) -> bool
    {
        return type == 0 || type == 2 || type == 4;
    }

    auto ReadTextures(int64_t afterHeader) -> int
    {
        _stream->Seek(afterHeader + 8);
        const int32_t dirOffset = _stream->I32();
        const int32_t dirNum = _stream->I32();
        const int64_t here = _stream->Pos();
        if (dirOffset < 0 || dirNum < 0 || static_cast<uint32_t>(dirNum) > MaxEntries || dirOffset + 56 > here)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid texture directory");
        _stream->Seek(dirOffset);
        const int32_t formatCheck = _stream->I32();
        const bool riddick = formatCheck != 512;
        const int64_t cacheSize = here - dirOffset - 24 - 56;
        if (cacheSize < 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid texture directory");
        std::vector<uint8_t> cache = Block(static_cast<int64_t>(dirOffset) + 56, cacheSize);
        Reader dir(std::move(cache));
        const int64_t entrySize = 28 + 34 + (riddick ? 4 : 0);
        int64_t next = 0;
        for (int32_t x = 0; x < dirNum; ++x)
        {
            dir.Seek(next);
            std::string name = Get32(dir);
            if (name.size() % 4 != 0)
                dir.Skip(4 - static_cast<int64_t>(name.size() % 4));
            next = dir.Pos() + entrySize;
            dir.U32();
            const int32_t mipMapNum = dir.I32();
            dir.Skip(8);
            const int32_t firstMipMapSize = dir.I32();
            dir.Skip(8);
            if (riddick)
                dir.Skip(4);
            dir.I32();
            const int32_t offset = dir.I32();
            // the first mip map
            if (offset < 0 || static_cast<int64_t>(offset) + 4 > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A texture is outside of the archive");
            _stream->Seek(offset);
            const int32_t first = _stream->I32();
            TextureHeader h;
            if (ReadTextureHeader(first, riddick, h) && h.Width > 0 && h.Height > 0 && IsDxt(h.Type))
                Add(name + ".dds", offset, firstMipMapSize, 1, mipMapNum);
            else
                Add(name, offset, firstMipMapSize, 0, 0);
        }
        return formatCheck;
    }

    void ReadImageList(int64_t afterHeader)
    {
        _stream->Seek(afterHeader + 4);
        const int32_t dirOffset = _stream->I32();
        if (dirOffset < 0 || dirOffset + 12 > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid image list");
        _stream->Seek(dirOffset);
        int32_t stop = _stream->I32();
        _stream->I32();
        int32_t nameLength = _stream->I32();
        while (stop == -1)
        {
            if (nameLength < 0 || static_cast<size_t>(nameLength) > MaxName || _entries.size() >= MaxEntries)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid image list");
            std::string raw(static_cast<size_t>(nameLength), '\0');
            if (nameLength > 0)
                _stream->ReadBuffer(raw.data(), raw.size());
            const std::string name = ZString(raw.data(), raw.size());
            if (nameLength % 4 > 0)
                _stream->Skip(4 - nameLength % 4);
            const int64_t mipMapOffset = _stream->Pos();
            const int32_t mipMapNum = _stream->I32();
            _stream->Skip(8);
            const int32_t firstMipMapSize = _stream->I32();
            const int32_t width = _stream->I32();
            const int32_t height = _stream->I32();
            _stream->I32();
            const int32_t type = _stream->I32();
            _stream->Skip(8);
            const int32_t textureOffset = _stream->I32();
            if (width > 0 && height > 0 && IsDxt(type))
                Add(name + ".dds", textureOffset, firstMipMapSize, 2, mipMapOffset);
            else
                Add(name, textureOffset, firstMipMapSize, 0, 0);
            _stream->Skip(4 * (static_cast<int64_t>(mipMapNum) - 1));
            _stream->Skip(18);
            stop = _stream->I32();
            _stream->I32();
            nameLength = _stream->I32();
        }
    }

    // The .dds file of a texture: the header and the mip maps (all but the last, whose offset is wrong)
    // (sizeOnly: only compute the size of that file, listing the archive)
    auto MakeDds(const Entry &entry, std::vector<uint8_t> &out, int64_t *sizeOnly = nullptr) -> bool
    {
        const bool darkAthena = entry.DataX == 2;
        int64_t mipMapNum = entry.DataY;
        std::vector<int32_t> offsets;
        if (darkAthena)
        {
            if (entry.DataY < 0 || entry.DataY + 44 > _stream->Size())
                return false;
            _stream->Seek(entry.DataY);
            mipMapNum = _stream->I32();
            _stream->Skip(36);
        }
        else
        {
            if (entry.Offset < 0 || entry.Offset > _stream->Size())
                return false;
            _stream->Seek(entry.Offset);
        }
        if (mipMapNum < 1 || mipMapNum > 32 || _stream->Pos() + mipMapNum * 4 > _stream->Size())
            return false;
        for (int64_t x = 0; x < mipMapNum; ++x)
            offsets.push_back(_stream->I32());
        TextureHeader h;
        if (!ReadTextureHeader(offsets[0], _riddick, h) || h.Width <= 0 || h.Height <= 0 || h.Size <= 0 || !IsDxt(h.Type))
            return false;
        // (v5 wrote the header for all the mip maps but the data of all but the last: here the header counts what is written)
        const int64_t levels = mipMapNum > 1 ? mipMapNum - 1 : 1;
        auto put = [&](uint32_t v) {
            for (int i = 0; i < 4; ++i)
                out.push_back(static_cast<uint8_t>(v >> (8 * i)));
        };
        if (sizeOnly)
            *sizeOnly = 128;
        out.assign({'D', 'D', 'S', ' '});
        put(124);
        put(0x1 | 0x2 | 0x4 | 0x1000 | 0x80000 | (levels > 1 ? 0x20000 : 0));
        put(static_cast<uint32_t>(h.Height));
        put(static_cast<uint32_t>(h.Width));
        put(static_cast<uint32_t>(h.Size));
        put(0);
        put(static_cast<uint32_t>(levels));
        for (int i = 0; i < 11; ++i)
            put(0);
        put(32);
        put(0x4);
        out.insert(out.end(), {'D', 'X', 'T', h.Type == 4 ? '5' : h.Type == 2 ? '3' : '1'});
        for (int i = 0; i < 5; ++i)
            put(0);
        put(0x1000 | (levels > 1 ? (0x8 | 0x400000) : 0));
        for (int i = 0; i < 4; ++i)
            put(0);
        for (int64_t x = 0; x < levels; ++x)
        {
            TextureHeader m;
            if (!ReadTextureHeader(offsets[static_cast<size_t>(x)], _riddick, m) || m.Size < 0)
                return false;
            int64_t dataAt = offsets[static_cast<size_t>(x)] + (_riddick ? 44 : 40);
            if (darkAthena)
                dataAt = m.Unk2 == 0 ? offsets[static_cast<size_t>(x)] + 36 : dataAt + 12;
            if (dataAt < 0 || dataAt + m.Size > _stream->Size())
                return false;
            if (sizeOnly)
            {
                *sizeOnly += m.Size;
                continue;
            }
            const size_t start = out.size();
            out.resize(start + static_cast<size_t>(m.Size));
            _stream->Seek(dataAt);
            if (m.Size > 0)
                _stream->ReadBuffer(out.data() + start, static_cast<size_t>(m.Size));
        }
        return true;
    }

    bool _isWave = false;
    bool _riddick = false;
};
} // namespace

void RegisterMosDatafile()
{
    Register<MosArchive>("*.XWC;*.XTC", "Enclave, The Chronicles of Riddick (*.XWC;*.XTC)");
}
} // namespace Dud
