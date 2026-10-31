// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// PKWARE Data Compression Library "implode" streams (the DCL format, also called blast): a byte for the literal
// coding (0 raw, 1 Huffman coded text), a byte for the dictionary size (4 to 6 bits, 1 to 4 KB), then an LSB first
// bit stream of literals and length/distance pairs ending with a special length code. Written from the public
// description of the format (code length tables stored in the nibble pair form of the format itself).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Dud
{
// Unpacks one stream. The output is limited to maxSize bytes (a stream that wants more fails, unless prefix is set:
// the decoding then stops with success once maxSize bytes are there, to read the start of a big stream). Returns
// false for malformed or truncated data (out then holds what was decoded).
auto DclExplode(const uint8_t *in, size_t inSize, size_t maxSize, std::vector<uint8_t> &out, bool prefix = false)
    -> bool;
} // namespace Dud
