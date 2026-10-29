// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// LZMA decoder (the "LZMA alone" format: 5 bytes of properties, 8 bytes of unpacked size, then
// the stream). C++ port of drv_lzma.pas; every access is bounds checked. The output buffer is
// the dictionary.
#pragma once

#include <cstdint>

namespace Dud
{
constexpr int LzmaOk = 0;
constexpr int LzmaErrData = -1;   // corrupted data
constexpr int LzmaErrInput = -2;  // truncated input
constexpr int LzmaErrProps = -3;  // invalid properties
constexpr int LzmaErrOutput = -4; // does not fit in the output

// Unpacks an "LZMA alone" stream (13 bytes header). dstLength is the expected size of the
// unpacked data (the one of the header is ignored, it can be unknown).
auto LzmaAloneDecode(const uint8_t *src, int srcLength, uint8_t *dst, int64_t dstLength) -> int;

// Size announced in the header of an "LZMA alone" stream (-1 if unknown or invalid)
auto LzmaAloneSize(const uint8_t *src, int srcLength) -> int64_t;
} // namespace Dud
