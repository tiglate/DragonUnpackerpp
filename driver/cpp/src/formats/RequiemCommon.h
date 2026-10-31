// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// What the Requiem: Avenging Angel formats share: the second packed block of a level file (see RequiemJam.cpp),
// which starts with the level header (a magic number at offset 4) and holds the palette of the software
// renderer pictures. The game keeps two tables of 256 RGBX entries from offset 0x1F (the Windows palette, with the
// system colors, and the full screen palette that the art uses); the second one is read.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "../codecs/Dcl.h"
#include "Reader.h"

namespace Dud::Requiem
{
constexpr uint32_t DataMagic = 0x0BEAF0CCu;
constexpr size_t PaletteTable = 0x41F; // the full screen palette in the unpacked data block
constexpr size_t PaletteBytes = 768;
using Palette = std::array<uint8_t, PaletteBytes>; // RGB

struct DataBlock
{
    int64_t Packed = 0;    // offset of the DCL stream
    uint32_t Unpacked = 0; // size of the unpacked data
};

// The header field at offset 4 is the offset (minus 12) of the u32 unpacked size of the second block, followed
// by its DCL stream
inline auto LocateData(Reader &stream, DataBlock &block) -> bool
{
    if (stream.Size() < 0x120)
        return false;
    stream.Seek(4);
    const int64_t at = static_cast<int64_t>(stream.U32()) + 12;
    if (at < 0x112 || at + 8 > stream.Size())
        return false;
    stream.Seek(at);
    block.Unpacked = stream.U32();
    block.Packed = at + 4;
    return block.Unpacked >= PaletteTable + 1024 && block.Unpacked <= 256u * 1024 * 1024;
}

// The palette of a level file; false if the block is not there or is not a level data block
inline auto ReadPalette(Reader &stream, Palette &palette) -> bool
{
    DataBlock block;
    if (!LocateData(stream, block))
        return false;
    // Only the start of the block is needed: a few KB of the stream are enough for 2 KB of tables
    const int64_t available = std::min<int64_t>(stream.Size() - block.Packed, 1 << 16);
    std::vector<uint8_t> packed(static_cast<size_t>(available));
    stream.Seek(block.Packed);
    stream.ReadBuffer(packed.data(), packed.size());
    std::vector<uint8_t> out;
    if (!DclExplode(packed.data(), packed.size(), PaletteTable + 1024, out, true) || out.size() != PaletteTable + 1024)
        return false;
    const uint32_t magic = out[4] | out[5] << 8 | out[6] << 16 | static_cast<uint32_t>(out[7]) << 24;
    if (magic != DataMagic)
        return false;
    for (size_t i = 0; i < 256; ++i)
        for (size_t c = 0; c < 3; ++c)
            palette[i * 3 + c] = out[PaletteTable + i * 4 + c];
    return true;
}
} // namespace Dud::Requiem
