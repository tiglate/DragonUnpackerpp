// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Unity asset bundle ("UnityFS", versions 6 to 8) support. Not in Dragon UnPACKer 5: written
// from the layout of the format (checked on the bundles of Hollow Knight: Silksong,
// Blasphemous 2, Chained Echoes, Metal Unit, Skul, Ara Fell).
// Header: "UnityFS" + null, format version (32 bits, big endian), Unity version and revision
// (null terminated texts), size of the bundle (64 bits), size of the blocks info (packed and
// unpacked), flags (bits 0-5: compression of the blocks info, 0x40: the files directory is in
// the blocks info, 0x80: the blocks info is at the end of the file, 0x200: padding before
// the blocks). From version 7 (or flag 0x200) the position is aligned on 16 bytes after the
// header. Everything is big endian.
// Blocks info (unpacked): 16 bytes hash, number of blocks, for each: unpacked size, packed
// size, flags (bits 0-5: 0 none, 1 LZMA, 2 or 3 LZ4); number of files, for each: offset in
// the unpacked data (64 bits), size (64 bits), flags, path.
// The data is the concatenation of the unpacked blocks; the files are ranges of it.
// The serialized files of the bundle ("CAB-*", flag 4, or the .assets files of a player data archive) are read
// too (UnitySerial): the textures, sounds, texts and fonts they hold are added as
// "CAB-xxx.objects/<Class>/<name>" (textures as TGA or DDS, see UnityExport; sounds are the FSB banks of the
// .resource stream), next to the raw files. A serialized file that cannot be read is left raw.

#include <array>
#include <cstring>

#include "PathUtil.h"
#include "Registry.h"
#include "UnityObjects.h"
#include "codecs/Lz4.h"
#include "codecs/Lzma.h"

namespace Dud
{
namespace
{
constexpr uint32_t MaxInfo = 64 * 1024 * 1024;
constexpr uint32_t MaxBlock = 1024 * 1024 * 1024;
constexpr uint32_t MaxBlocks = 4000000;
constexpr uint32_t MaxFiles = 1000000;

struct Block
{
    int64_t UStart = 0; // start in the unpacked data
    uint32_t USize = 0;
    int64_t CPos = 0; // position of the packed data in the file
    uint32_t CSize = 0;
    uint16_t Flags = 0;
};

auto BE32(const uint8_t *b, int64_t pos) -> uint32_t
{
    const uint8_t *p = b + pos;
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

auto BE64(const uint8_t *b, int64_t pos) -> int64_t
{
    return static_cast<int64_t>((static_cast<uint64_t>(BE32(b, pos)) << 32) | BE32(b, pos + 4));
}

auto EndsText(const std::string &text, const char *suffix) -> bool
{
    const std::string s = ToLower(suffix);
    const std::string t = ToLower(text);
    return t.size() >= s.size() && t.compare(t.size() - s.size(), s.size(), s) == 0;
}

auto UnpackBlock(int method, const uint8_t *src, int srcLen, uint8_t *dst, int dstLen) -> bool
{
    switch (method)
    {
    case 0:
        if (srcLen < dstLen)
            return false;
        std::memcpy(dst, src, static_cast<size_t>(dstLen));
        return true;
    case 1: {
        // LZMA: 5 bytes of properties then the stream (no size): give it the "alone" header
        if (srcLen < 6)
            return false;
        std::vector<uint8_t> tmp(static_cast<size_t>(srcLen) + 8, 0);
        std::memcpy(tmp.data(), src, 5);
        std::memcpy(&tmp[13], src + 5, static_cast<size_t>(srcLen) - 5);
        return LzmaAloneDecode(tmp.data(), srcLen + 8, dst, dstLen) == LzmaOk;
    }
    case 2:
    case 3:
        return Lz4DecompressBlock(src, srcLen, dst, dstLen) == dstLen;
    default:
        return false;
    }
}

class UnityFsArchive : public UnityObjectArchive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[8];
        return stream.Read(id, 8) == 8 && std::memcmp(id, "UnityFS", 7) == 0 && id[7] == '\0';
    }

