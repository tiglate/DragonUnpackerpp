// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Declares the Register<Name> function of every format listed in FormatList.inc.
#pragma once

namespace Dud
{
#define DUD_FORMAT(Name) void Register##Name();
#include "FormatList.inc"
#undef DUD_FORMAT
} // namespace Dud
