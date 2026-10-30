// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// What is known about the graphics of popular console games, from data/rom-metadata (recorded by running the games in an
// emulator): the palettes the screen used with the graphics, so that previews and exports have the right colours.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Dud
{
struct KnownRun
{
    int64_t Offset = 0;
    int64_t Length = 0;
    int Bpp = 4;
};

struct KnownStream
{
    int64_t Offset = 0;
    std::vector<std::vector<uint32_t>> Palettes; // 0xRRGGBB, most used first
};

struct KnownGame
{
    std::vector<KnownStream> Streams;
    std::vector<std::vector<uint32_t>> RawPalettes;
    std::vector<KnownRun> Runs; // uncompressed tiles
};

// The table of the game whose ROM (without copier header) has this CRC-32, or false
auto FindKnownGame(uint32_t crc32, KnownGame &game) -> bool;
} // namespace Dud
