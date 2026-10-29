// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Build engine .ART tile files (Duke Nukem 3D, Shadow Warrior, Blood)
// (ported from drv_default.dpr of Dragon UnPACKer 5).
// Header: version (1), number of tiles, first and last tile of the file.
// Then widths (16 bits), heights (16 bits) and picanm (32 bits) for each tile
// and finally the 8 bits pixels of the tiles (width * height bytes each).
// Each tile is presented as an entry (DataX = width, DataY = height).

#include "Registry.h"

namespace Dud
{
namespace
{
constexpr int64_t MaxTiles = 65536;

// "texture #" and the tile number on 6 digits: shorter numbers are padded with zeros, longer ones
// keep their last 6 digits
auto TileName(int64_t number) -> std::string
{
    const std::string digits = std::to_string(number);
    return "texture #" + ("00000" + digits).substr(digits.size() - 1, 6) + ".tex";
}

class ArtArchive : public Archive
{
  public:
    static auto RequiresExtension() -> bool
    {
        return true;
    }

    static auto Check(Reader &stream) -> bool
    {
        stream.Seek(0);
        if (stream.Size() < 16)
            return false;
        const int32_t version = stream.I32();
        const int32_t tiles = stream.I32();
        const int32_t first = stream.I32();
        const int32_t last = stream.I32();
        return version == 1 && tiles >= 0 && first >= 0 && last >= first - 1 &&
               static_cast<int64_t>(last) - first + 1 <= MaxTiles &&
               16 + (static_cast<int64_t>(last) - first + 1) * 8 <= stream.Size();
    }

    void Parse() override
    {
        _stream->Seek(4);
        _stream->I32();
        const int32_t first = _stream->I32();
        const int32_t last = _stream->I32();
        const int32_t count = last - first + 1;
        CheckTable(16, count, 8);

        _formatId = "ART";
        _dirSeparator = "";
        std::vector<uint16_t> widths(static_cast<size_t>(count));
        std::vector<uint16_t> heights(static_cast<size_t>(count));
        for (auto &w : widths)
            w = _stream->U16();
        for (auto &h : heights)
            h = _stream->U16();
        // picanm: 4 bytes per tile, not needed
        int64_t offset = 16 + static_cast<int64_t>(count) * 8;
        for (int32_t i = 0; i < count; ++i)
        {
            const int64_t size = static_cast<int64_t>(widths[static_cast<size_t>(i)]) * heights[static_cast<size_t>(i)];
            if (size <= 0)
                continue;
            if (offset + size > _stream->Size())
                Fail(DUDI7_ERR_NOTFORMAT, "Tile data is outside of the file");
            Add(TileName(static_cast<int64_t>(i) + first), offset, size, widths[static_cast<size_t>(i)],
                heights[static_cast<size_t>(i)]);
            offset += size;
        }
    }
};
} // namespace

void RegisterArt()
{
    Register<ArtArchive>("*.ART", "Duke Nukem 3D (*.ART)|Shadow Warrior (*.ART)|Blood (*.ART)");
}
} // namespace Dud