    void Parse() override
    {
        _stream->Seek(8);
        uint32_t version = 0;
        _stream->ReadBuffer(&version, 4);
        version = BE32(reinterpret_cast<const uint8_t *>(&version), 0);
        if (version < 6 || version > 8)
            Fail(DUDI7_ERR_UNSUPPORTED, "Unsupported UnityFS version " + std::to_string(version));
        // version and revision texts
        uint8_t hdr[256] = {};
        _stream->Read(hdr, sizeof hdr);
        uint32_t z = 0;
        while (z < 100 && hdr[z] != 0)
            ++z;
        uint32_t z2 = z + 1;
        while (z2 < 200 && hdr[z2] != 0)
            ++z2;
        if (z2 >= 200)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid Unity version");
        _engineVersion.assign(reinterpret_cast<const char *>(&hdr[z + 1]), z2 - z - 1);
        int64_t p = z2 + 1;
        const int64_t bundleSize = BE64(hdr, p);
        const uint32_t csize = BE32(hdr, p + 8);
        const uint32_t usize = BE32(hdr, p + 12);
        const uint32_t flags = BE32(hdr, p + 16);
        int64_t infoPos = 12 + p + 20; // 8 (signature) + 4 (version) + texts + fields
        if (version >= 7 || (flags & 0x200) != 0)
            infoPos = (infoPos + 15) & ~static_cast<int64_t>(15);
        int64_t dataPos = infoPos;
        if ((flags & 0x80) != 0)
            infoPos = _stream->Size() - csize;
        else
            dataPos = infoPos + csize;
        // The data is aligned on 16 bytes after the blocks info in the bundles of some Unity versions
        // (flag $200, version 7) and not in the others: both are tried below
        const int64_t altPos = dataPos;
        if (version >= 7 || (flags & 0x200) != 0)
            dataPos = (dataPos + 15) & ~static_cast<int64_t>(15);
        if (csize > MaxInfo || usize > MaxInfo || infoPos < 0 || infoPos + csize > _stream->Size() || bundleSize < 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid blocks info");
        std::vector<uint8_t> packed(static_cast<size_t>(csize) + 1);
        std::vector<uint8_t> info(static_cast<size_t>(usize) + 16);
        _stream->Seek(infoPos);
        if (csize > 0)
            _stream->ReadBuffer(packed.data(), csize);
        if (!UnpackBlock(static_cast<int>(flags & 0x3F), packed.data(), static_cast<int>(csize), info.data(),
                    static_cast<int>(usize)))
            Fail(DUDI7_ERR_NOTFORMAT, "Cannot unpack the blocks info");
        // blocks
        if (usize < 20)
            Fail(DUDI7_ERR_NOTFORMAT, "Blocks info too small");
        const uint32_t numBlocks = BE32(info.data(), 16);
        if (numBlocks > MaxBlocks || 20 + static_cast<int64_t>(numBlocks) * 10 + 4 > usize)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of blocks");
        _blocks.assign(numBlocks, Block());
        p = 20;
        int64_t upos = 0;
        uint32_t packedPos = 0;
        for (uint32_t i = 0; i < numBlocks; ++i)
        {
            Block &b = _blocks[i];
            b.USize = BE32(info.data(), p);
            b.CSize = BE32(info.data(), p + 4);
            b.Flags = static_cast<uint16_t>((static_cast<uint16_t>(info[static_cast<size_t>(p) + 8]) << 8) |
                                            info[static_cast<size_t>(p) + 9]);
            b.UStart = upos;
            b.CPos = dataPos + packedPos;
            if (b.USize > MaxBlock)
                Fail(DUDI7_ERR_NOTFORMAT, "A block is too big");
            upos += b.USize;
            packedPos += b.CSize;
            p += 10;
        }
        ResetCache();
        // choose where the data starts: the first block must unpack
        if (numBlocks > 0 && altPos != dataPos && _blocks[0].USize > 0 && !LoadBlock(0))
        {
            const int64_t delta = altPos - dataPos;
            for (Block &b : _blocks)
                b.CPos += delta;
            ResetCache();
        }
        // files
        const uint32_t numFiles = BE32(info.data(), p);
        p += 4;
        if (numFiles > MaxFiles)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of files");
        _formatId = "UnityFS";
        _dirSeparator = "/";
        _extractInternal = true;
        ResetCache();
        std::vector<uint32_t> nodeFlags(static_cast<size_t>(numFiles) + 1);
        for (uint32_t i = 1; i <= numFiles; ++i)
        {
            if (p + 21 > usize)
                Fail(DUDI7_ERR_NOTFORMAT, "The files directory is truncated");
            const int64_t filePos = BE64(info.data(), p);
            const int64_t fileSize = BE64(info.data(), p + 8);
            nodeFlags[i - 1] = BE32(info.data(), p + 16);
            p += 20;
            int64_t e = p;
            while (e < usize && info[static_cast<size_t>(e)] != 0)
                ++e;
            if (e >= usize)
                Fail(DUDI7_ERR_NOTFORMAT, "Unterminated path");
            const std::string name(reinterpret_cast<const char *>(&info[static_cast<size_t>(p)]),
                                   static_cast<size_t>(e - p));
            p = e + 1;
            if (filePos < 0 || fileSize < 0 || filePos + fileSize > upos)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the data");
            Add(name, filePos, fileSize);
        }
        // the objects of the serialized files
        _rawCount = static_cast<int>(_entries.size());
        // (the flag 4 is not always set, the "data.unity3d" of some games hold the .assets files of the
        // player without it: any file that is not a data stream is tried, its header decides)
        for (int i = 0; i < _rawCount; ++i)
        {
            const Entry entry = _entries[static_cast<size_t>(i)];
            if (entry.Size >= 64 &&
                ((nodeFlags[static_cast<size_t>(i)] & 4) != 0 ||
                 !(EndsText(entry.Name, ".resS") || EndsText(entry.Name, ".resource"))))
                AddObjects(entry.Offset, entry.Size, entry.Name + ".objects/");
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX > 0)
            return ExtractVirtual(_virt[static_cast<size_t>(entry.DataX) - 1], sink);
        return StreamRange(entry.Offset, entry.Size, sink);
    }

  protected:
    auto ReadAt(int64_t pos, int64_t len, void *dst) -> bool override
    {
        if (len < 0)
            return false;
        while (len > 0)
        {
            const int b = FindBlock(pos);
            if (b < 0 || !LoadBlock(b))
                return false;
            const int64_t from = pos - _blocks[static_cast<size_t>(b)].UStart;
            int64_t n = static_cast<int64_t>(_blocks[static_cast<size_t>(b)].USize) - from;
            if (n > len)
                n = len;
            if (n <= 0)
                return false;
            std::memcpy(dst, &_slotData[static_cast<size_t>(_cur)][static_cast<size_t>(from)], static_cast<size_t>(n));
            dst = static_cast<uint8_t *>(dst) + n;
            pos += n;
            len -= n;
        }
        return true;
    }

    auto PackedBlock(int64_t pos, int64_t &unpackBytes) -> int64_t override
    {
        int64_t result = FindBlock(pos);
        unpackBytes = 0;
        if (result >= 0 && (_blocks[static_cast<size_t>(result)].Flags & 0x3F) != 0)
            unpackBytes = _blocks[static_cast<size_t>(result)].USize;
        else
            result = -1; // (stored blocks cost nothing)
        return result;
    }

    // Data referenced by a path of the bundle ("archive:/CAB-xxx/CAB-xxx.resS"): size bytes at off
    // in the file of that name
    auto ResolveStream(const std::string &path, int64_t off, int64_t size, int64_t &pos) -> bool override
    {
        const size_t slash = path.find_last_of('/');
        const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
        if (base.empty() || off < 0 || size < 0)
            return false;
        for (int i = 0; i < _rawCount; ++i)
        {
            const Entry &entry = _entries[static_cast<size_t>(i)];
            if (entry.Name == base && off + size <= entry.Size)
            {
                pos = entry.Offset + off;
                return true;
            }
        }
        return false;
    }

  private:
    void ResetCache()
    {
        for (int &idx : _slotIdx)
            idx = -1;
        _cur = 0;
    }

    auto FindBlock(int64_t pos) const -> int
    {
        if (_blocks.empty() || pos < 0)
            return -1;
        int lo = 0, hi = static_cast<int>(_blocks.size()) - 1;
        while (lo < hi)
        {
            const int mid = (lo + hi + 1) / 2;
            if (_blocks[static_cast<size_t>(mid)].UStart <= pos)
                lo = mid;
            else
                hi = mid - 1;
        }
        // skip the empty blocks
        const int high = static_cast<int>(_blocks.size()) - 1;
        while (lo <= high && pos >= _blocks[static_cast<size_t>(lo)].UStart + _blocks[static_cast<size_t>(lo)].USize)
            ++lo;
        return lo <= high ? lo : -1;
    }

    auto LoadBlock(int idx) -> bool
    {
        ++_clock;
        for (int i = 0; i < 4; ++i)
            if (_slotIdx[i] == idx)
            {
                _cur = i;
                _slotAge[i] = _clock;
                return true;
            }
        // the oldest slot is reused
        int slot = 0;
        for (int i = 1; i <= 3; ++i)
            if (_slotIdx[i] < 0 || (_slotIdx[slot] >= 0 && _slotAge[i] < _slotAge[slot]))
                slot = i;
        _slotIdx[slot] = -1;
        const Block &b = _blocks[static_cast<size_t>(idx)];
        if (b.CPos < 0 || b.CPos + b.CSize > _stream->Size())
            return false;
        std::vector<uint8_t> src(static_cast<size_t>(b.CSize) + 1);
        _slotData[slot].assign(static_cast<size_t>(b.USize) + 1, 0);
        _stream->Seek(b.CPos);
        if (b.CSize > 0)
            _stream->ReadBuffer(src.data(), b.CSize);
        if (UnpackBlock(b.Flags & 0x3F, src.data(), static_cast<int>(b.CSize), _slotData[slot].data(),
                   static_cast<int>(b.USize)))
        {
            _slotIdx[slot] = idx;
            _slotAge[slot] = _clock;
            _cur = slot;
            return true;
        }
        return false;
    }

    std::vector<Block> _blocks;
    // the last blocks that were unpacked: an object that straddles two blocks, or sprites that read in two places,
    // would unpack the same block again and again with only one
    int _slotIdx[4] = {-1, -1, -1, -1};
    std::vector<uint8_t> _slotData[4];
    int64_t _slotAge[4] = {};
    int64_t _clock = 0;
    int _cur = 0;
    int _rawCount = 0;
};
} // namespace

void RegisterUnityFs()
{
    Register<UnityFsArchive>("*.BUNDLE;*.UNITY3D;*.ASSETBUNDLE", "Unity Asset Bundle (*.BUNDLE;*.UNITY3D)");
}
} // namespace Dud
