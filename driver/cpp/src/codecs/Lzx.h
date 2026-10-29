// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// LZX decompressor as used by the XNA content files (.xnb): a 64 KB window, the data is cut in
// chunks of at most 32768 bytes of output, each preceded by its sizes. C++ port of drv_lzx.pas,
// bounds checked: bad data gives false, never an exception.
#pragma once

#include <cstdint>

namespace Dud
{
// src: the chunks (srcLength bytes), gives exactly dstLength bytes in dst
auto LzxDecodeXna(const uint8_t *src, int64_t srcLength, uint8_t *dst, int64_t dstLength) -> bool;
} // namespace Dud
