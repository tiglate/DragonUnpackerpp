// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Emperor: Battle for Dune / Nox .BAG support, ported from drv_default.dpr of Dragon UnPACKer 5. Two variants:
// the directory inside the file (below) or in a .IDX file next to a .BAG that only holds the data (BagIdx:
// the .IDX has the same 12 bytes header, then entries of 36 bytes: name[16], offset, size, frequency, media
// type, unknown).
// "GABA", unknown, number of entries, entry size (64), then entries of 64 bytes: name[32],
// offset, size, frequency, media type, 16 unknown bytes. The extension is deduced
// from the media type.

#include "PathUtil.h"
#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t HeaderSize = 16;
constexpr uint32_t EntrySize = 64;

class BagArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        uint32_t h[4];
        return stream.Read(h, 16) == 16 && h[0] == 0x41424147 && h[3] == EntrySize;
    }

    void Parse() override
    {
        _stream->Seek(8);
        const uint32_t count = _stream->U32();
        CheckTable(HeaderSize, count, EntrySize);
        _stream->Seek(HeaderSize);

        _formatId = "BAG";
        _dirSeparator = "";
        _extractInternal = false;
        for (uint32_t i = 1; i <= count; ++i)
        {
            char name[32];
            _stream->ReadBuffer(name);
            const uint32_t offset = _stream->U32();
            const uint32_t size = _stream->U32();
            _stream->U32();
            const uint32_t mediaType = _stream->U32();
            _stream->Skip(16);
            std::string ext;
            switch (mediaType)
            {
            case 2:
            case 6:
                ext = "w";
                break;
            case 12:
            case 28:
                ext = "cmp";
                break;
            case 37:
                ext = "mp3";
                break;
            default:
                ext = "raw";
            }
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(name) + "." + ext, offset, size);
        }
    }
};

auto MediaExtension(uint32_t mediaType) -> std::string
{
    switch (mediaType)
    {
    case 2:
    case 6:
        return "w";
    case 12:
    case 28:
        return "cmp";
    case 37:
        return "mp3";
    default:
        return "raw";
    }
}

class BagIdxArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        if (StartsWith(stream, "GABA"))
            return false; // (the other variant)
        const std::unique_ptr<Reader> idx = OpenIdx(stream);
        if (!idx || idx->Size() < 12)
            return false;
        idx->Seek(0);
        const uint32_t id = idx->U32();
        idx->U32();
        const uint32_t count = idx->U32();
        if (id != 0x41424147 || count == 0 || 12 + static_cast<int64_t>(count) * 36 > idx->Size())
            return false;
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[16];
            idx->ReadBuffer(name);
            const uint32_t offset = idx->U32();
            const uint32_t size = idx->U32();
            idx->Skip(12);
            if (name[0] == '\0' || static_cast<int64_t>(offset) + size > stream.Size())
                return false;
        }
        return true;
    }

    void Parse() override
    {
        const std::unique_ptr<Reader> idx = OpenIdx(*_stream);
        if (!idx)
            Fail(DUDI7_ERR_NOTFORMAT, "No .IDX file next to the .BAG");
        idx->Seek(8);
        const uint32_t count = idx->U32();
        if (12 + static_cast<int64_t>(count) * 36 > idx->Size())
            Fail(DUDI7_ERR_NOTFORMAT, "The index is outside of the file");
        _formatId = "BAG";
        _dirSeparator = "";
        _extractInternal = false;
        for (uint32_t i = 0; i < count; ++i)
        {
            char name[16];
            idx->ReadBuffer(name);
            const uint32_t offset = idx->U32();
            const uint32_t size = idx->U32();
            idx->U32();
            const uint32_t mediaType = idx->U32();
            idx->U32();
            if (static_cast<int64_t>(offset) + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(ZString(name) + "." + MediaExtension(mediaType), offset, size);
        }
    }

  private:
    static auto OpenIdx(Reader &bag) -> std::unique_ptr<Reader>
    {
        const std::string path = Sibling(bag.Path(), ".idx");
        if (path.empty())
            return nullptr;
        try
        {
            return std::make_unique<Reader>(path);
        }
        catch (const Error &)
        {
            return nullptr;
        }
    }
};
} // namespace

void RegisterBag()
{
    Register<BagArchive>("*.BAG", "Westwood/Pandemic Archive (*.BAG)|Emperor: Battle for Dune (*.BAG)|Nox (*.BAG)");
    Register<BagIdxArchive>("*.BAG", "Nox (*.BAG + *.IDX)");
}
} // namespace Dud
