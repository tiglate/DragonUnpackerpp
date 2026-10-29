// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// LZO1X decompressor (the format of the data of Battlefield 1942 .RFA files and others).
// Safe version: every read and write is bounds checked. C++ port of drv_lzo.pas.
#pragma once

#include <cstdint>

namespace Dud
{
constexpr int LzoOk = 0;
constexpr int LzoErrInput = -1;      // reads beyond the input (truncated data)
constexpr int LzoErrOutput = -2;     // writes beyond the output
constexpr int LzoErrLookBehind = -3; // reference before the start of the output
constexpr int LzoErrSize = -4;       // end marker found but the output has not the expected size

// Unpack src (srcLength bytes) into dst, which must be exactly dstLength bytes once done.
auto Lzo1xDecompress(const uint8_t *src, int srcLength, uint8_t *dst, int dstLength) -> int;
} // namespace Dud
