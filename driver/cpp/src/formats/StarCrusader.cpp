// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Star Crusader .GL and .PAK support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header: number of entries (16 bits) and the offset of the directory (32 bits); the directory holds, for each
// entry, its offset (32 bits) and its name (8 characters). The data of an entry runs up to the next offset and
// the last one up to the directory. A .GL file holds files and sub archives (their names get the extension
// ".gl"); a .PAK file holds sounds (".wav" is added to the names). There is no signature: only files with
// these extensions are tried, and the offsets must grow and stay before the directory.

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t EntrySize = 12;

// Is the block at offset (size bytes) a Star Crusader archive too?
auto IsSubArchive(Reader &stream, int64_t offset, int64_t size) -> bool
{
    if (offset < 0 || offset + 6 > stream.Size())
        return false;
    stream.Seek(offset);
    uint16_t count;
    uint32_t dirOffset;
    if (stream.Read(&count, 2) != 2 || stream.Read(&dirOffset, 4) != 4)
        return false;
    return static_cast<int64_t>(dirOffset) + count * EntrySize <= size;
}

template <bool Gl> class StarCrusaderArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint16_t count;
        uint32_t dirOffset;
        if (stream.Size() < 6 + EntrySize || stream.Read(&count, 2) != 2 || stream.Read(&dirOffset, 4) != 4 ||
            count == 0)
            return false;
        if (static_cast<int64_t>(dirOffset) + count * EntrySize > stream.Size() || dirOffset < 6)
            return false;
        stream.Seek(dirOffset);
        uint32_t previous = 6;
        for (uint16_t i = 0; i < count; ++i)
        {
            uint32_t offset;
            char name[8];
            if (stream.Read(&offset, 4) != 4 || stream.Read(name, 8) != 8 || offset < previous || offset > dirOffset)
                return false;
            for (char c : name)
                if (c != '\0' && (static_cast<unsigned char>(c) < ' ' || static_cast<unsigned char>(c) > '~'))
                    return false;
            previous = offset;
        }
        return true;
    }

    void Parse() override
    {
        _stream->Seek(0);
        const uint16_t count = _stream->U16();
        const uint32_t dirOffset = _stream->U32();
        if (count == 0 || static_cast<int64_t>(dirOffset) + count * EntrySize > _stream->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory");
        _formatId = Gl ? "SCPAK" : "SCGL"; // (swapped in v5, kept)
        _dirSeparator = "";
        _extractInternal = false;
        _stream->Seek(dirOffset);
        uint32_t oldOffset = dirOffset;
        std::string name;
        for (uint16_t x = 1; x <= count; ++x)
        {
            const uint32_t offset = _stream->U32();
            char raw[8];
            _stream->ReadBuffer(raw);
            if (x > 1)
            {
                if (offset < oldOffset || offset > dirOffset)
                    Fail(DUDI7_ERR_NOTFORMAT, "Invalid offset");
                if (Gl && IsSubArchive(*_stream, oldOffset, offset - oldOffset))
                    name = ChangeFileExt(name, ".gl");
                Add(name, oldOffset, offset - oldOffset);
                _stream->Seek(static_cast<int64_t>(dirOffset) + x * EntrySize);
            }
            oldOffset = offset;
            name = ZString(raw);
            if (!Gl)
                name += ".wav";
        }
        if (oldOffset > dirOffset)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid offset");
        Add(name, oldOffset, dirOffset - oldOffset);
    }
};
} // namespace

void RegisterStarCrusaderGl()
{
    Register<StarCrusaderArchive<true>>("*.GL", "Star Crusader Archive (*.GL)");
}

void RegisterStarCrusaderPak()
{
    Register<StarCrusaderArchive<false>>("*.PAK", "Star Crusader Sound Archive (*.PAK)");
}
} // namespace Dud
