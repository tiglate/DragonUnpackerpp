// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Prototype / Scarface / The Incredible Hulk: Ultimate Destruction .RCF support, ported
// from drv_default.dpr of Dragon UnPACKer 5.
// Header (60 bytes): "ATG CORE CEMENT LIBRARY" padded to 32 bytes, 3 unknown, offset of
// the names, 2 unknown, number of files. Then the file table (12 bytes: unknown, offset,
// size), which must be sorted by offset to match the names: at the names offset + 4, for
// each file: 16 bytes, name length (the null included), the name without its null.
// Names ending in ".rz" are zlib packed files: a 16 bytes header ("RZ", 64 bits unpacked
// size) then the zlib data; they are listed without the extension and unpacked.

#include <algorithm>
#include <cstring>

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr std::string_view Id = "ATG CORE CEMENT LIBRARY";
constexpr int64_t HeaderSize = 60;

class RcfArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char id[32];
        if (stream.Read(id, 32) != 32 || std::string_view(id, Id.size()) != Id)
            return false;
        for (size_t i = Id.size(); i < 32; ++i)
        {
            if (id[i] != '\0')
                return false;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(44);
        const uint32_t nameOffset = _stream->U32();
        _stream->Seek(56);
        const uint32_t fileCount = _stream->U32();
        if (nameOffset > _stream->Size() || fileCount > static_cast<uint64_t>(_stream->Size()) / 12)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid header");
        CheckTable(HeaderSize, fileCount, 12);
        std::vector<uint32_t> offsets(fileCount);
        std::vector<uint32_t> sizes(fileCount);
        for (uint32_t i = 0; i < fileCount; ++i)
        {
            _stream->U32();
            offsets[i] = _stream->U32();
            sizes[i] = _stream->U32();
        }
        // Files sorted by offset: the names are in that order (the key keeps the file number in
        // its low bits; as in the Pascal driver the key is signed)
        std::vector<int64_t> keys(fileCount);
        for (uint32_t i = 0; i < fileCount; ++i)
            keys[i] = static_cast<int64_t>((static_cast<uint64_t>(offsets[i]) << 32) | i);
        std::ranges::sort(keys);

        _formatId = "RCF";
        _dirSeparator = "\\";
        _extractInternal = true;
        _stream->Seek(static_cast<int64_t>(nameOffset) + 4);
        for (uint32_t i = 0; i < fileCount; ++i)
        {
            _stream->Skip(16);
            const uint32_t nameLength = _stream->U32();
            if (nameLength == 0 || nameLength > 256)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid name length");
            std::string name(nameLength - 1, '\0');
            if (nameLength > 1)
                _stream->ReadBuffer(name.data(), nameLength - 1);
            name = ZString(name.data(), name.size());
            if (!name.empty() && name[0] == '\\')
                name.erase(0, 1);
            const auto j = static_cast<uint32_t>(static_cast<uint64_t>(keys[i]) & 0xFFFFFFFFu);
            if (static_cast<int64_t>(offsets[j]) + sizes[j] > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            const bool packed = name.size() >= 3 && ToLower(name.substr(name.size() - 3)) == ".rz";
            if (packed && sizes[j] >= 16)
            {
                const int64_t position = _stream->Pos();
                _stream->Seek(offsets[j]);
                uint8_t header[16];
                _stream->ReadBuffer(header, 16);
                _stream->Seek(position);
                static constexpr uint8_t Magic[8] = {'R', 'Z', 0, 0, 0, 0, 0, 0};
                if (std::memcmp(header, Magic, 8) == 0)
                {
                    int64_t unpacked = 0;
                    std::memcpy(&unpacked, header + 8, 8);
                    Add(name.substr(0, name.size() - 3), offsets[j], unpacked, 1, sizes[j]);
                }
                else
                {
                    Add(name.substr(0, name.size() - 3), offsets[j], sizes[j], 0, 0);
                }
            }
            else
            {
                Add(name, offsets[j], sizes[j], 0, 0);
            }
        }
    }

    auto Extract(int64_t index, const Sink &sink) -> int32_t override
    {
        const Entry &entry = _entries[static_cast<size_t>(index)];
        if (entry.DataX == 0)
            return Archive::Extract(index, sink);
        return ExtractZlib(entry.Offset + 16, entry.DataY - 16, entry.Size, sink);
    }
};
} // namespace

void RegisterRcf()
{
    Register<RcfArchive>("*.RCF", "Radical Cement Library (*.RCF)|Prototype (*.RCF)|Scarface (*.RCF)|The Incredible "
                                  "Hulk: Ultimate Destruction (*.RCF)");
}
} // namespace Dud
