// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/HyperRipper.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Dup::Core
{
/// Naming of the files found by the HyperRipper (the "prefix" of v5).
struct NamingOptions
{
    /// Replaced in the template: %f name of the scanned file without its extension, %x its extension, %o offset in decimal,
    /// %h offset in hexadecimal (16 digits), %s size in decimal, %l size in hexadecimal (16 digits), %n number of the find
    /// (10 digits). The characters that file names cannot hold are replaced by '_'. The extension of the find is added.
    std::string Template{DEFAULT_RIP_TEMPLATE};
    /// Put the finds in the folders Audio, Video, Image and Unknown.
    bool MakeDirs{false};

    static constexpr const char *DEFAULT_RIP_TEMPLATE = "%f_%x-%n";
};

enum class RipKind
{
    Unknown,
    Audio,
    Video,
    Image
};

/// What a find is, from its extension.
[[nodiscard]] auto GetRipKind(const std::string &extension) -> RipKind;

/// The names of the finds, in the same order, with "\\" between the folders.
[[nodiscard]] auto MakeRipNames(const std::filesystem::path &scanned, const std::vector<RippedFile> &files, const NamingOptions &options) -> std::vector<std::string>;
} // namespace Dup::Core
