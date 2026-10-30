// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// The formats of the driver register their extensions as masks in capitals ("*.SFC;*.SMC"). The file dialogs of
// Linux compare the masks of a filter with the case of the file name, so game.sfc does not match *.SFC and the
// "All supported formats" list of the dialog came out empty for files in lower case.

#include <string>
#include <string_view>
#include <vector>

namespace Dup::Core
{
/// The masks of a ";" separated list ("*.PAK;*.WAD"), each followed by its lower case form ("*.pak"), without
/// duplicates and in order. Only the text after the last "." is changed by the lower case form (masks that
/// contain no letters are not repeated).
[[nodiscard]] auto ExpandMasks(std::string_view masks) -> std::vector<std::string>;
} // namespace Dup::Core
