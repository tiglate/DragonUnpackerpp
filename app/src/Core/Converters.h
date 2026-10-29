// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Dup::Core
{
struct ConvertedFile
{
    std::string Extension; // without the dot
    std::vector<std::byte> Data;
};

/// What "extract with conversion" does to an entry: TGA and DDS images (every format the preview decodes) become PNG files.
/// Returns nullopt for entries that are not converted (other types, or images that cannot be decoded): they are extracted as they are.
[[nodiscard]] auto ConvertForExtraction(const std::string &fileName, const std::string &formatId, std::span<const std::byte> bytes) -> std::optional<ConvertedFile>;

/// Is this entry one that ConvertForExtraction can convert (by its extension)?
[[nodiscard]] auto IsConvertible(const std::string &fileName) -> bool;
} // namespace Dup::Core
