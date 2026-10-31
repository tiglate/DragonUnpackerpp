// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// What is known about the graphics of popular console games, from data/rom-metadata (recorded by running the games in an
// emulator): the palettes the screen used with the graphics, so that previews and exports have the right colours.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Dud
{
struct KnownRun
{
    int64_t Offset = 0;
    int64_t Length = 0;
    int Bpp = 4;
    std::vector<std::vector<uint32_t>> Palettes; // the ones it was drawn with, most used first
};

struct KnownStream
{
    std::string Codec; // lz, lzp, lzss (the scanner finds those) or dkc or lz5 (decoded where the table says)
    int64_t Offset = 0;
    std::vector<int> Bpp; // tile formats the screen used from it
    std::vector<std::vector<uint32_t>> Palettes; // 0xRRGGBB, most used first
};

// How a game loads its music into the sound chip: the engine and the samples once, then one bank of songs, and the
// state of the engine after it started; a song is a value of the CPU ports (see codecs/Spc)
struct KnownBlock
{
    int64_t Offset = 0;
    int64_t Length = 0;
    int Dest = 0;
};

struct KnownBank
{
    std::vector<KnownBlock> Blocks;
    std::vector<std::array<uint8_t, 4>> Songs;
};

struct KnownMusic
{
    int Pc = 0, A = 0, X = 0, Y = 0, Psw = 0, Sp = 0;
    std::vector<uint8_t> Dsp; // the 128 registers of the DSP
    std::vector<uint8_t> Io;  // $F0-$FF of the chip
    std::vector<KnownBlock> Engine;
    std::vector<std::pair<int, std::vector<uint8_t>>> Patches; // variables of the engine: (address, bytes)
    std::vector<KnownBank> Banks;
};

struct KnownGame
{
    KnownMusic Music;
    std::vector<KnownStream> Streams;
    std::vector<std::vector<uint32_t>> RawPalettes;
    std::vector<KnownRun> Runs; // uncompressed tiles
};

// The table of the game whose ROM (without copier header) has this CRC-32, or false
auto FindKnownGame(uint32_t crc32, KnownGame &game) -> bool;
} // namespace Dud
