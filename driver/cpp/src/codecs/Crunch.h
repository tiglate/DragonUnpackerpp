// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Decoder of the "Unity crunch" textures (DXT1 and DXT5 only): the crunch (.crn) compression of
// Binomial LLC's crnlib, in the variant of Unity's fork, whose decoder gives the DXT blocks directly.
// C++ port of drv_crunch.pas, which was ported from the decompressor of that fork (crn_decomp.h, zlib
// license, Copyright (c) 2010-2016 Richard Geldreich, Jr. and Binomial LLC; Unity's changes by Unity
// Technologies), as read in the texture2ddecoder package (MIT). Layout: a big endian header (signature
// "Hx", sizes, the width, height, number of levels, format, and for four palettes - color endpoints,
// color selectors, alpha endpoints, alpha selectors - the offset, size and number of entries; the
// position and size of the Huffman tables; the offset of each mip level); the palettes and the levels
// are streams of prefix codes (canonical codes, most significant bit first) whose tables are sent in
// the stream itself. A level is the list of the blocks of 2x2 block groups: a code gives for every block
// how its endpoint is chosen (new: delta from the last one, same as the last, same as the block above),
// then the selector index. Bounds checked: bad data gives false, never an exception. Not ported: the ETC
// variants (no sample to check them), DXT3, DXN, DXT5A.
#pragma once

#include <cstdint>

namespace Dud
{
struct CrunchHeaderInfo
{
    int Format = 0; // 0 = DXT1, 2 and 3-6 = DXT5 (the others are not supported)
    int Width = 0, Height = 0;
    int Levels = 0, Faces = 0;
};

// The header of a crunch file (false if it is not one, or not one of DXT1/DXT5). avail bytes can be
// read at src (at least the header); the file has len bytes
auto CrunchInfo(const uint8_t *src, int64_t avail, int64_t len, CrunchHeaderInfo &info) -> bool;
// Number of bytes of the blocks of a level
auto CrunchLevelSize(const CrunchHeaderInfo &info, int level) -> int64_t;
// The blocks of the level (rows of blocks, tightly packed) in dst
auto CrunchUnpack(const uint8_t *src, int64_t len, int level, uint8_t *dst, int64_t dstLen) -> bool;
} // namespace Dud
