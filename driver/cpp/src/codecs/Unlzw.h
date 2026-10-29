// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The LZW variant of the Vietcong game (COM_LZW_Decompress, compatible with gunzip's), after unlzw.c by Luigi
// Auriemma (2008-07-07), through the Delphi translation of Dragon UnPACKer 5 (lib_unlzw.pas). The original code
// was under the GPL, so this translation is probably also to be considered under GPL.
#pragma once

#include <cstddef>
#include <cstdint>

namespace Dud
{
// Unpacks the input into the output buffer (zero filled by the caller; its size is the expected size); returns
// the number of bytes written. Bad data gives a short count, never an error.
auto UnlzwDecode(const uint8_t *in, size_t inSize, uint8_t *out, size_t outSize) -> size_t;
} // namespace Dud
