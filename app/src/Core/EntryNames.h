// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// Entry names come from files we do not control: they can contain "..", absolute paths, drive letters, control
// characters, or be duplicated (the lumps of a Doom WAD are not unique). Everything written to the disk goes through
// this header:
//  - SplitEntryName splits a name in components using the separator of the archive,
//  - SafeRelativePath turns it into a relative path that cannot escape from the destination directory,
//  - NameAllocator makes the paths unique (THINGS, THINGS~2, ...) and solves the clashes between a file and a
//    directory of the same name.
// Port of the Pascal unit core/entrynames.pas (same rules, same test cases).

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Dup::Core
{
/// Splits a name with the separator of the archive: "/" , "\\" (a "/" is a separator too) or "" for flat archives.
[[nodiscard]] auto SplitEntryName(std::string_view name, std::string_view dirSeparator) -> std::vector<std::string>;

/// A name usable as a file name on every platform ("" when nothing can be kept).
[[nodiscard]] auto SafeComponent(std::string_view component) -> std::string;

/// A relative path (preferred separator) for an entry: never empty, never absolute, no ".." component. entryIndex names
/// the entries that have no usable name ("entry_<n>").
[[nodiscard]] auto SafeRelativePath(std::string_view name, std::string_view dirSeparator, std::int64_t entryIndex) -> std::string;

/// Case insensitive comparison where digit sequences compare as numbers (file2 < file10): <0, 0 or >0.
[[nodiscard]] auto NaturalCompare(std::string_view a, std::string_view b) -> int;

/// Is the path (after normalisation) strictly inside the directory root?
[[nodiscard]] auto IsInsideDir(const std::filesystem::path &root, const std::filesystem::path &path) -> bool;

/// Gives every path of an extraction a unique name; names are compared without case (the destination can be case
/// insensitive).
class NameAllocator
{
  public:
    /// The path to use for relativePath (a SafeRelativePath result).
    [[nodiscard]] auto Allocate(const std::string &relativePath) -> std::string;

  private:
    std::set<std::string> _files;
    std::set<std::string> _directories;
};
} // namespace Dup::Core
