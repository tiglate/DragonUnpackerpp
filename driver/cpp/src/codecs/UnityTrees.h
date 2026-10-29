// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Type trees of the objects the Unity exporter reads, taken from real files; they stand in for the trees
// stripped from the serialized files of release players (the layout must consume the whole object).
#pragma once

namespace Dud
{
constexpr int TreeCount = 16;
// Class ID of the template i
auto TreeClass(int i) -> int;
// Its nodes as 'level;type;name;size;flags,...'
auto TreeText(int i) -> const char *;
} // namespace Dud
