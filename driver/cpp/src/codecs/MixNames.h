// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Names of the files of the Westwood MIX archives (Command & Conquer, Red Alert, Tiberian Sun), which only
// store a hash of the name: the list of Dragon UnPACKer 5 (about 21,000 names) and the two hashes.

#pragma once

#include <cstdint>
#include <string>

namespace Dud
{
// Hash of the name in Command & Conquer
auto MixIdCommandAndConquer(const std::string &name) -> uint32_t;
// Hash of the name in Red Alert and Tiberian Sun (CRC32 of the padded name)
auto MixIdRedAlert(const std::string &name) -> uint32_t;
// The known name of a hash (the last one of the list if several share it) or an empty string
auto MixNameCommandAndConquer(uint32_t id) -> std::string;
auto MixNameRedAlert(uint32_t id) -> std::string;
} // namespace Dud
