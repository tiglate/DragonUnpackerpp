// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Decoders of the compression schemes of console games and scanners that find the compressed
// streams in a ROM: Mega Drive / Genesis (Nemesis and Kosinski, the ones of Sonic and many other
// games) and Super Nintendo (the LZ family of Nintendo: Super Mario World, Zelda 3...).
// Written from the public descriptions of the formats (Sega Retro, the Sonic disassemblies,
// SMW Central, the Zelda 3 documentation).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Dud
{
enum class RomCodec
{
    Nemesis,  // tile art: word header (tile count, XOR flag), code table, prefix coded nibble runs
    Kosinski, // LZSS with 16 bits description words, ends with a special match
    Lz2,      // Nintendo LZ: command byte (3 bits command, 5 bits length), ends with 0xFF (Zelda 3)
    Lz2Be,    // the same with the offsets of the copy command high byte first (Super Mario World)
};

struct RomStream
{
    RomCodec Codec = RomCodec::Nemesis;
    int64_t Offset = 0;  // in the ROM
    int64_t Packed = 0;  // bytes used by the compressed data
    int64_t Size = 0;    // bytes once decompressed
};

// Decode the stream at the start of src (avail bytes). False when the data is not a valid stream
// of that format; used receives the bytes of src that the stream takes. maxOut bounds the output.
auto DecodeNemesis(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used) -> bool;
auto DecodeKosinski(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used) -> bool;
// Nintendo LZ ("LC_LZ2"): 0 copy, 1 byte fill, 2 word fill, 3 incrementing fill, 4 copy from the
// output (absolute offset), 7 = longer length; 0xFF ends the stream
auto DecodeLz2(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used,
               bool bigEndianOffsets = false) -> bool;
auto Decode(RomCodec codec, const uint8_t *src, size_t avail, std::vector<uint8_t> &out, size_t &used) -> bool;

// The Super Nintendo streams of a ROM (hiRom: the addresses of the tables use the HiROM map)
auto ScanSnes(const uint8_t *rom, size_t size, bool hiRom) -> std::vector<RomStream>;

// The streams found in a ROM (word aligned, not overlapping, in order of offset)
auto ScanMegaDrive(const uint8_t *rom, size_t size) -> std::vector<RomStream>;
} // namespace Dud
