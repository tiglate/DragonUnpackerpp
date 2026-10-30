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
    Lz5,      // Super Metroid: the Nintendo LZ with two more copy commands (inverted, relative)
    Lzss,     // Final Fantasy VI: LZSS, 2 KB ring buffer, size header that counts itself
    LzssPayload, // the same, the size header counts only what follows it
    LzPackets,   // Chrono Trigger: LZ in packets, each starting with the offset of its end, where a control byte sits
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
enum class Lz2Variant
{
    Little, // the offset of command 4 low byte first (Zelda 3)
    Big,    // high byte first (Super Mario World)
    Metroid, // low byte first, plus 5: copy of the output with the bits flipped, 6: copy from a byte offset back
};
// copies, when given, receives the number of commands that depend on the spelling the stream uses (the
// copies of command 4, for Metroid the commands 5 and 6): only the right spelling decodes the streams
// that have them
auto DecodeLz2(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used,
               Lz2Variant variant = Lz2Variant::Little, size_t *copies = nullptr) -> bool;
// The LZSS of Final Fantasy VI (found by reading the compressed blocks of the game, not from any document): a 16 bits
// size, then flag bytes read from the lowest bit (1: a literal byte, 0: a reference of two bytes) over a ring buffer of
// 2048 bytes that starts empty, written from position 0x7DE; a reference is the position in the ring (the first byte
// and the low 3 bits of the second) and a length of (the 5 high bits of the second) + 3. The data ends with the size.
auto DecodeLzss(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used,
                bool headerCountsItself) -> bool;
// The LZ of Chrono Trigger (the public description of the format at the NESdev wiki, checked against the game's tables):
// a 16 bits offset of the end of the first packet (from the start of the stream), the data, and at each packet end a
// control byte (low 6 bits: how many flag bits the next mask has, 0 ends the stream; the first one also tells the width
// of the offsets, 12 bits under 0xC0, else 11) followed by the offset of the end of the next packet. Masks are read from
// the lowest bit: 0 a literal byte, 1 two bytes with the distance (low bits) and the length - 3 (high bits). A mask of 0
// with 8 bits copies 8 literals. The stream takes up to the control byte that ends it.
auto DecodeLzPackets(const uint8_t *src, size_t avail, size_t maxOut, std::vector<uint8_t> &out, size_t &used) -> bool;
auto Decode(RomCodec codec, const uint8_t *src, size_t avail, std::vector<uint8_t> &out, size_t &used) -> bool;

// The Super Nintendo streams of a ROM (hiRom: the addresses of the tables use the HiROM map)
auto ScanSnes(const uint8_t *rom, size_t size, bool hiRom) -> std::vector<RomStream>;

// The streams found in a ROM (word aligned, not overlapping, in order of offset)
auto ScanMegaDrive(const uint8_t *rom, size_t size) -> std::vector<RomStream>;
} // namespace Dud
