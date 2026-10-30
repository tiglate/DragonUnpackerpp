// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Where the colors of a game are: the palettes stored in a ROM, found by their structure, and their
// conversion to RGB.
//  - Mega Drive: words 0000 BBB0 GGG0 RRR0 (the bits that the 9 bits color does not use are zero), a run of
//    16 such words or more is a palette (very unlikely to happen by chance);
//  - Super Nintendo: words 0BBBBBGGGGGRRRRR (15 bits), a weak signature (tile maps look the same): only blocks of
//    8 lines of 16 colors or more count;
//  - NES: bytes below 0x40 in groups of 4 whose first byte is the same background color in each group; each
//    group of 4 is reported as a palette.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Dud
{
enum class PaletteKind
{
    MegaDrive,
    Snes,
    Nes,
};

struct PaletteFind
{
    PaletteKind Kind = PaletteKind::MegaDrive;
    int64_t Offset = 0; // in the ROM
    int Colors = 0;     // 16 for each line of a Mega Drive or SNES palette, 4 for each palette of the NES tiles
};

auto BytesPerColor(PaletteKind kind) -> int;

auto FindMegaDrivePalettes(const uint8_t *rom, size_t size) -> std::vector<PaletteFind>;
auto FindSnesPalettes(const uint8_t *rom, size_t size) -> std::vector<PaletteFind>;
auto FindNesPalettes(const uint8_t *rom, size_t size) -> std::vector<PaletteFind>;

// 0xAARRGGBB colors of the palette at data (find.Colors entries)
auto PaletteColors(PaletteKind kind, const uint8_t *data, int count) -> std::vector<uint32_t>;
// A JASC-PAL text file (what image editors read)
auto MakeJascPal(const std::vector<uint32_t> &colors) -> std::vector<uint8_t>;
// A 32 bits TGA with the colors as squares, 16 per row
auto MakeSwatchTga(const std::vector<uint32_t> &colors) -> std::vector<uint8_t>;
} // namespace Dud
