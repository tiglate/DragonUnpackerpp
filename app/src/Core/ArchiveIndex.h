// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Dudi7/Driver.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Dup::Core
{
/// The entries of an archive and their folder tree, built once. The entries are kept in flat arrays
/// (no object per entry): a million entries cost a few tens of megabytes.
class ArchiveIndex
{
  public:
    static constexpr std::uint32_t ROOT = 0;

    struct Folder
    {
        std::string Name;
        std::uint32_t Parent{0};
        std::vector<std::uint32_t> Children;
        std::vector<std::uint32_t> Entries; // indexes of the files directly in this folder
        std::uint32_t FileCount{0};         // files in this folder and everything below it
    };

    /// Reads every entry of the archive (one driver call per entry).
    static auto Build(const Dudi7::IArchive &archive) -> ArchiveIndex;
    /// For tests and tools: an index of plain names (no size, no offset). dirSeparator as in the archive info.
    static auto FromNames(const std::vector<std::string> &names, std::string_view dirSeparator) -> ArchiveIndex;

    [[nodiscard]] auto GetEntryCount() const -> std::uint32_t
    {
        return static_cast<std::uint32_t>(_sizes.size());
    }
    [[nodiscard]] auto GetDirSeparator() const -> const std::string &
    {
        return _dirSeparator;
    }
    /// Complete name as stored in the archive.
    [[nodiscard]] auto GetPath(std::uint32_t entry) const -> std::string_view;
    /// Last component of the name.
    [[nodiscard]] auto GetFileName(std::uint32_t entry) const -> std::string_view;
    [[nodiscard]] auto GetSize(std::uint32_t entry) const -> std::int64_t
    {
        return _sizes[entry];
    }
    [[nodiscard]] auto GetOffset(std::uint32_t entry) const -> std::int64_t
    {
        return _offsets[entry];
    }
    /// The folder that holds the entry.
    [[nodiscard]] auto GetFolderOf(std::uint32_t entry) const -> std::uint32_t
    {
        return _entryFolder[entry];
    }
    [[nodiscard]] auto GetFolders() const -> const std::vector<Folder> &
    {
        return _folders;
    }
    /// Entries of the folder and of everything below it, in archive order.
    [[nodiscard]] auto GetEntriesRecursive(std::uint32_t folder) const -> std::vector<std::uint32_t>;

  private:
    void AddEntry(std::string_view name, std::int64_t size, std::int64_t offset);
    void BuildFolders();

    std::string _dirSeparator;
    std::string _pool;                     // every name, one after the other
    std::vector<std::uint32_t> _nameStart; // start of each name in the pool, with a final end marker
    std::vector<std::uint32_t> _fileStart; // start of the last component of each name
    std::vector<std::int64_t> _sizes;
    std::vector<std::int64_t> _offsets;
    std::vector<std::uint32_t> _entryFolder;
    std::vector<Folder> _folders;
};
} // namespace Dup::Core
