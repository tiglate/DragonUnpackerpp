// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Decoders of the block compressed texture formats of mobile GPUs: ETC1, ETC2 (RGB, RGBA1, RGBA8) and
// EAC (one or two channels, unsigned or signed), with ASTC (Astc.h) and the DXT1/DXT5 blocks the sprites
// need. Each 4x4 block gives 16 pixels packed as BGRA (blue in the lowest byte, so that they are written
// as they are in a 32 bits TGA). C++ port of drv_texdecode.pas, written from the specifications (Khronos
// Data Format / OpenGL ES 3.0 for ETC2 and EAC); the rounding and the handling of invalid blocks follow
// the texture2ddecoder package (MIT).
#pragma once

#include <array>
#include <cstdint>

namespace Dud
{
using BlockPixels = std::array<uint32_t, 144>; // row by row (4x4 up to 12x12 pixels)

// What a Unity texture format is made of: block size in pixels, bytes per block (false: not handled)
auto BlockFormatInfo(int unityFmt, int &bw, int &bh, int &blockBytes) -> bool;
// The same for every format that DecodeBlock knows: also DXT1 (10) and DXT5 (12), which the archive exports as DDS
// but that the sprites need as pixels
auto DecodableFormatInfo(int unityFmt, int &bw, int &bh, int &blockBytes) -> bool;
// Decodes one block of the Unity texture format
void DecodeBlock(int unityFmt, const uint8_t *src, BlockPixels &px);
// False for a block that is not valid (ASTC: it was decoded as the magenta error color)
auto DecodeBlockOk(int unityFmt, const uint8_t *src, BlockPixels &px) -> bool;
// Decodes the row by of blocks of an image of w x h pixels (src: the blocks of that row) into dst, as
// 32 bits BGRA pixels, w pixels per line, as many lines as the row has (up to the block height)
void DecodeBlockRow(int unityFmt, const uint8_t *src, int w, int h, int by, uint8_t *dst);
} // namespace Dud
