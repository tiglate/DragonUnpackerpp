// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Decoders of the compression schemes of Mega Drive / Genesis games (Nemesis and Kosinski, the
// ones of Sonic and many other games) and a scanner that finds the compressed streams in a ROM.
// Written from the public descriptions of the formats (Sega Retro, the Sonic disassemblies).
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
auto Decode(RomCodec codec, const uint8_t *src, size_t avail, std::vector<uint8_t> &out, size_t &used) -> bool;

// The streams found in a ROM (word aligned, not overlapping, in order of offset)
auto ScanMegaDrive(const uint8_t *rom, size_t size) -> std::vector<RomStream>;
} // namespace Dud
