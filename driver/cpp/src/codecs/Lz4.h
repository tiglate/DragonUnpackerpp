// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// LZ4 block decompressor (the raw block format, no frame), bounds checked. C++ port of drv_lz4.pas.
#pragma once

#include <cstdint>

namespace Dud
{
constexpr int Lz4ErrInput = -1;  // truncated input
constexpr int Lz4ErrOutput = -2; // writes beyond the output
constexpr int Lz4ErrOffset = -3; // reference before the start of the output

// Unpacks one block; returns the number of bytes written (<= dstLength) or a negative error.
auto Lz4DecompressBlock(const uint8_t *src, int srcLength, uint8_t *dst, int dstLength) -> int;
} // namespace Dud
