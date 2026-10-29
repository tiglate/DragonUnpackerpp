// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// LEGO Star Wars .DAT support, ported from drv_default.dpr of Dragon UnPACKer 5.
// Header (8 bytes): offset and size of the directory; the file ends with the directory (offset + size = size of the
// file) and the directory starts with -1. At offset + 4: the number of entries and the entries (offset, size,
// size again, unknown: 16 bytes), then a table of tree nodes (16 bits info, 16 bits info, offset of the name in
// the names block: 8 bytes each) and the names block (size in front). The nodes are a tree in preorder: a node
// without a dot in its name is a folder whose children are the next nodes up to the index given in info 1; a
// node with a dot is a file whose entry is (info 1 xor $FFFF) + 1. Only files with the extension .DAT are tried.

#include "Registry.h"

namespace Dud
{
namespace
{
class LegoDatArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        int32_t h[2];
        if (stream.Size() < 16 || stream.Read(h, 8) != 8)
            return false;
        if (h[0] < 8 || h[1] < 0 || static_cast<int64_t>(h[0]) + h[1] != stream.Size())
            return false;
        stream.Seek(h[0]);
        int32_t marker;
        return stream.Read(&marker, 4) == 4 && marker == -1;
    }

    void Parse() override
    {
        const int64_t total = _stream->Size();
        _stream->Seek(0);
        const int32_t dirOffset = _stream->I32();
        const int32_t dirSize = _stream->I32();
        if (dirOffset + 4 > total || dirSize < 0 || dirOffset < 0 || static_cast<int64_t>(dirOffset) + dirSize > total)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid directory");
        _stream->Seek(static_cast<int64_t>(dirOffset) + 4);
        const int32_t entryCount = _stream->I32();
        // entries should be between 1 and 100000
        if (entryCount <= 0 || entryCount > 100000)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of entries");
        struct Item
        {
            int32_t Offset, Size, Size2, Unknown;
        };
        std::vector<Item> items(static_cast<size_t>(entryCount));
        for (Item &item : items)
        {
            _stream->ReadBuffer(&item, 16);
            // the offset and the size of the entry should be into the file
            if (item.Offset > total || item.Size < 0 || item.Offset < 0 || static_cast<int64_t>(item.Offset) + item.Size > total)
                Fail(DUDI7_ERR_NOTFORMAT, "A file is outside of the archive");
        }
        const int32_t nodeCount = _stream->I32();
        if (nodeCount <= 0 || nodeCount > 100000)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid number of nodes");
        struct Node
        {
            uint16_t Info1, Info2;
            int32_t NamePos;
        };
        std::vector<Node> nodes(static_cast<size_t>(nodeCount));
        for (Node &node : nodes)
            _stream->ReadBuffer(&node, 8);
        const int32_t namesSize = _stream->I32();
        if (namesSize < 0 || namesSize > total || _stream->Pos() + namesSize > total)
            Fail(DUDI7_ERR_NOTFORMAT, "Invalid size of the names");
        std::vector<char> names(static_cast<size_t>(namesSize));
        if (namesSize > 0)
            _stream->ReadBuffer(names.data(), names.size());

        _formatId = "LEGODAT";
        _dirSeparator = "\\";
        _extractInternal = false;
        std::vector<std::string> dirName(nodes.size());
        for (size_t x = 0; x < nodes.size(); ++x)
        {
            const int32_t pos = nodes[x].NamePos;
            if (pos < 0 || pos >= namesSize)
                Fail(DUDI7_ERR_NOTFORMAT, "A name is outside of the names");
            size_t end = static_cast<size_t>(pos);
            while (end < names.size() && names[end] != '\0')
                ++end;
            const std::string disp(&names[static_cast<size_t>(pos)], end - static_cast<size_t>(pos));
            dirName[x] = dirName[x].empty() ? disp : dirName[x] + "\\" + disp;
            if (disp.find('.') == std::string::npos)
            {
                for (size_t y = x + 1; y <= nodes[x].Info1 && y < nodes.size(); ++y)
                    if (dirName[y].find('.') == std::string::npos)
                        dirName[y] = dirName[x];
            }
            else
            {
                const uint16_t index = static_cast<uint16_t>((nodes[x].Info1 ^ 0xFFFF) + 1);
                if (index >= items.size())
                    Fail(DUDI7_ERR_NOTFORMAT, "An entry does not exist");
                Add(dirName[x], items[index].Offset, items[index].Size);
            }
        }
    }
};
} // namespace

void RegisterLegoDat()
{
    Register<LegoDatArchive>("*.DAT", "LEGO Star Wars Archive (*.DAT)");
}
} // namespace Dud
