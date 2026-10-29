// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The Vorbis setup packets that FMOD FSB5 files refer to by a CRC32 (port of drv_fsb_codebooks.pas).
#pragma once

#include <cstdint>
#include <string>

namespace Dud
{
// The setup packet (starting with 05 "vorbis") for the CRC32, "" when it is not known
auto FsbSetupPacket(uint32_t crc) -> std::string;
} // namespace Dud
