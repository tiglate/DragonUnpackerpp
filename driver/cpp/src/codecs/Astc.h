// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ASTC (Adaptive Scalable Texture Compression) decoder: the 2D block sizes from 4x4 to 12x12, LDR and
// HDR (the HDR colors are converted to 8 bits). Each block gives its pixels as BGRA. C++ port of
// drv_astc.pas, itself ported from the decoder of the texture2ddecoder package (MIT; derived from the
// ASTC format specification), block by block with the same rounding. A block that is not valid
// (reserved block mode, endpoints or weights that do not fit in the 128 bits...) gives the magenta
// "error color" of the specification.
#pragma once

#include <cstdint>

namespace Dud
{
// The block of bw x bh pixels at src (16 bytes) into px (bw * bh pixels BGRA, row by row);
// false when the block is not valid (px is then filled with the error color)
auto AstcBlock(const uint8_t *src, int bw, int bh, uint32_t *px) -> bool;
} // namespace Dud
