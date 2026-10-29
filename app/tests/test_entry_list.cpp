// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/EntryListExport.h"
#include "ThirdParty/doctest.h"

#include <sstream>

using namespace Dup::Core;

TEST_CASE("The list of entries as CSV, JSON and text")
{
    const auto index = ArchiveIndex::FromNames({"a/b.txt", "we,ird \"name\".bin", "tab\there"}, "/");
    const std::vector<std::uint32_t> all{0, 1, 2};

    std::ostringstream csv;
    WriteEntryList(index, all, ListFormat::CSV, csv);
    CHECK(csv.str() == "name,size,offset\na/b.txt,0,0\n\"we,ird \"\"name\"\".bin\",0,0\ntab\there,0,0\n");

    std::ostringstream json;
    WriteEntryList(index, all, ListFormat::JSON, json);
    CHECK(json.str() ==
          "[\n  {\"name\": \"a/b.txt\", \"size\": 0, \"offset\": 0},\n  {\"name\": \"we,ird \\\"name\\\".bin\", \"size\": 0, \"offset\": 0},\n"
          "  {\"name\": \"tab\\there\", \"size\": 0, \"offset\": 0}\n]\n");

    std::ostringstream text;
    const std::vector<std::uint32_t> some{2, 0};
    WriteEntryList(index, some, ListFormat::TEXT, text);
    CHECK(text.str() == "tab\there\na/b.txt\n");

    std::ostringstream empty;
    WriteEntryList(index, {}, ListFormat::JSON, empty);
    CHECK(empty.str() == "[]\n");

    CHECK(ListFormatForFile("x.CSV") == ListFormat::CSV);
    CHECK(ListFormatForFile("x.json") == ListFormat::JSON);
    CHECK(ListFormatForFile("x.txt") == ListFormat::TEXT);
    CHECK(ListFormatForFile("noext") == ListFormat::TEXT);
}
