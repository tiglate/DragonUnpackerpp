// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ArchiveIndex.h"

#include <iosfwd>
#include <span>

namespace Dup::Core
{
enum class ListFormat
{
    CSV,  // name,size,offset with a header line; fields with commas or quotes are quoted
    JSON, // an array of {"name", "size", "offset"}
    TEXT  // one name per line
};

/// "Create list of entries" of v5: writes the entries (numbers in the index) to the stream, in the given order.
void WriteEntryList(const ArchiveIndex &index, std::span<const std::uint32_t> entries, ListFormat format, std::ostream &out);

/// The format for a file name by its extension (.csv, .json, else text).
[[nodiscard]] auto ListFormatForFile(const std::string &fileName) -> ListFormat;
} // namespace Dup::Core
