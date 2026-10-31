// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// SPC files (the SPC700 sound file format: the RAM and the registers of the SNES sound chip, which any SPC player plays)
// rebuilt from a ROM. A game uploads its sound engine, its samples and one bank of songs to the chip; the ROM keeps those
// blocks as they are, so a file is the blocks at their addresses, the state the engine has once it started (a few hundred
// bytes of variables, the DSP registers) and, in the ports the CPU writes, the number of the song to play.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "codecs/RomGfx.h"
#include "codecs/RomTables.h"

namespace Dud
{
constexpr int64_t SpcFileBytes = 0x10200;

auto MakeSpc(const KnownMusic &music, size_t bank, size_t song, const RomSource &rom, const std::string &title, const std::string &game)
    -> std::vector<uint8_t>;
} // namespace Dud
