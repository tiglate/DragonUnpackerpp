// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Console ROM support shared by the NES, SNES and Mega Drive/Genesis formats: tile graphics
// rendered as sheets (colour mapped TGA, 16 tiles per row, in grays since the palettes live
// elsewhere in the ROM), and the archive base class that presents a ROM as raw parts, an info
// text and the sheets.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Archive.h"
#include "codecs/Palette.h"
#include "codecs/Pcm.h"
#include "codecs/RomCompress.h"

namespace Dud
{
enum class TileFormat
{
    Nes2bpp,  // 16 bytes per tile, plane 0 then plane 1
    Snes2bpp, // 16 bytes per tile, bit planes interleaved by row
    Snes3bpp, // 24 bytes per tile, planes 0/1 interleaved by row, then plane 2 (Mario World...)
    Snes4bpp, // 32 bytes per tile, planes 0/1 then 2/3 interleaved by row
    Md4bpp,   // 32 bytes per tile, 4 bits per pixel, left pixel in the high nibble
};

auto TileBytes(TileFormat format) -> int64_t;
// The TGA of the tiles in data (the last row of tiles is completed with black)
auto RenderTileSheet(const uint8_t *data, int64_t length, TileFormat format) -> std::vector<uint8_t>;
// Size of that TGA for length bytes of tiles
auto TileSheetSize(int64_t length, TileFormat format) -> int64_t;

// Text from a fixed field of a ROM header: printable ASCII only, trailing blanks removed
auto HeaderText(const uint8_t *field, size_t size) -> std::string;
// "name: value" lines for the info entries
auto InfoLine(const std::string &name, const std::string &value) -> std::string;
auto Hex(uint64_t value, int digits) -> std::string;

// The ROM of a cartridge as the "logical" bytes the console sees. Plain files are copied as is
// (from RomBase, after a copier header); Super Magic Drive dumps are deinterleaved
// (16 KB blocks: odd bytes in the first half, even bytes in the second one).
class RomSource
{
  public:
    static constexpr int64_t SmdHeader = 512;
    static constexpr int64_t SmdBlock = 16384;

    RomSource(Reader &stream, int64_t base, bool interleaved) : _stream(stream), _base(base), _smd(interleaved)
    {
    }
    // The size of the ROM in bytes
    auto Size() const -> int64_t;
    // Fill buffer with length bytes from the logical offset (clipped to the ROM, the rest is
    // zero); returns the number of bytes available
    auto Read(int64_t offset, uint8_t *buffer, int64_t length) const -> int64_t;
    // Offset in the file of a logical offset (plain files only)
    auto FileOffset(int64_t offset) const -> int64_t
    {
        return _base + offset;
    }
    auto Interleaved() const noexcept -> bool
    {
        return _smd;
    }

  private:
    Reader &_stream;
    int64_t _base;
    bool _smd;
};

// Base of the ROM formats: Parse of the descendants calls Add* with offsets in the logical ROM
class RomArchive : public Archive
{
  public:
    auto Extract(int64_t index, const Sink &sink) -> int32_t override;

  protected:
    // The ROM bytes [offset, offset + length)
    void AddRaw(const std::string &name, int64_t offset, int64_t length);
    // Tile sheets of sheetBytes bytes covering [offset, offset + length)
    void AddSheets(const std::string &folder, const std::string &prefix, int64_t offset, int64_t length,
                   TileFormat format, int64_t sheetBytes);
    // A compressed stream at the ROM offset (packed bytes), giving size bytes: the raw data, or
    // the tiles drawn as a sheet
    void AddDecoded(const std::string &name, RomCodec codec, int64_t offset, int64_t packed, int64_t size, bool sheet,
                    TileFormat format = TileFormat::Md4bpp);
    // A BRR sample (blocks of 9 bytes at the ROM offset) as a WAV file
    void AddBrr(const std::string &name, int64_t offset, int64_t blocks);
    // Raw 8 bits sound at the ROM offset, as a WAV file
    void AddPcm(const std::string &name, int64_t offset, int64_t length);
    // The palettes found in the ROM image: a .pal file (JASC-PAL) and a swatch picture of each
    void AddPalettes(const uint8_t *image, size_t size, const std::vector<PaletteFind> &finds);
    void AddText(const std::string &name, std::string text);
    // Bytes that are not in the ROM part (a copier header, ...): offset in the file
    void AddFileBytes(const std::string &name, int64_t fileOffset, int64_t length);
    void SetRom(int64_t base, bool interleaved);

    std::unique_ptr<RomSource> _rom;

  private:
    enum class Kind
    {
        Raw,
        Sheet,
        Text,
        File,
        Decoded,
        Brr,
        Swatch,
        Pcm,
    };
    struct Plan
    {
        Kind Type = Kind::Raw;
        int64_t Offset = 0;
        int64_t Length = 0;
        TileFormat Format = TileFormat::Nes2bpp;
        std::string Text;
        RomCodec Codec = RomCodec::Nemesis;
        int64_t Packed = 0;
        bool AsSheet = false;
        std::vector<uint32_t> Colors;
    };
    std::vector<Plan> _plans;
};
} // namespace Dud
