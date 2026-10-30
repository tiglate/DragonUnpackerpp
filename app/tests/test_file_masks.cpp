// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/FileMasks.h"

#include "ThirdParty/doctest.h"

using namespace Dup::Core;
using Masks = std::vector<std::string>;

TEST_CASE("each upper case mask is followed by its lower case form")
{
    CHECK(ExpandMasks("*.SFC;*.SMC") == Masks{"*.SFC", "*.sfc", "*.SMC", "*.smc"});
    CHECK(ExpandMasks("*.PAK") == Masks{"*.PAK", "*.pak"});
}

TEST_CASE("masks without letters or already in lower case are not repeated")
{
    CHECK(ExpandMasks("*.7z") == Masks{"*.7z"});
    CHECK(ExpandMasks("*") == Masks{"*"});
    CHECK(ExpandMasks("*.pak;*.PAK") == Masks{"*.pak", "*.PAK"});
}

TEST_CASE("empty parts are skipped")
{
    CHECK(ExpandMasks("").empty());
    CHECK(ExpandMasks(";*.NES;").size() == 2);
}
