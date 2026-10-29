// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Fifth Element .MRC support, ported from drv_default.dpr of Dragon UnPACKer 5.
// A RIFF file of type "mmRC" (blocks are not padded to even sizes). The block LIST "FSys" holds "DLst"
// (directories, 20 bytes: name offset in NLst, number of sub folders, first sub folder, number of files, index
// of the file before the first one), "FLst" (files, 16 bytes: name offset, size, offset in the content, 0) and
// "NLst" (the names, null terminated). The block LIST "FSto" holds the content. Differences with v5: a folder
// with a single sub folder gets its name in the path of that sub folder (v5 compared Start < End).

#include <cstring>

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int MaxNesting = 64;

struct Dir
{
    int64_t StartDir = 0, EndDir = -1;
    int64_t StartFile = 0, EndFile = 0;
    std::string Name;
};

auto NameAt(const std::vector<uint8_t> &names, int64_t at) -> std::string
{
    if (at < 0 || at >= static_cast<int64_t>(names.size()))
        return "";
    const char *p = reinterpret_cast<const char *>(names.data()) + at;
    return ZString(p, names.size() - static_cast<size_t>(at));
}

auto Le32(const uint8_t *p) -> int32_t
{
    int32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

class MrcArchive : public Archive
{
  public:
    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        char h[12];
        if (stream.Size() < 12 || stream.Read(h, 12) != 12)
            return false;
        int32_t size;
        std::memcpy(&size, h + 4, 4);
        return std::memcmp(h, "RIFF", 4) == 0 && std::memcmp(h + 8, "mmRC", 4) == 0 && size == stream.Size() - 8;
    }

    void Parse() override
    {
        const int64_t total = _stream->Size();
        std::vector<uint8_t> dlst, flst, nlst;
        int64_t storage = 0;
        int64_t pos = 12;
        // the blocks of the file
        while (pos + 8 <= total)
        {
            _stream->Seek(pos);
            char id[4];
            _stream->ReadBuffer(id);
            const int32_t size = _stream->I32();
            if (size < 0)
                Fail(DUDI7_ERR_NOTFORMAT, "Invalid block");
            const int64_t body = pos + 8;
            const int64_t next = body + size;
            if (std::memcmp(id, "LIST", 4) == 0)
            {
                char kind[4];
                _stream->ReadBuffer(kind);
                if (std::memcmp(kind, "FSys", 4) == 0)
                {
                    int64_t sub = body + 4;
                    while (sub + 8 <= next && sub + 8 <= total)
                    {
                        _stream->Seek(sub);
                        char sid[4];
                        _stream->ReadBuffer(sid);
                        const int32_t ssize = _stream->I32();
                        if (ssize < 0 || sub + 8 + ssize > total)
                            Fail(DUDI7_ERR_NOTFORMAT, "Invalid sub block");
                        std::vector<uint8_t> *target = std::memcmp(sid, "DLst", 4) == 0   ? &dlst
                                                       : std::memcmp(sid, "FLst", 4) == 0 ? &flst
                                                       : std::memcmp(sid, "NLst", 4) == 0 ? &nlst
                                                                                          : nullptr;
                        if (!target)
                            Fail(DUDI7_ERR_NOTFORMAT, "Unknown sub block");
                        target->resize(static_cast<size_t>(ssize));
                        if (ssize > 0)
                            _stream->ReadBuffer(target->data(), target->size());
                        sub += 8 + ssize;
                    }
                }
                else if (std::memcmp(kind, "FSto", 4) == 0)
                    storage = body + 4;
                else
                    Fail(DUDI7_ERR_NOTFORMAT, "Unknown list");
            }
            pos = next;
        }
        if (storage <= 0 || dlst.empty() || flst.empty() || nlst.empty() || dlst.size() % 20 != 0 ||
            flst.size() % 16 != 0)
            Fail(DUDI7_ERR_NOTFORMAT, "Missing or invalid lists");

        std::vector<Dir> dirs(dlst.size() / 20);
        for (size_t x = 0; x < dirs.size(); ++x)
        {
            const uint8_t *e = dlst.data() + x * 20;
            dirs[x].Name = NameAt(nlst, Le32(e));
            dirs[x].StartFile = static_cast<int64_t>(Le32(e + 16)) + 1;
            dirs[x].EndFile = static_cast<int64_t>(Le32(e + 16)) + Le32(e + 12);
            dirs[x].StartDir = Le32(e + 8);
            dirs[x].EndDir = static_cast<int64_t>(Le32(e + 8)) + Le32(e + 4) - 1;
        }
        // folder names with their parents (parents come first)
        for (size_t x = 0; x < dirs.size(); ++x)
            for (int64_t y = dirs[x].StartDir; y <= dirs[x].EndDir; ++y)
                if (y >= 0 && y < static_cast<int64_t>(dirs.size()) && y != static_cast<int64_t>(x))
                    dirs[static_cast<size_t>(y)].Name = dirs[x].Name + "\\" + dirs[static_cast<size_t>(y)].Name;

        _formatId = "mmRC";
        _dirSeparator = "\\";
        _extractInternal = false;
        const size_t count = flst.size() / 16;
        for (size_t x = 1; x <= count; ++x)
        {
            const uint8_t *e = flst.data() + (x - 1) * 16;
            std::string name;
            for (const Dir &d : dirs)
                if (d.StartFile <= static_cast<int64_t>(x) && d.EndFile >= static_cast<int64_t>(x))
                {
                    name = d.Name + "\\";
                    break;
                }
            name += NameAt(nlst, Le32(e));
            const int64_t offset = static_cast<int64_t>(Le32(e + 8)) + storage;
            const int64_t size = Le32(e + 4);
            if (size < 0 || Le32(e + 8) < 0 || offset + size > total)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
            Add(name, offset, size);
        }
    }
};
} // namespace

void RegisterMrc()
{
    Register<MrcArchive>("*.MRC", "The Fifth Element (*.MRC)");
}
} // namespace Dud
